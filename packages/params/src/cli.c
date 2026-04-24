#include <argtable3.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "biosim/params/params.h"

/* Forward declaration of internal TOML loader defined in toml.c */
biosim_status_t params_load_toml_file(biosim_params_t *p, const char *path);

/* ── static data ────────────────────────────────────────────────────────── */

/* Glossary section order — NULL first (top-level params), then named tables. */
static const char *const glossary_tables_order[] = {NULL, "simulation"};
#define GLOSSARY_TABLES_COUNT (sizeof(glossary_tables_order) / sizeof(glossary_tables_order[0]))

/* ── internal helpers ───────────────────────────────────────────────────── */

static bool str_eq_nullable(const char *a, const char *b) {
    if (a == NULL && b == NULL) {
        return true;
    }
    if (a == NULL || b == NULL) {
        return false;
    }
    return strcmp(a, b) == 0;
}

/* Prints the usage one-liner using arg_print_syntax on a filtered shallow copy of argtable. */
static void print_synopsis(FILE *fp, const char *progname, void **argtable, size_t nstatic,
                           const biosim_params_t *p, size_t ndyn) {
    size_t nsyn = 0;
    for (size_t i = 0; i < ndyn; i++) {
        if (biosim_params_entry(p, i)->cli_long != NULL) {
            nsyn++;
        }
    }

    void **syn = (void **)malloc((nstatic + nsyn + 1) * sizeof(void *));
    if (!syn) {
        (void)fprintf(stderr, "fatal: unhandled allocation error\n");
        return;
    }
    memcpy((void *)syn, (const void *)argtable, nstatic * sizeof(void *));

    size_t k = nstatic;
    for (size_t i = 0; i < ndyn; i++) {
        if (biosim_params_entry(p, i)->cli_long != NULL) {
            syn[k++] = argtable[nstatic + i];
        }
    }
    struct arg_end *syn_end = arg_end(1);
    syn[k] = syn_end;

    (void)fprintf(fp, "Usage: %s", progname);
    arg_print_syntax(fp, syn, "\n\n");

    /* We only free the shallow copy including the dedicated arg_end(1) */
    arg_freetable(&syn[k], 1);
    free((void *)syn);
}

/* Prints full glossary using arg_print_glossary, shallow grouped by params table. */
static void print_glossary(FILE *fp, void **argtable, size_t nstatic, const biosim_params_t *p,
                           size_t ndyn) {
    void **stbl = (void **)malloc((nstatic + 1) * sizeof(void *));
    if (!stbl) {
        (void)fprintf(stderr, "fatal: unhandled allocation error\n");
        return;
    }

    /* The convention is argtable[:nstatic] are static arguments */
    memcpy((void *)stbl, (const void *)argtable, nstatic * sizeof(void *));
    struct arg_end *stbl_end = arg_end(1);
    stbl[nstatic] = stbl_end;
    arg_print_glossary(fp, stbl, "  %-25s %s\n");
    /* We only free the shallow copy including the dedicated arg_end(1) */
    arg_freetable(&stbl[nstatic], 1);
    free((void *)stbl);

    for (size_t t = 0; t < GLOSSARY_TABLES_COUNT; t++) {
        const char *tname = glossary_tables_order[t];
        size_t ntable = 0;
        for (size_t i = 0; i < ndyn; i++) {
            if (str_eq_nullable(biosim_params_entry(p, i)->table, tname)) {
                ntable++;
            }
        }
        if (ntable == 0) {
            continue;
        }

        (void)fprintf(fp, "\n");
        if (tname != NULL) {
            (void)fprintf(fp, "[%s] parameters\n", tname);
        }

        void **tbl = (void **)malloc((ntable + 1) * sizeof(void *));
        if (!tbl) {
            (void)fprintf(stderr, "fatal: unhandled allocation error\n");
            return;
        }
        size_t k = 0;
        for (size_t i = 0; i < ndyn; i++) {
            if (str_eq_nullable(biosim_params_entry(p, i)->table, tname)) {
                tbl[k++] = argtable[nstatic + i];
            }
        }
        struct arg_end *tbl_end = arg_end(1);
        tbl[k] = tbl_end;
        arg_print_glossary(fp, tbl, "  %-25s %s\n");

        /* We only free the shallow copy including the dedicated arg_end(1) */
        arg_freetable(&tbl[k], 1);
        free((void *)tbl);
    }
    (void)fprintf(fp, "\n");
}

/* Formats the default value of e into a malloc'd "default: <val>" string, or NULL on failure. */
static char *format_default(const biosim_param_entry_t *e) {
    char buf[256];
    switch (e->type) {
    case PARAM_INT:
        (void)snprintf(buf, sizeof(buf), "default: %d", e->value.i);
        break;
    case PARAM_FLOAT:
        (void)snprintf(buf, sizeof(buf), "default: %g", e->value.f);
        break;
    case PARAM_BOOL:
        (void)snprintf(buf, sizeof(buf), "default: %s", e->value.b ? "true" : "false");
        break;
    case PARAM_STRING:
        (void)snprintf(buf, sizeof(buf), "default: %s", e->value.s);
        break;
    }
    size_t n = strlen(buf) + 1;
    char *out = (char *)malloc(n);
    if (out) {
        memcpy(out, buf, n);
    }
    return out;
}

