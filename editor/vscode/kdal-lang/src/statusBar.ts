import * as vscode from "vscode";
import { ToolchainPaths, runTool } from "./toolchain";

let statusItem: vscode.StatusBarItem;
let fileInfoItem: vscode.StatusBarItem;
let editorChangeDisposable: vscode.Disposable | undefined;

export function createStatusBar(tc: ToolchainPaths): void {
  // Dispose previous instances to avoid leaks
  statusItem?.dispose();
  fileInfoItem?.dispose();
  editorChangeDisposable?.dispose();

  // KDAL toolchain status
  statusItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 50);
  if (tc.kdality) {
    statusItem.text = "$(tools) KDAL";
    statusItem.tooltip = `kdalc: ${tc.kdalc || "not found"}\nkdality: ${tc.kdality}`;
    statusItem.command = "kdal.showToolchainInfo";
  } else {
    statusItem.text = "$(warning) KDAL: No toolchain";
    statusItem.tooltip = "Click to install the KDAL SDK";
    statusItem.command = "kdal.installSDK";
  }

  // File type indicator
  fileInfoItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
  fileInfoItem.hide();

  updateFileInfo(vscode.window.activeTextEditor);

  editorChangeDisposable = vscode.window.onDidChangeActiveTextEditor(updateFileInfo);
}

export function showStatusBar(): void {
  statusItem.show();
}

export function disposeStatusBar(): void {
  statusItem?.dispose();
  fileInfoItem?.dispose();
  editorChangeDisposable?.dispose();
}

export async function showToolchainInfo(tc: ToolchainPaths): Promise<void> {
  const lines: string[] = [];

  if (tc.kdality) {
    const result = await runTool(tc.kdality, ["version"]);
    lines.push(`kdality: ${result.stdout.trim() || "installed"}`);
  } else {
    lines.push("kdality: not found");
  }

  if (tc.kdalc) {
    const result = await runTool(tc.kdalc, ["-v"]);
    lines.push(`kdalc: ${result.stdout.trim() || "installed"}`);
  } else {
    lines.push("kdalc: not found");
  }

  lines.push(`stdlib: ${tc.stdlib || "not found"}`);

  vscode.window.showInformationMessage(lines.join(" | "));
}

function updateFileInfo(editor: vscode.TextEditor | undefined): void {
  if (!editor) {
    fileInfoItem.hide();
    return;
  }

  const fsPath = editor.document.uri.fsPath;
  if (fsPath.endsWith(".kdh")) {
    fileInfoItem.text = "$(file-code) KDAL Header";
    fileInfoItem.tooltip = "KDAL Device Header (.kdh)";
    fileInfoItem.show();
  } else if (fsPath.endsWith(".kdc")) {
    fileInfoItem.text = "$(file-code) KDAL Driver";
    fileInfoItem.tooltip = "KDAL Driver Code (.kdc)";
    fileInfoItem.show();
  } else {
    fileInfoItem.hide();
  }
}
