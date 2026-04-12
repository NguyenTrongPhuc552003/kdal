import * as vscode from "vscode";
import { findToolchain, ToolchainPaths } from "./toolchain";
import {
  compileCommand,
  lintCommand,
  initCommand,
  simulateCommand,
  dtgenCommand,
  testgenCommand,
  installSDKCommand,
  setOutputChannel,
} from "./commands";
import { createStatusBar, showStatusBar, disposeStatusBar, showToolchainInfo } from "./statusBar";
import { KdalTaskProvider } from "./taskProvider";

let diagnosticCollection: vscode.DiagnosticCollection;
let toolchain: ToolchainPaths;

export async function activate(context: vscode.ExtensionContext): Promise<void> {
  // Output channel
  const outputChannel = vscode.window.createOutputChannel("KDAL");
  setOutputChannel(outputChannel);

  // Diagnostics
  diagnosticCollection = vscode.languages.createDiagnosticCollection("kdal");
  context.subscriptions.push(diagnosticCollection);

  // Find toolchain
  toolchain = findToolchain();

  // Status bar
  createStatusBar(toolchain);
  showStatusBar();
  context.subscriptions.push({ dispose: disposeStatusBar });

  // Register commands
  context.subscriptions.push(
    vscode.commands.registerCommand("kdal.compile", () => compileCommand(toolchain)),
    vscode.commands.registerCommand("kdal.lint", () => lintCommand(toolchain, diagnosticCollection)),
    vscode.commands.registerCommand("kdal.init", () => initCommand(toolchain)),
    vscode.commands.registerCommand("kdal.simulate", () => simulateCommand(toolchain)),
    vscode.commands.registerCommand("kdal.dtgen", () => dtgenCommand(toolchain)),
    vscode.commands.registerCommand("kdal.testgen", () => testgenCommand(toolchain)),
    vscode.commands.registerCommand("kdal.showToolchainInfo", () => showToolchainInfo(toolchain)),
    vscode.commands.registerCommand("kdal.installSDK", () => installSDKCommand()),
    vscode.commands.registerCommand("kdal.redetectToolchain", () => {
      toolchain = findToolchain();
      createStatusBar(toolchain);
      showStatusBar();
      diagnosticCollection.clear();
      if (toolchain.kdality) {
        vscode.window.showInformationMessage("KDAL SDK detected successfully.");
      }
    })
  );

  // Task provider
  context.subscriptions.push(
    vscode.tasks.registerTaskProvider(KdalTaskProvider.type, new KdalTaskProvider(toolchain.kdality))
  );

  // Lint on save
  context.subscriptions.push(
    vscode.workspace.onDidSaveTextDocument((doc) => {
      const config = vscode.workspace.getConfiguration("kdal");
      if (!config.get<boolean>("lintOnSave", true)) return;
      if (doc.languageId === "kdh" || doc.languageId === "kdc") {
        lintCommand(toolchain, diagnosticCollection, doc);
      }
    })
  );

  // Clear diagnostics when KDAL documents are closed
  context.subscriptions.push(
    vscode.workspace.onDidCloseTextDocument((doc) => {
      if (doc.languageId === "kdh" || doc.languageId === "kdc") {
        diagnosticCollection.delete(doc.uri);
      }
    })
  );

  // Re-detect toolchain on configuration change
  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration((e) => {
      if (e.affectsConfiguration("kdal.toolchainPath") || e.affectsConfiguration("kdal.stdlibPath")) {
        toolchain = findToolchain();
        createStatusBar(toolchain);
        showStatusBar();
        diagnosticCollection.clear();
      }
    })
  );

  outputChannel.appendLine("[KDAL] Extension activated");
  if (toolchain.kdality) {
    outputChannel.appendLine(`[KDAL] kdality: ${toolchain.kdality}`);
  }
  if (toolchain.kdalc) {
    outputChannel.appendLine(`[KDAL] kdalc: ${toolchain.kdalc}`);
  }

  // Prompt installation when SDK is missing
  if (!toolchain.kdality && !toolchain.kdalc) {
    const choice = await vscode.window.showWarningMessage(
      "KDAL SDK not found. Install it for full language support (compile, lint, simulate).",
      "Install SDK",
      "Configure Path",
      "Dismiss"
    );
    if (choice === "Install SDK") {
      vscode.commands.executeCommand("kdal.installSDK");
    } else if (choice === "Configure Path") {
      vscode.commands.executeCommand("workbench.action.openSettings", "kdal.toolchainPath");
    }
  }
}

export function deactivate(): void {
  // cleanup handled by disposables
}
