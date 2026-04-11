/*
 * simulate.c — kdality simulate subcommand.
 *
 * Dry-run interpreter that traces .kdc driver execution without a kernel.
 * Simulates register reads/writes to a virtual register file and prints
 * a step-by-step execution trace.
 *
 *   kdality simulate <file.kdc> [--trace] [--event <name>]
 *
 * This lets newcomers see what their driver does before compiling and
 * loading a kernel module.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Virtual register file ───────────────────────────────────────── */

#define MAX_VREGS 64
#define NAME_LEN 64
#define LINE_LEN 512

struct vreg {
	char name[NAME_LEN];
	unsigned long value;
};

struct sim_state {
	struct vreg regs[MAX_VREGS];
	int nregs;
	int trace;
	int step;
};

static unsigned long sim_reg_read(struct sim_state *s, const char *name)
{
	for (int i = 0; i < s->nregs; i++) {
		if (strcmp(s->regs[i].name, name) == 0) {
			if (s->trace)
				printf("  [%d] reg_read(%s) → 0x%lx\n",
				       s->step++, name, s->regs[i].value);
			return s->regs[i].value;
		}
	}
	/* Unknown register — create with zero */
	if (s->nregs < MAX_VREGS) {
		strncpy(s->regs[s->nregs].name, name, NAME_LEN - 1);
		s->regs[s->nregs].value = 0;
		s->nregs++;
	}
	if (s->trace)
		printf("  [%d] reg_read(%s) → 0x0 (new)\n", s->step++, name);
	return 0;
}

static void sim_reg_write(struct sim_state *s, const char *name,
			  unsigned long val)
{
	for (int i = 0; i < s->nregs; i++) {
		if (strcmp(s->regs[i].name, name) == 0) {
			if (s->trace)
				printf("  [%d] reg_write(%s, 0x%lx) "
				       "[was 0x%lx]\n",
				       s->step++, name, val, s->regs[i].value);
			s->regs[i].value = val;
			return;
		}
	}
	if (s->nregs < MAX_VREGS) {
		strncpy(s->regs[s->nregs].name, name, NAME_LEN - 1);
		s->regs[s->nregs].value = val;
		s->nregs++;
		if (s->trace)
			printf("  [%d] reg_write(%s, 0x%lx) [new]\n", s->step++,
			       name, val);
	}
}

static void sim_log(struct sim_state *s, const char *msg)
{
	if (s->trace)
		printf("  [%d] log(\"%s\")\n", s->step++, msg);
	else
		printf("  LOG: %s\n", msg);
}

/* ── Simple .kdc line interpreter ────────────────────────────────
 *
 * We interpret .kdc files line by line, recognizing:
 *   reg_write(NAME, VALUE);
 *   reg_read(NAME)
 *   log("message");
 *   on <event> {  — starts handler scope
 *   }             — ends handler scope
 *
 * This is intentionally simple (not a full AST walk). It demonstrates
 * the concept of dry-run simulation for newcomers.
 */

static int simulate_handler(FILE *fp, const char *handler_name,
			    struct sim_state *s)
{
	char line[LINE_LEN];
	int depth = 1;

	printf("\n── Simulating: on %s ──\n", handler_name);
	s->step = 0;

	while (fgets(line, sizeof(line), fp) && depth > 0) {
		char *p = line;
		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '\n' || *p == '\0' || *p == '/')
			continue;

		/* Track brace depth */
		for (char *c = p; *c; c++) {
			if (*c == '{')
				depth++;
			if (*c == '}')
				depth--;
		}

		if (depth <= 0)
			break;

		/* reg_write(NAME, VALUE); */
		char rname[NAME_LEN];
		unsigned long val;
		if (sscanf(p, "reg_write ( %63[^,] , %li )", rname, &val) ==
			    2 ||
		    sscanf(p, "reg_write(%63[^,], %li)", rname, &val) == 2 ||
		    sscanf(p, "reg_write(%63[^,],%li)", rname, &val) == 2) {
			sim_reg_write(s, rname, val);
			continue;
		}

		/* reg_read(NAME) - standalone */
		if (sscanf(p, "reg_read ( %63[^)] )", rname) == 1 ||
		    sscanf(p, "reg_read(%63[^)])", rname) == 1) {
			sim_reg_read(s, rname);
			continue;
		}

		/* val = reg_read(NAME); */
		char vname[NAME_LEN];
		if (sscanf(p, "%63s = reg_read(%63[^)])", vname, rname) == 2 ||
		    sscanf(p, "%63s = reg_read ( %63[^)] )", vname, rname) ==
			    2) {
			unsigned long v = sim_reg_read(s, rname);
			if (s->trace)
				printf("  [%d] %s = 0x%lx\n", s->step++, vname,
				       v);
			continue;
		}

