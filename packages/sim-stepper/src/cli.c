#include <argtable3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "biosim/stepper/cli.h"
#include "biosim/stepper/toml.h"

/* sim-stepper-specific parameter extensions */
static const biosim_param_entry_t s_stepper_params[] = {
    {"sim-name",  PARAM_STRING, {.s = "unnamed"}, {.s = "unnamed"}, false},
    {"trace-out", PARAM_STRING, {.s = ""},        {.s = ""},        false},
};

biosim_status_t stepper_params_resolve(biosim_params_t *p,
                                       int argc, char **argv) {
    /* Pass 1 extension: add stepper-specific params */
    biosim_status_t st = biosim_params_extend(
        p, s_stepper_params,
        sizeof(s_stepper_params) / sizeof(s_stepper_params[0]));
    if (st != BIOSIM_OK)
        return st;

    size_t n = biosim_params_count(p);

    /* +4 slots: --config, -h/--help, --version, arg_end */
    void **argtable = calloc(n + 4, sizeof(void *));
    if (!argtable)
        return BIOSIM_ERR_NOMEM;

    /* Build param-driven argtable entries */
    for (size_t i = 0; i < n; i++) {
        const biosim_param_entry_t *e = biosim_params_entry(p, i);
        switch (e->type) {
        case PARAM_INT:
            argtable[i] = arg_int0(NULL, e->name, "<n>", "");
            break;
        case PARAM_FLOAT:
            argtable[i] = arg_dbl0(NULL, e->name, "<v>", "");
            break;
        case PARAM_BOOL:
            argtable[i] = arg_lit0(NULL, e->name, "");
            break;
        case PARAM_STRING:
            argtable[i] = arg_str0(NULL, e->name, "<s>", "");
            break;
        }
    }

    /* Non-param flags */
    struct arg_file *arg_config  = arg_file0(NULL, "config",  "<path>",
                                             "TOML config file");
    struct arg_lit  *arg_help    = arg_lit0("h",   "help",    "print help and exit");
    struct arg_lit  *arg_version = arg_lit0(NULL,  "version", "print version and exit");
    struct arg_end  *arg_end_s   = arg_end(20);

    argtable[n]     = arg_config;
    argtable[n + 1] = arg_help;
    argtable[n + 2] = arg_version;
    argtable[n + 3] = arg_end_s;

    int nerrors = arg_parse(argc, argv, argtable);

    if (arg_help->count > 0) {
        printf("Usage: biosim-stepper");
        arg_print_syntax(stdout, argtable, "\n");
        arg_freetable(argtable, n + 4);
        free(argtable);
        exit(0);
    }
    if (arg_version->count > 0) {
        printf("biosim-stepper 0.1.0\n");
        arg_freetable(argtable, n + 4);
        free(argtable);
        exit(0);
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, arg_end_s, "biosim-stepper");
        arg_freetable(argtable, n + 4);
        free(argtable);
        return BIOSIM_ERR_NOTFOUND;
    }

    /* Pass 2: TOML file */
    if (arg_config->count > 0)
        stepper_load_toml_file(p, arg_config->filename[0]);

    /* Pass 3: CLI flags override TOML */
    for (size_t i = 0; i < n; i++) {
        const biosim_param_entry_t *e = biosim_params_entry(p, i);
        switch (e->type) {
        case PARAM_INT: {
            struct arg_int *a = argtable[i];
            if (a->count > 0)
                biosim_params_set_int(p, e->name, a->ival[0]);
            break;
        }
        case PARAM_FLOAT: {
            struct arg_dbl *a = argtable[i];
            if (a->count > 0)
                biosim_params_set_float(p, e->name, a->dval[0]);
            break;
        }
        case PARAM_BOOL: {
            struct arg_lit *a = argtable[i];
            if (a->count > 0)
                biosim_params_set_bool(p, e->name, true);
            break;
        }
        case PARAM_STRING: {
            struct arg_str *a = argtable[i];
            if (a->count > 0)
                biosim_params_set_string(p, e->name, a->sval[0]);
            break;
        }
        }
    }

    arg_freetable(argtable, n + 4);
    free(argtable);
    return BIOSIM_OK;
}
