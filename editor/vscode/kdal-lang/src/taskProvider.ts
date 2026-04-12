import * as vscode from "vscode";

const TASK_TYPE = "kdal";

interface KdalTaskDefinition extends vscode.TaskDefinition {
  command: string;
  file?: string;
}

export class KdalTaskProvider implements vscode.TaskProvider {
  static readonly type = TASK_TYPE;
  private toolchainPath: string;

  constructor(toolchainPath?: string) {
    this.toolchainPath = toolchainPath || "kdality";
  }

  provideTasks(): vscode.ProviderResult<vscode.Task[]> {
    const tasks: vscode.Task[] = [];
    const ws = vscode.workspace.workspaceFolders;
    if (!ws) return tasks;

    const folder = ws[0];

    tasks.push(
      this.makeTask(folder, "compile", "KDAL: Compile active file", ["compile"]),
      this.makeTask(folder, "lint", "KDAL: Lint active file", ["lint"]),
      this.makeTask(folder, "simulate", "KDAL: Simulate active file", ["simulate"]),
      this.makeTask(folder, "dt-gen", "KDAL: Generate device tree", ["dt-gen"]),
      this.makeTask(folder, "test-gen", "KDAL: Generate test", ["test-gen"])
    );

    return tasks;
  }

  resolveTask(task: vscode.Task): vscode.ProviderResult<vscode.Task> {
    const def = task.definition as KdalTaskDefinition;
    if (def.command) {
      const file = def.file
        || vscode.window.activeTextEditor?.document.uri.fsPath
        || "";
      return this.makeTask(
        task.scope as vscode.WorkspaceFolder,
        def.command,
        task.name,
        [def.command, file]
      );
    }
    return undefined;
  }

  private makeTask(
    folder: vscode.WorkspaceFolder,
    command: string,
    label: string,
    args: string[]
  ): vscode.Task {
    const def: KdalTaskDefinition = { type: TASK_TYPE, command };
    // Resolve the active file at task execution time via ${file}
    const fullArgs = args.length === 1
      ? [...args, "${file}"]
      : args;
    const exec = new vscode.ShellExecution(this.toolchainPath, fullArgs);
    const task = new vscode.Task(def, folder, label, "kdal", exec, "$gcc");
    task.group = vscode.TaskGroup.Build;
    task.presentationOptions = { reveal: vscode.TaskRevealKind.Always };
    return task;
  }
}
