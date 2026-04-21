#include <argtable3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "biosim/stepper/cli.h"
#include "biosim/stepper/toml.h"

/* sim-stepper-specific parameter extensions */
static const biosim_param_entry_t stepper_params[] = {
    {"trace-out", {.s = ""}, {.s = ""}, PARAM_STRING, false},
};

biosim_status_t stepper_cli_and_toml(biosim_params_t *p, const biosim_build_info_t *info, int argc,
                                     char **argv) {
    biosim_status_t st =
        biosim_params_extend(p, stepper_params, sizeof(stepper_params) / sizeof(stepper_params[0]));
    if (st != BIOSIM_OK) {
        return st;
    }

    /* Static flags — always first in the argtable */
    struct arg_lit *arg_help = arg_lit0("h", "help", "print help and exit");
    struct arg_lit *arg_version = arg_lit0(NULL, "version", "print version and exit");
    struct arg_file *arg_config = arg_file0(NULL, "config", "<path>", "TOML config file");
    void *static_flags[] = {arg_help, arg_version, arg_config};
    size_t nstatic = sizeof(static_flags) / sizeof(static_flags[0]);

    size_t ndyn = biosim_params_count(p);
    size_t total = nstatic + ndyn + 1; /* +1 for arg_end */

    void **argtable = (void **)calloc(total, sizeof(void *));
    if (!argtable) {
        return BIOSIM_ERR_NOMEM;
    }

    memcpy((void *)argtable, (const void *)static_flags, nstatic * sizeof(void *));

    /* Build param-driven argtable entries after the static flags */
    for (size_t i = 0; i < ndyn; i++) {
        const biosim_param_entry_t *e = biosim_params_entry(p, i);
        switch (e->type) {
        case PARAM_INT:
            argtable[nstatic + i] = arg_int0(NULL, e->name, "<n>", "");
            break;
        case PARAM_FLOAT:
            argtable[nstatic + i] = arg_dbl0(NULL, e->name, "<v>", "");
            break;
        case PARAM_BOOL:
            argtable[nstatic + i] = arg_lit0(NULL, e->name, "");
            break;
        case PARAM_STRING:
            argtable[nstatic + i] = arg_str0(NULL, e->name, "<s>", "");
            break;
        }
    }

    struct arg_end *arg_end_s = arg_end(20);
    argtable[nstatic + ndyn] = arg_end_s;

    int nerrors = arg_parse(argc, argv, argtable);

    if (arg_help->count > 0) {
        printf("Usage: %s", info->progname);
        arg_print_syntax(stdout, argtable, "\n\n");
        printf("%s — biosim4-gpu single-threaded CPU reference simulator\n\n", info->progname);
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        arg_freetable(argtable, total);
        free((void *)argtable);
        exit(0);
    }
    if (arg_version->count > 0) {
        printf("%s %s (%s) [%s]\n", info->progname, info->version, info->build_timestamp,
               info->build_type);
        arg_freetable(argtable, total);
        free((void *)argtable);
        exit(0);
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, arg_end_s, info->progname);
        arg_freetable(argtable, total);
        free((void *)argtable);
        return BIOSIM_ERR_NOTFOUND;
    }

    /* Pass 2: TOML file overrides defaults */
    if (arg_config->count > 0) {
        stepper_load_toml_file(p, arg_config->filename[0]);
    }

    /* Pass 3: CLI flags override TOML */
    for (size_t i = 0; i < ndyn; i++) {
        const biosim_param_entry_t *e = biosim_params_entry(p, i);
        switch (e->type) {
        case PARAM_INT: {
            struct arg_int *a = argtable[nstatic + i];
            if (a->count > 0) {
                biosim_params_set_int(p, e->name, a->ival[0]);
            }
            break;
        }
        case PARAM_FLOAT: {
            struct arg_dbl *a = argtable[nstatic + i];
            if (a->count > 0) {
                biosim_params_set_float(p, e->name, a->dval[0]);
            }
            break;
        }
        case PARAM_BOOL: {
            struct arg_lit *a = argtable[nstatic + i];
            if (a->count > 0) {
                biosim_params_set_bool(p, e->name, true);
            }
            break;
        }
        case PARAM_STRING: {
            struct arg_str *a = argtable[nstatic + i];
            if (a->count > 0) {
                biosim_params_set_string(p, e->name, a->sval[0]);
            }
            break;
        }
        }
    }

    arg_freetable(argtable, total);
    free((void *)argtable);
    return BIOSIM_OK;
}
