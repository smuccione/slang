/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/

import * as vscode from 'vscode';
import { activateslangDebug } from '../activateSlangDebug';

export function activate(context: vscode.ExtensionContext) {
	activateslangDebug(context);
}

export function deactivate() {
	// nothing to do
}
