/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Paths the user typed on kdality's command line.
 *
 * kdality runs with the user's own rights, so a path check defends no trust
 * boundary (docs/first-project-journey.md): inputs and output directories
 * are taken as given, absolute or relative, with "..". Only an empty path
 * or one with control characters is refused. Names that end up inside
 * generated files keep their own checks (dt-gen, test-gen, init).
 */
#ifndef KDALITY_PATHS_H
#define KDALITY_PATHS_H

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static inline int kdality_path_ok(const char *path)
{
	if (!path || path[0] == '\0')
		return 0;
	for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
		if (*p < 32 || *p == 127)
			return 0;
	}
	return 1;
}

/* An output directory: made (mode 0755) when missing. 0 on success. */
static inline int kdality_ensure_dir(const char *who, const char *dir)
{
	struct stat st;

	if (stat(dir, &st) == 0) {
		if (S_ISDIR(st.st_mode))
			return 0;
		(void)fprintf(stderr, "%s: '%s' is not a directory\n", who,
			      dir);
		return -1;
	}
	if (errno == ENOENT && mkdir(dir, 0755) == 0)
		return 0;
	(void)fprintf(stderr, "%s: cannot create directory '%s': %s\n", who,
		      dir, strerror(errno));
	return -1;
}

#endif /* KDALITY_PATHS_H */
