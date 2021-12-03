import * as net from 'net';
import * as vscode from 'vscode';
import {
    StreamInfo,	
	LanguageClient,
	LanguageClientOptions,
   // TransportKind,
  } from 'vscode-languageclient/node';
import * as DebuggerExtension from './Debugger/extension';
import * as path from 'path';
//import * as fs from 'fs';

/*
import {   
	RequestType,  
	Range 
} from 'vscode-languageclient';

interface WorkspaceFolderParams {
    workspaceFolderUri?: string;
}


export interface LanguageRange extends Range {  
	languageId: string | undefined;
}

interface SwitchHeaderSourceParams extends WorkspaceFolderParams {
    switchHeaderSourceFileName: string;
}

const SwitchHeaderSourceRequest: RequestType<SwitchHeaderSourceParams, string, void, void> = new RequestType<SwitchHeaderSourceParams, string, void, void>('slang/GetLanguageRanges');

export class DocumentLanguageRangeProvider {
    private client: LanguageClient;
    constructor(client: LanguageClient) {
        this.client = client;

    }

    public requestSwitchHeaderSource(rootPath: string, fileName: string): Thenable<string> {
        const params: SwitchHeaderSourceParams = {
            switchHeaderSourceFileName: fileName,
            workspaceFolderUri: rootPath
        };
        return this.client.sendRequest(SwitchHeaderSourceRequest, params);
    }

    private getLanguageRanges (): LanguageRange[] {
        const documentSymbols: LanguageRange[] = [];
        return documentSymbols;
    }

    public async provideLanguageRanges(document: vscode.TextDocument): Promise<LanguageRange[]> {
        return await this.client.sendRequest(SwitchHeaderSourceRequest, null)(async () => {
            const params: GetDocumentSymbolRequestParams = {
                uri: document.uri.toString()
            };
            const symbols: LocalizeDocumentSymbol[] = await this.client.languageClient.sendRequest(GetDocumentSymbolRequest, params);
            const resultSymbols: vscode.DocumentSymbol[] = this.getChildrenSymbols(symbols);
            return resultSymbols;
        });
    }
}
*/
export function activate(context: vscode.ExtensionContext) {
	context.subscriptions.push(vscode.commands.registerCommand('extension.Slang-debug.getProgramName', config => {
		return vscode.window.showInputBox({
			placeHolder: "Please enter the name of a file in the workspace folder",
			value: "page.ap"
		});
	}));


	const virtualDocumentContents = new Map<string, string>();

	vscode.workspace.registerTextDocumentContentProvider('embedded-content', {
		provideTextDocumentContent: uri => {
			const originalUri = uri.path.slice(1).slice(0, -4);
			const decodedUri = decodeURIComponent(originalUri);
			return virtualDocumentContents.get(decodedUri);
		}
	});

	const clientOptions: LanguageClientOptions = {
		// Register the server for plain text documents
		documentSelector: [
			{ scheme: 'file', language: 'slang' },
			{ scheme: 'file', language: 'fgl' },
			{ scheme: 'file', language: 'ap' }
		],
		synchronize: {
			fileEvents:  vscode.workspace.createFileSystemWatcher('**/*.*')
		}
	};

    DebuggerExtension.activate(context);

    if ( 0 )
    {
        const connectionInfo = {
            port: 6996,
            host: "127.0.0.1"
        };
    
        const  serverOptions = () => {
            // Connect to language server via socket
            const socket = net.connect(connectionInfo);
            const result: StreamInfo  = {
                writer: socket,
                reader: socket
            };
            return Promise.resolve(result);
        };

        // Options to control the language client
        // Create the language client and start the client.
        const client = new LanguageClient(
            'Slang',
            'Slang Language Server',
            serverOptions,
            clientOptions
        );

        // Start the client. This will also launch the server
        client.start();    
    } else
    {
        const serverModule = context.asAbsolutePath(path.join('./dist', 'slangd.exe'));

        const serverOptions  = {
            command: serverModule, 
            args: ["-c"],
        };

        // Options to control the language client
        // Create the language client and start the client.
        const client = new LanguageClient(
            'Slang',
            'Slang Language Server',
            serverOptions,
            clientOptions
        );

        // Start the client. This will also launch the server
        client.start();
    }
}
