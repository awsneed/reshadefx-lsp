#include "effect_codegen.hpp"
#include "effect_parser.hpp"
#include "effect_preprocessor.hpp"
#include "lsp/connection.h"
#include "lsp/error.h"
#include "lsp/io/standardio.h"
#include "lsp/messagehandler.h"
#include "lsp/messages.h"
#include <iostream>
#include <map>

int main(int argc, char *argv[])
{
    /*
     ** ReShade setup **********************************************************
     */
    reshadefx::preprocessor preprocessor;
    // TODO: Make these configurable
	preprocessor.add_macro_definition("__RESHADE__", "99999");
	preprocessor.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", "0");
	preprocessor.add_macro_definition("BUFFER_WIDTH", "800");
	preprocessor.add_macro_definition("BUFFER_HEIGHT", "600");
	preprocessor.add_macro_definition("BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)");
	preprocessor.add_macro_definition("BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)");
    
    
    // TODO: Make use of these later
    std::unique_ptr<reshadefx::codegen> backend;
    backend.reset(reshadefx::create_codegen_spirv(false, false, false));
    reshadefx::parser parser;

    /*
     ** LSP Setup **************************************************************
     */
    auto connection = lsp::Connection(lsp::io::standardIO());
    auto msg_handler = lsp::MessageHandler(connection);
    
    // Initialize handler
    msg_handler.add<lsp::requests::Initialize>(
        [](lsp::requests::Initialize::Params&& params)
        {
            return lsp::requests::Initialize::Result{
                .capabilities = {
                    .positionEncoding = lsp::PositionEncodingKind::UTF16,
                },
                .serverInfo = lsp::InitializeResultServerInfo{
                    .name = "ReShade FX Language Server",
                    .version = "0.0.1"
                }
            };
        }
    );
    
    auto open_files = new std::unordered_map<lsp::DocumentUri, lsp::TextDocumentItem>;

    // Document open
    msg_handler.add<lsp::notifications::TextDocument_DidOpen>(
        [&open_files](lsp::DidOpenTextDocumentParams&& params)
        {
            lsp::Uri& uri = params.textDocument.uri;
            if (open_files->contains(uri)) {
                std::cerr << "URI " << uri.toString() << " already open!" << std::endl;
                return;
            }

            open_files->insert({uri, params.textDocument});
        }
    );
    
    // Document change
    msg_handler.add<lsp::notifications::TextDocument_DidChange>(
        [&open_files](lsp::DidChangeTextDocumentParams&& params)
        {
            lsp::Uri& uri = params.textDocument.uri;
            if (!open_files->contains(uri)) {
                std::cerr << "Tried to change unopened URI " << uri.toString() << std::endl;
                return;
            }
            
            lsp::TextDocumentItem& doc = open_files->at(uri);
        }
    );
    
    // Document close
    msg_handler.add<lsp::notifications::TextDocument_DidClose>(
        [&open_files](lsp::DidCloseTextDocumentParams&& params)
        {
            lsp::Uri& uri = params.textDocument.uri;
            if (!open_files->contains(uri)) {
                std::cerr << "URI " << uri.toString() << " wasn't open!" << std::endl;
                return;
            }
            
            open_files->erase(uri);
        }
    );
    
    // Validate document
    msg_handler.add<lsp::notifications::TextDocument_DidChange>(
        [](lsp::DidChangeTextDocumentParams&& params)
        {
        }
    );

    // Shutdown handler
    bool shutdown = false;
    msg_handler.add<lsp::requests::Shutdown>(
        [&shutdown]()
        {
            // TODO: Make this cleanly shut down (if needed?)
            shutdown = true;

            // TODO: Do we need to do more here?
            return lsp::ShutdownResult();
        }
    );
    
    while (!shutdown)
        msg_handler.processIncomingMessages();

    return 0;
}
