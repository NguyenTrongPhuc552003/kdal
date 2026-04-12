import * as vscode from "vscode";
import * as path from "path";
import { ToolchainPaths, runTool } from "./toolchain";

let outputChannel: vscode.OutputChannel;

export function setOutputChannel(ch: vscode.OutputChannel): void {
  outputChannel = ch;
}

/* ── Helper: prompt SDK install when toolchain missing ─────────── */

const SDK_MISSING_MSG = "KDAL SDK not found. Install it to use this feature.";

async function requireTool(tc: ToolchainPaths, tool: "kdality" | "kdalc"): Promise<string | undefined> {
  const bin = tc[tool];
  if (bin) return bin;
  const choice = await vscode.window.showErrorMessage(
    SDK_MISSING_MSG,
    "Install SDK",
    "Configure Path"
  );
  if (choice === "Install SDK") {
    vscode.commands.executeCommand("kdal.installSDK");
  } else if (choice === "Configure Path") {
    vscode.commands.executeCommand("workbench.action.openSettings", "kdal.toolchainPath");
  }
  return undefined;
}

/* ── Install SDK command ───────────────────────────────────────── */

export async function installSDKCommand(): Promise<void> {
  const platform = process.platform;
  if (platform !== "linux" && platform !== "darwin") {
    vscode.window.showErrorMessage("KDAL SDK installer supports Linux and macOS only.");
    return;
  }

  const choice = await vscode.window.showInformationMessage(
    "Install the KDAL SDK via kdalup?",
    { modal: false },
    "Install Latest",
    "Open Documentation"
  );

  if (choice === "Open Documentation") {
    vscode.env.openExternal(vscode.Uri.parse(
      "https://github.com/NguyenTrongPhuc552003/kdal/blob/main/docs/installation.md"
    ));
    return;
  }

  if (choice !== "Install Latest") return;

  const terminal = vscode.window.createTerminal({ name: "KDAL SDK Install" });
  terminal.show();
  terminal.sendText(
    'curl -fsSL https://raw.githubusercontent.com/NguyenTrongPhuc552003/kdal/main/scripts/installer/install.sh | sh'
  );

  // Watch for terminal close to trigger re-detection
  const disposable = vscode.window.onDidCloseTerminal((t) => {
    if (t === terminal) {
      disposable.dispose();
      vscode.commands.executeCommand("kdal.redetectToolchain");
    }
  });
}

/* ── Helper: resolve active .kdh/.kdc file ─────────────────────── */

function activeKdalFile(): string | undefined {
  const editor = vscode.window.activeTextEditor;
  if (!editor) {
    vscode.window.showWarningMessage("KDAL: No active editor");
    return undefined;
  }
  const fsPath = editor.document.uri.fsPath;
  if (!fsPath.endsWith(".kdh") && !fsPath.endsWith(".kdc")) {
    vscode.window.showWarningMessage("KDAL: Active file is not a .kdh or .kdc file");
    return undefined;
  }
  return fsPath;
}

/* ── Commands ──────────────────────────────────────────────────── */

export async function compileCommand(tc: ToolchainPaths): Promise<void> {
  const file = activeKdalFile();
  if (!file) return;

  const bin = await requireTool(tc, "kdality");
  if (!bin) return;

  outputChannel.show(true);
  outputChannel.appendLine(`[KDAL] Compiling ${path.basename(file)}...`);

  const cwd = path.dirname(file);
  const result = await runTool(bin, ["compile", file], cwd);

  outputChannel.appendLine(result.stdout);
  if (result.stderr) outputChannel.appendLine(result.stderr);

  if (result.code === 0) {
    vscode.window.showInformationMessage(`KDAL: Compiled ${path.basename(file)} successfully`);
  } else {
    vscode.window.showErrorMessage(`KDAL: Compilation failed - see output`);
  }
}

export async function lintCommand(
  tc: ToolchainPaths,
  diagnosticCollection: vscode.DiagnosticCollection,
  document?: vscode.TextDocument
): Promise<void> {
  const file = document ? document.uri.fsPath : activeKdalFile();
  if (!file) return;

  const bin = await requireTool(tc, "kdality");
  if (!bin) return;

  const cwd = path.dirname(file);
  const result = await runTool(bin, ["lint", file], cwd);

  const diagnostics = parseLintOutput(result.stdout + result.stderr, file);
  const uri = vscode.Uri.file(file);
  diagnosticCollection.set(uri, diagnostics);

  if (!document) {
    if (diagnostics.length === 0) {
      vscode.window.showInformationMessage(`KDAL: No lint warnings in ${path.basename(file)}`);
    } else {
      vscode.window.showWarningMessage(`KDAL: ${diagnostics.length} lint issue(s) in ${path.basename(file)}`);
    }
  }
}