/* Appends argtable3 entries for every param (1-to-1: param i → argtable[nstatic+i]).
 * Auto-generated long flag names are malloc'd into generated[i]; NULL otherwise.
 * Formatted default strings are malloc'd into glossaries[i]; NULL otherwise. */
static void build_argtable(void **argtable, size_t nstatic, const biosim_params_t *p, size_t ndyn,
                           char **generated, char **glossaries) {
    for (size_t i = 0; i < ndyn; i++) {
        const biosim_param_entry_t *e = biosim_params_entry(p, i);

        generated[i] = NULL;
        glossaries[i] = NULL;

        const char *longflag;
        if (e->cli_long != NULL) {
            longflag = e->cli_long;
        } else if (e->table != NULL) {
            size_t len = strlen(e->table) + 1 + strlen(e->name) + 1;
            generated[i] = (char *)malloc(len);
            (void)snprintf(generated[i], len, "%s-%s", e->table, e->name);
            longflag = generated[i];
        } else {
            longflag = e->name;
        }

        const char *glossary = "";
        if (e->has_default) {
            glossaries[i] = format_default(e);
            if (glossaries[i]) {
                glossary = glossaries[i];
            }
        }

        switch (e->type) {
        case PARAM_INT:
            argtable[nstatic + i] = arg_int0(e->cli_short, longflag, "<n>", glossary);
            break;
        case PARAM_FLOAT:
            argtable[nstatic + i] = arg_dbl0(e->cli_short, longflag, "<v>", glossary);
            break;
        case PARAM_BOOL:
            argtable[nstatic + i] = arg_lit0(e->cli_short, longflag, glossary);
            break;
        case PARAM_STRING:
            argtable[nstatic + i] = arg_str0(e->cli_short, longflag, "<s>", glossary);
            break;
        }
    }
}

/* Reads parsed CLI values back into params (1-to-1: param i → argtable[nstatic+i]). */
static void apply_cli_args(void **argtable, size_t nstatic, biosim_params_t *p, size_t ndyn) {
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
}

static void free_generated(char **generated, size_t n) {
    for (size_t i = 0; i < n; i++) {
        free(generated[i]);
    }
    free((void *)generated);
}

/* ── public API ─────────────────────────────────────────────────────────── */

biosim_status_t biosim_params_parse(biosim_params_t *p, const char *progname, const char *version,
                                    int argc, char **argv) {
    struct arg_lit *arg_help = arg_lit0("h", "help", "print help and exit");
    struct arg_lit *arg_ver = arg_lit0(NULL, "version", "print version and exit");
    struct arg_file *arg_config = arg_file0(NULL, "config", "<path>", "TOML config file");
    void *static_flags[] = {arg_help, arg_ver, arg_config};
    size_t nstatic = sizeof(static_flags) / sizeof(static_flags[0]);

    size_t ndyn = biosim_params_count(p);
    size_t total = nstatic + ndyn + 1;

    void **argtable = (void **)calloc(total, sizeof(void *));
    if (!argtable) {
        return BIOSIM_ERR_NOMEM;
    }

    char **generated = (char **)calloc(ndyn, sizeof(char *));
    if (!generated) {
        free((void *)argtable);
        return BIOSIM_ERR_NOMEM;
    }

    char **glossaries = (char **)calloc(ndyn, sizeof(char *));
    if (!glossaries) {
        free_generated(generated, ndyn);
        free((void *)argtable);
        return BIOSIM_ERR_NOMEM;
    }

    memcpy((void *)argtable, (const void *)static_flags, nstatic * sizeof(void *));
    build_argtable(argtable, nstatic, p, ndyn, generated, glossaries);
    struct arg_end *arg_end_s = arg_end(20);
    argtable[nstatic + ndyn] = arg_end_s;

    int nerrors = arg_parse(argc, argv, argtable);
    biosim_status_t returncode = BIOSIM_OK;
    bool exit_instead_of_return = false;

    if (arg_help->count > 0) {
        print_synopsis(stdout, progname, argtable, nstatic, p, ndyn);
        printf("%s — biosim4-gpu simulator\n\n", progname);
        print_glossary(stdout, argtable, nstatic, p, ndyn);
        exit_instead_of_return = true;
        goto exit;
    }
    if (arg_ver->count > 0) {
        printf("%s %s\n", progname, version);
        exit_instead_of_return = true;
        goto exit;
    }
    if (nerrors > 0) {
        arg_print_errors(stderr, arg_end_s, progname);
        returncode = BIOSIM_ERR_NOTFOUND;
        exit_instead_of_return = true;
        goto exit;
    }

    /* Pass 2: TOML file overrides defaults */
    if (arg_config->count > 0) {
        params_load_toml_file(p, arg_config->filename[0]);
    }

    /* Pass 3: CLI flags override TOML */
    apply_cli_args(argtable, nstatic, p, ndyn);

exit:
    arg_freetable(argtable, total);
    free_generated(generated, ndyn);
    free_generated(glossaries, ndyn);
    free((void *)argtable);
    if (exit_instead_of_return) {
        exit(returncode);
    }
    return returncode;
}
