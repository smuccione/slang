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

export function activate(context: vscode.ExtensionContext) {
	context.subscriptions.push(vscode.commands.registerCommand('extension.Slang-debug.getProgramName', config => {
		return vscode.window.showInputBox({
			placeHolder: "Please enter the name of a file in the workspace folder",
			value: "page.ap"
		});
	}));

	DebuggerExtension.activate(context);

	const clientOptions: LanguageClientOptions = {
		// Register the server for plain text documents
		documentSelector: [
			{ scheme: 'file', language: 'slang' },
			{ scheme: 'file', language: 'fgl' },
			{ scheme: 'file', language: 'ap' }
		],
		synchronize: {
			fileEvents: vscode.workspace.createFileSystemWatcher('**/*.*'),
			configurationSection : ['slang']
		}
	};

	let serverOptions: any = null;

	if ( 0 ) {
		const connectionInfo = {
			port: 6996,
			host: "127.0.0.1"
		};

		serverOptions = () => {
			// Connect to language server via socket
			const socket = net.connect(connectionInfo);
			const result: StreamInfo = {
				writer: socket,
				reader: socket
			};
			return Promise.resolve(result);
		};
	} else {
		const serverModule = context.asAbsolutePath(path.join('./dist', 'slangd.exe'));

		serverOptions = {
			command: serverModule,
			args: ["-c"],
		};
	}
	// Options to control the language client
	// Create the language client and start the client.
	const client = new LanguageClient(
		'slang',
		'Slang Language Server',
		serverOptions,
		clientOptions
	);

	// Start the client. This will also launch the server
	client.start();
}
