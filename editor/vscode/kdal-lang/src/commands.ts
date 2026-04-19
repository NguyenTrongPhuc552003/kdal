import * as vscode from "vscode";
import * as path from "path";
import * as https from "https";
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

  // Poll for the binaries every 3 s for up to 5 minutes.
  // This is more reliable than onDidCloseTerminal because the user may leave
  // the terminal open after the install script finishes.
  const kdalcPath = require("path").join(
    process.env["KDAL_HOME"] || require("path").join(process.env["HOME"] || "", ".kdal"),
    "bin", "kdalc"
  );
  const fs = require("fs") as typeof import("fs");
  let elapsed = 0;
  const POLL_MS = 3000;
  const MAX_MS = 5 * 60 * 1000;
  const poll = setInterval(() => {
    elapsed += POLL_MS;
    if (fs.existsSync(kdalcPath)) {
      clearInterval(poll);
      vscode.commands.executeCommand("kdal.redetectToolchain");
      vscode.window.showInformationMessage("KDAL SDK installed successfully.");
    } else if (elapsed >= MAX_MS) {
      clearInterval(poll);
    }
  }, POLL_MS);

  // Also re-detect when the terminal is closed (belt-and-suspenders)
  const disposable = vscode.window.onDidCloseTerminal((t) => {
    if (t === terminal) {
      disposable.dispose();
      clearInterval(poll);
      vscode.commands.executeCommand("kdal.redetectToolchain");
    }
  });
}

/* ── Extension update check ────────────────────────────────────── */

const REPO = "NguyenTrongPhuc552003/kdal";
const CHECK_INTERVAL_MS = 24 * 60 * 60 * 1000; // once per day
const LAST_CHECK_KEY = "kdal.lastUpdateCheck";

function fetchLatestTag(): Promise<string> {
  return new Promise((resolve, reject) => {
    const options = {
      hostname: "api.github.com",
      path: `/repos/${REPO}/releases/latest`,
      headers: { "User-Agent": "kdal-vscode-extension", "Accept": "application/vnd.github+json" },
    };
    https.get(options, (res) => {
      let data = "";
      res.on("data", (chunk) => { data += chunk; });
      res.on("end", () => {
        try {
          const json = JSON.parse(data);
          if (json.tag_name) resolve(json.tag_name as string);
          else reject(new Error("No tag_name in response"));
        } catch (e) {
          reject(e);
        }
      });
    }).on("error", reject);
  });
}

export async function checkForExtensionUpdate(context: vscode.ExtensionContext): Promise<void> {
  // Throttle to once per day
  const lastCheck = context.globalState.get<number>(LAST_CHECK_KEY, 0);
  if (Date.now() - lastCheck < CHECK_INTERVAL_MS) return;

  try {
    const latestTag = await fetchLatestTag(); // e.g. "v0.2.0"
    await context.globalState.update(LAST_CHECK_KEY, Date.now());

    const latestVersion = latestTag.replace(/^v/, "");
    const currentVersion: string = context.extension.packageJSON.version;

    if (latestVersion === currentVersion) return;

    // Simple semver comparison: just compare strings after splitting on '.'
    const toNum = (v: string) => v.split(".").map(Number);
    const [lMaj, lMin, lPat] = toNum(latestVersion);
    const [cMaj, cMin, cPat] = toNum(currentVersion);

    const isNewer =
      lMaj > cMaj ||
      (lMaj === cMaj && lMin > cMin) ||
      (lMaj === cMaj && lMin === cMin && lPat > cPat);

    if (!isNewer) return;

    const choice = await vscode.window.showInformationMessage(
      `KDAL extension ${latestTag} is available (you have v${currentVersion}). Download the new .vsix from GitHub Releases.`,
      "Download",
      "Later"
    );
    if (choice === "Download") {
      vscode.env.openExternal(
        vscode.Uri.parse(`https://github.com/${REPO}/releases/tag/${latestTag}`)
      );
    }
  } catch {
    // Network errors are silent — don't interrupt the user
  }
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