		/* log("message"); */
		char *lq = strstr(p, "log(\"");
		if (lq) {
			lq += 5;
			char *end = strchr(lq, '"');
			if (end) {
				char msg[256];
				size_t len = (size_t)(end - lq);
				if (len >= sizeof(msg))
					len = sizeof(msg) - 1;
				memcpy(msg, lq, len);
				msg[len] = '\0';
				sim_log(s, msg);
			}
			continue;
		}

		/* return — just note it */
		if (strncmp(p, "return", 6) == 0) {
			if (s->trace)
				printf("  [%d] return\n", s->step++);
			continue;
		}
	}

	return 0;
}

static int simulate_file(const char *path, const char *target_event, int trace)
{
	FILE *fp;
	char line[LINE_LEN];
	struct sim_state state;
	char driver_name[NAME_LEN] = "";
	char device_name[NAME_LEN] = "";
	int found = 0;

	memset(&state, 0, sizeof(state));
	state.trace = trace;

	fp = fopen(path, "r");
	if (!fp) {
		fprintf(stderr, "simulate: cannot open '%s': %s\n", path,
			strerror(errno));
		return -1;
	}

	printf("=== KDAL Simulation: %s ===\n", path);

	while (fgets(line, sizeof(line), fp)) {
		char *p = line;
		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '\n' || *p == '\0' || *p == '/')
			continue;

		/* driver NAME for DEVICE { */
		if (strncmp(p, "driver ", 7) == 0) {
			sscanf(p, "driver %63s for %63[^ {]", driver_name,
			       device_name);
			printf("Driver: %s (device: %s)\n", driver_name,
			       device_name);
			continue;
		}

		/* on <event> { */
		char event[NAME_LEN] = "";
		char extra[NAME_LEN] = "";
		if (strncmp(p, "on ", 3) == 0) {
			sscanf(p + 3, "%63s %63[^{ \n]", event, extra);

			/* If targeting a specific event, skip others */
			if (target_event) {
				if (strcmp(event, target_event) != 0 &&
				    (extra[0] == '\0' ||
				     strcmp(extra, target_event) != 0)) {
					/* Skip this handler */
					int depth = 0;
					for (char *c = p; *c; c++) {
						if (*c == '{')
							depth++;
					}
					while (depth > 0 &&
					       fgets(line, sizeof(line), fp)) {
						for (char *c = line; *c; c++) {
							if (*c == '{')
								depth++;
							if (*c == '}')
								depth--;
						}
					}
					continue;
				}
			}

			char full_event[128];
			if (extra[0] && strcmp(extra, "{") != 0)
				snprintf(full_event, sizeof(full_event),
					 "%s %s", event, extra);
			else
				snprintf(full_event, sizeof(full_event), "%s",
					 event);

			simulate_handler(fp, full_event, &state);
			found = 1;
			continue;
		}
	}

	fclose(fp);

	if (!found && target_event) {
		fprintf(stderr, "simulate: handler 'on %s' not found in '%s'\n",
			target_event, path);
		return 1;
	}

	/* Print final register state */
	if (state.nregs > 0) {
		printf("\n── Final register state ──\n");
		for (int i = 0; i < state.nregs; i++) {
			printf("  %-16s = 0x%lx\n", state.regs[i].name,
			       state.regs[i].value);
		}
	}

	printf("\n=== Simulation complete ===\n");
	return 0;
}

/* ── help ────────────────────────────────────────────────────────── */

static void simulate_help(void)
{
	fprintf(stderr,
		"Usage: kdality simulate <file.kdc> [options]\n\n"
		"Dry-run a .kdc driver without a kernel. Shows register\n"
		"read/write traces and final register state.\n\n"
		"Options:\n"
		"  --trace            show step-by-step execution trace\n"
		"  --event <name>     simulate only this handler (e.g. probe, read)\n"
		"  -h, --help         show this help\n\n"
		"Example:\n"
		"  kdality simulate examples/kdc_hello/uart_hello.kdc --trace\n"
		"  kdality simulate examples/kdc_hello/uart_hello.kdc --event probe\n");
}

int kdality_simulate(int argc, char *argv[])
{
	const char *input = NULL;
	const char *target_event = NULL;
	int trace = 0;

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			simulate_help();
			return 0;
		} else if (strcmp(argv[i], "--trace") == 0) {
			trace = 1;
		} else if (strcmp(argv[i], "--event") == 0 && i + 1 < argc) {
			target_event = argv[++i];
		} else if (argv[i][0] != '-') {
			input = argv[i];
		}
	}

	if (!input) {
		fprintf(stderr, "simulate: no input .kdc file\n\n");
		simulate_help();
		return 1;
	}

	return simulate_file(input, target_event, trace);
}
