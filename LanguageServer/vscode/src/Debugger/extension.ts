/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/

'use strict';

import * as vscode from 'vscode';
import { activateslangDebug } from './activateSlangDebug';

/*
 * The compile time flag 'runMode' controls how the debug adapter is run.
 * Please note: the test suite only supports 'external' mode.
 */
export function activate(context: vscode.ExtensionContext) {
	activateslangDebug(context, new SlangDebugAdapterServerDescriptorFactory());
}

export function deactivate() {
	// nothing to do
}

class SlangDebugAdapterServerDescriptorFactory implements vscode.DebugAdapterDescriptorFactory {
	createDebugAdapterDescriptor(session: vscode.DebugSession, executable: vscode.DebugAdapterExecutable | undefined): vscode.ProviderResult<vscode.DebugAdapterDescriptor> {
		return new vscode.DebugAdapterServer( 9772 );
	}
}
