/*
 * kdality — KDAL utility: runtime control CLI + language toolchain.
 */

#include <stdio.h>
#include <string.h>

int kdalctl_run(int argc, char **argv);
int kdality_compile(int argc, char **argv);
int kdality_init(int argc, char **argv);
int kdality_dtgen(int argc, char **argv);
int kdality_simulate(int argc, char **argv);
int kdality_testgen(int argc, char **argv);
int kdality_lint(int argc, char **argv);

int main(int argc, char **argv)
{
	if (argc >= 2 && strcmp(argv[1], "compile") == 0)
		return kdality_compile(argc - 1, argv + 1);

	if (argc >= 2 && strcmp(argv[1], "init") == 0)
		return kdality_init(argc - 1, argv + 1);

	if (argc >= 2 && strcmp(argv[1], "dt-gen") == 0)
		return kdality_dtgen(argc - 1, argv + 1);

	if (argc >= 2 && strcmp(argv[1], "simulate") == 0)
		return kdality_simulate(argc - 1, argv + 1);

	if (argc >= 2 && strcmp(argv[1], "test-gen") == 0)
		return kdality_testgen(argc - 1, argv + 1);

	if (argc >= 2 && strcmp(argv[1], "lint") == 0)
		return kdality_lint(argc - 1, argv + 1);

	return kdalctl_run(argc, argv);
}
