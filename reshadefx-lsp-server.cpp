#include "effect_codegen.hpp"
#include "effect_parser.hpp"
#include "effect_preprocessor.hpp"
#include "lsp/connection.h"
#include "lsp/fileuri.h"
#include "lsp/io/socket.h"
#include "lsp/io/standardio.h"
#include "lsp/messagehandler.h"
#include "lsp/messages.h"
#include "lsp/types.h"
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

auto load_rfx_pp_macros(
    reshadefx::preprocessor &rfx_pp,
    const std::map<std::string, std::string> &rfx_pp_macros) {
  auto itr = rfx_pp_macros.cbegin();

  while (itr != rfx_pp_macros.cend()) {
    rfx_pp.add_macro_definition(itr->first, itr->second);
    ++itr;
  }
}

} // unnamed namespace

auto main(int argc, char *argv[]) -> int {
  /*
   * Initial Setup
   */

  enum io_mode : std::uint8_t {
    UNKNOWN,
    stdio,
    socket,
    pipe,
    ipc,
  };

  // Convert arguments to a vector of strings for ease of processing
  const auto args{std::vector<std::string>{argv + 1, argv + argc}};

  auto io_mode{io_mode::UNKNOWN};
  auto pipe_path{std::filesystem::path{}};
  auto port{std::optional<unsigned short>{}};
  auto client_pid{std::optional<pid_t>{}};

  // Process args
  for (auto begin{args.cbegin()}, end{args.cend()}; begin != end; ++begin) {
    auto arg{*begin};
    static constexpr auto prefix{std::string{"--"}};

    if (arg.starts_with(prefix)) {
      arg = arg.substr(prefix.length());

      if (static constexpr auto match{std::string{"stdio"}}; arg == match) {
        io_mode = stdio;

      } else if (static constexpr auto match{std::string{"pipe"}};
                 arg.starts_with(match)) {
        arg = arg.substr(match.length());
        io_mode = pipe;

        if (arg.length() == 0) {
          // Pipe path is the next arg
          pipe_path = *(++begin);

        } else if (static constexpr std::string equ{"="};
                   arg.starts_with(equ)) {
          pipe_path = arg.substr(equ.length());

        } else { // Invalid state
          std::cerr << "ERROR: Invalid or no argument given to --pipe|--pipe="
                    << '\n';
          return 1;
        }

      } else if (static constexpr std::string match{"socket"};
                 arg.starts_with(match)) {
        arg = arg.substr(match.length());
        io_mode = socket;

        if (port) {
          continue;
        }

        if ((*(begin + 1)).find_first_not_of("0123456789") ==
            std::string::npos) {
          ++begin;
          port = std::stoi(*begin);
        }

      } else if (static constexpr std::string match{"port="};
                 arg.starts_with(match)) {
        arg = arg.substr(match.length());

        port = std::stoi(arg);

      } else if (static constexpr std::string match{"node-ipc"};
                 arg.starts_with(match)) {
        io_mode = ipc;
        std::cerr << "Server is not running under node!" << '\n';
        return 1;

      } else if (arg == "clientProcessId") {
        client_pid = std::stoi(*(begin + 1));
      }

    } else {
      // TODO: Consider other arguments?
    }

  } // Process args

  if (io_mode == UNKNOWN) {
    std::cerr << "IO mode never specified!" << '\n';
    return 1;
  }

  /*
   * ReShade Setup
   */

  // Global preprocessor macros
  // TODO: Make these configurable via settings / flags(?)
  const std::map<std::string, std::string> builtin_rfx_pp_macros{
      {"__RESHADE__", "99999"},
      {"__RESHADE_PERFORMANCE_MODE__", "0"},
      {"BUFFER_WIDTH", "1920"},
      {"BUFFER_HEIGHT", "1080"},
      {"BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)"},
      {"BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)"},
  };

  // Global preprocessor that we might need
  // auto global_rfx_pp = reshadefx::preprocessor();

  // load_rfx_pp_macros(global_rfx_pp, builtin_rfx_pp_macros);

  // Global backend that we might need
  // auto global_rfx_backend = unique_ptr<reshadefx::codegen>(
  // reshadefx::create_codegen_spirv(false, false, false));

  /*
   * LSP Setup
   */
  auto connection{std::unique_ptr<lsp::Connection>{}};
  auto listener{std::unique_ptr<lsp::io::SocketListener>{}};

  switch (io_mode) {
  case pipe:
    // TODO: Finish pipe implementation
    std::cerr << "ERROR: Pipe implementation is incomplete!" << '\n';
    break;

  case socket:
    listener = std::make_unique<lsp::io::SocketListener>(*port);
    // TODO: Finish socket implementation
    std::cerr << "ERROR: Socket implementation is incomplete!" << '\n';
    return 1;

  case ipc:
    // TODO: Add more blocking of ipc, or possibly implement somehow?
    std::cerr << "ERROR: ipc communication not supported!" << '\n';
    return 1;

  case stdio:
    [[fallthrough]];
  default:
    connection = std::make_unique<lsp::Connection>(lsp::io::standardIO());
    break;
  }

  /*
   * TODO: Finish socket implementation with concurrency / multi-threading
   */

  auto msg_handler{lsp::MessageHandler{*connection}};

  // Initialize handler
  msg_handler.add<lsp::requests::Initialize>(
      [](lsp::requests::Initialize::Params &&params) {
        return lsp::requests::Initialize::Result{
            .capabilities =
                {
                    .positionEncoding = lsp::PositionEncodingKind::UTF16,
                    .textDocumentSync =
                        lsp::TextDocumentSyncOptions{
                            // TODO: Implement incremental sync
                            .openClose = true,
                            .change = lsp::TextDocumentSyncKind::Full,
                        },
                    .diagnosticProvider =
                        lsp::DiagnosticOptions{

                        },
                },
            .serverInfo =
                lsp::InitializeResultServerInfo{
                    .name = "ReShadeFX LSP Server",
                    .version = "0.0.1",
                },
        };
      });

  auto open_files{std::unordered_map<std::filesystem::path, std::string>{}};

  // Document open
  msg_handler.add<lsp::notifications::TextDocument_DidOpen>(
      [&open_files](lsp::DidOpenTextDocumentParams &&params) {
        auto doc_path{std::filesystem::path{params.textDocument.uri.path()}};

        if (open_files.contains(doc_path)) {
          std::cerr << "URI " << doc_path << " already open!" << '\n';
          return;
        }

        open_files.insert({doc_path, params.textDocument.text});
      });

  auto validateDocument{[&open_files, &msg_handler](lsp::DocumentUri &uri) {
    int result = 0;
    reshadefx::preprocessor rfx_pp{};
    lsp::notifications::TextDocument_PublishDiagnostics::Params params{
        lsp::notifications::TextDocument_PublishDiagnostics::Params{
            .uri = uri, .diagnostics = std::vector<lsp::Diagnostic>()}};

    if (!rfx_pp.append_string(open_files.at(uri.toString()), uri.toString())) {
      params.diagnostics.push_back(lsp::Diagnostic(
          {.range = lsp::Range({
               .start = lsp::Position({.line = 0, .character = 0}),
               .end = lsp::Position({.line = 0, .character = 0}),
           }),
           .message = rfx_pp.errors(),
           .severity = lsp::DiagnosticSeverity::Error}));

      result = 1;
    }

    auto rfx_parser = reshadefx::parser();
    auto rfx_backend = std::unique_ptr<reshadefx::codegen>(
        reshadefx::create_codegen_spirv(false, false, false));

    if (!rfx_parser.parse(rfx_pp.output(), rfx_backend.get())) {
      params.diagnostics.push_back(lsp::Diagnostic(
          {.range = lsp::Range({
               .start = lsp::Position({.line = 0, .character = 0}),
               .end = lsp::Position({.line = 0, .character = 0}),
           }),
           .message = rfx_parser.errors(),
           .severity = lsp::DiagnosticSeverity::Error}));

      result = 2;
    }

    msg_handler
        .sendNotification<lsp::notifications::TextDocument_PublishDiagnostics>(
            std::move(params));

    return result;
  }};

  // Document change
  msg_handler.add<lsp::notifications::TextDocument_DidChange>(
      [&open_files,
       &validateDocument](lsp::DidChangeTextDocumentParams &&params) {
        lsp::DocumentUri &uri{params.textDocument.uri};

        if (!open_files.contains(uri.toString())) {
          std::cerr << "Tried to change unopened URI " << uri.toString()
                    << '\n';
          return;
        }

        // TODO: Implement incremental
        open_files.at(uri.toString())
            .swap(std::get<lsp::TextDocumentContentChangeEvent_Text>(
                      params.contentChanges.back())
                      .text);

        validateDocument(uri);
      });

  // Document close
  msg_handler.add<lsp::notifications::TextDocument_DidClose>(
      [&open_files](lsp::DidCloseTextDocumentParams &&params) {
        lsp::Uri &uri{params.textDocument.uri};

        if (!open_files.contains(uri.toString())) {
          std::cerr << "URI " << uri.toString() << " wasn't open!" << '\n';
          return;
        }

        open_files.erase(uri.toString());
      });

  // Shutdown handler
  bool shutdown = false;
  msg_handler.add<lsp::requests::Shutdown>([&shutdown]() {
    // TODO: Make this cleanly shut down (if needed?)
    shutdown = true;

    // TODO: Do we need to do more here?
    return lsp::ShutdownResult();
  });

  while (!shutdown) {
    msg_handler.processIncomingMessages();
  }

  return 0;
}
