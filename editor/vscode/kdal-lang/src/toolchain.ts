import * as vscode from "vscode";
import * as path from "path";
import * as fs from "fs";
import { execFile } from "child_process";

export interface ToolchainPaths {
  kdalc: string | undefined;
  kdality: string | undefined;
  stdlib: string | undefined;
}

/**
 * Locate kdalc and kdality binaries by searching:
 *  1. User-configured kdal.toolchainPath
 *  2. KDAL_HOME/bin  (kdalup layout)
 *  3. System PATH
 */
export function findToolchain(): ToolchainPaths {
  const config = vscode.workspace.getConfiguration("kdal");
  const configured = config.get<string>("toolchainPath");

  const candidates: string[] = [];

  if (configured && configured.length > 0) {
    candidates.push(configured);
  }

  const kdalHome = process.env["KDAL_HOME"] || path.join(homeDir(), ".kdal");
  candidates.push(path.join(kdalHome, "bin"));

  const result: ToolchainPaths = {
    kdalc: undefined,
    kdality: undefined,
    stdlib: undefined,
  };

  for (const dir of candidates) {
    if (!result.kdalc) {
      const p = path.join(dir, "kdalc");
      if (fs.existsSync(p)) {
        result.kdalc = p;
      }
    }
    if (!result.kdality) {
      const p = path.join(dir, "kdality");
      if (fs.existsSync(p)) {
        result.kdality = p;
      }
    }
  }

  // Fall back to PATH
  if (!result.kdalc) {
    result.kdalc = whichSync("kdalc");
  }
  if (!result.kdality) {
    result.kdality = whichSync("kdality");
  }

  // Stdlib search
  const stdlibDirs = [
    config.get<string>("stdlibPath"),
    path.join(kdalHome, "share", "kdal", "stdlib"),
    "/usr/local/share/kdal/stdlib",
    "/usr/share/kdal/stdlib",
  ];
  for (const d of stdlibDirs) {
    if (d && fs.existsSync(d)) {
      result.stdlib = d;
      break;
    }
  }

  return result;
}

export function runTool(
  bin: string,
  args: string[],
  cwd?: string
): Promise<{ stdout: string; stderr: string; code: number }> {
  return new Promise((resolve) => {
    execFile(bin, args, { cwd, maxBuffer: 4 * 1024 * 1024, timeout: 30_000 }, (err, stdout, stderr) => {
      resolve({
        stdout: stdout || "",
        stderr: stderr || "",
        code: err
          ? (typeof (err as any).code === "number" ? (err as any).code : 1)
          : 0,
      });
    });
  });
}

function homeDir(): string {
  return process.env["HOME"] || process.env["USERPROFILE"] || "/tmp";
}

function whichSync(name: string): string | undefined {
  const pathEnv = process.env["PATH"] || "";
  const dirs = pathEnv.split(path.delimiter);
  for (const dir of dirs) {
    const full = path.join(dir, name);
    try {
      fs.accessSync(full, fs.constants.X_OK);
      return full;
    } catch {
      // not found here
    }
  }
  return undefined;
}