export async function initCommand(tc: ToolchainPaths): Promise<void> {
  const bin = await requireTool(tc, "kdality");
  if (!bin) return;

  const name = await vscode.window.showInputBox({
    prompt: "New KDAL project name",
    placeHolder: "mydriver",
    validateInput: (v) => /^[a-z][a-z0-9_]*$/.test(v) ? null : "Lowercase alphanumeric name with underscores",
  });
  if (!name) return;

  const folders = vscode.workspace.workspaceFolders;
  const cwd = folders ? folders[0].uri.fsPath : undefined;
  if (!cwd) {
    vscode.window.showErrorMessage("KDAL: Open a workspace folder first");
    return;
  }

  outputChannel.show(true);
  outputChannel.appendLine(`[KDAL] Creating project "${name}"...`);

  const result = await runTool(bin, ["init", name], cwd);
  outputChannel.appendLine(result.stdout);
  if (result.stderr) outputChannel.appendLine(result.stderr);

  if (result.code === 0) {
    vscode.window.showInformationMessage(`KDAL: Project "${name}" created`);
  } else {
    vscode.window.showErrorMessage("KDAL: Project creation failed - see output");
  }
}

export async function simulateCommand(tc: ToolchainPaths): Promise<void> {
  const file = activeKdalFile();
  if (!file || !file.endsWith(".kdh")) {
    vscode.window.showWarningMessage("KDAL: Simulate requires an active .kdh file");
    return;
  }

  const bin = await requireTool(tc, "kdality");
  if (!bin) return;

  outputChannel.show(true);
  outputChannel.appendLine(`[KDAL] Simulating ${path.basename(file)}...`);

  const cwd = path.dirname(file);
  const result = await runTool(bin, ["simulate", file], cwd);
  outputChannel.appendLine(result.stdout);
  if (result.stderr) outputChannel.appendLine(result.stderr);
}

export async function dtgenCommand(tc: ToolchainPaths): Promise<void> {
  const file = activeKdalFile();
  if (!file || !file.endsWith(".kdh")) {
    vscode.window.showWarningMessage("KDAL: dt-gen requires an active .kdh file");
    return;
  }

  const bin = await requireTool(tc, "kdality");
  if (!bin) return;

  outputChannel.show(true);
  outputChannel.appendLine(`[KDAL] Generating device tree from ${path.basename(file)}...`);

  const cwd = path.dirname(file);
  const result = await runTool(bin, ["dt-gen", file], cwd);
  outputChannel.appendLine(result.stdout);
  if (result.stderr) outputChannel.appendLine(result.stderr);

  if (result.code === 0) {
    vscode.window.showInformationMessage("KDAL: Device tree overlay generated");
  }
}

export async function testgenCommand(tc: ToolchainPaths): Promise<void> {
  const file = activeKdalFile();
  if (!file || !file.endsWith(".kdc")) {
    vscode.window.showWarningMessage("KDAL: test-gen requires an active .kdc file");
    return;
  }

  const bin = await requireTool(tc, "kdality");
  if (!bin) return;

  outputChannel.show(true);
  outputChannel.appendLine(`[KDAL] Generating tests from ${path.basename(file)}...`);

  const cwd = path.dirname(file);
  const result = await runTool(bin, ["test-gen", file], cwd);
  outputChannel.appendLine(result.stdout);
  if (result.stderr) outputChannel.appendLine(result.stderr);

  if (result.code === 0) {
    vscode.window.showInformationMessage("KDAL: Test file generated");
  }
}

/* ── Lint output parser ────────────────────────────────────────── */

function parseLintOutput(output: string, file: string): vscode.Diagnostic[] {
  const diagnostics: vscode.Diagnostic[] = [];
  // Match patterns like:  file.kdh:10: warning: message
  // or:  file.kdh:10:5: error: message
  const linePattern = /(?:^|\n)([^:\n]+):(\d+)(?::(\d+))?:\s*(warning|error|info):\s*(.+)/g;
  let match: RegExpExecArray | null;

  while ((match = linePattern.exec(output)) !== null) {
    const line = Math.max(0, parseInt(match[2], 10) - 1);
    const col = match[3] ? Math.max(0, parseInt(match[3], 10) - 1) : 0;
    const severity = match[4] === "error"
      ? vscode.DiagnosticSeverity.Error
      : match[4] === "warning"
        ? vscode.DiagnosticSeverity.Warning
        : vscode.DiagnosticSeverity.Information;
    const message = match[5].trim();

    const range = new vscode.Range(line, col, line, col + 1);
    diagnostics.push(new vscode.Diagnostic(range, message, severity));
  }

  return diagnostics;
}
