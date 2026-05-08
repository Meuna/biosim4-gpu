#include <argtable3.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "biosim/core/log.h"
#include "biosim/core/status.h"
#include "biosim/params/params.h"

/* Forward declaration of internal TOML loader defined in toml.c */
biosim_status_t params_load_toml_file(biosim_params_t *p, const char *path);

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

static void collect_table_order(const biosim_params_t *p, size_t ndyn, const char **out_order,
                                size_t *out_count) {
    assert(ndyn <= p->count);
    size_t n = 0;
    for (size_t i = 0; i < ndyn; i++) {
        const char *tname = p->entries[i].table;
        bool found = false;
        for (size_t j = 0; j < n; j++) {
            if (str_eq_nullable(out_order[j], tname)) {
                found = true;
                break;
            }
        }
        if (!found) {
            out_order[n++] = tname;
        }
    }
    *out_count = n;
}

/* Prints the usage one-liner using arg_print_syntax on a filtered shallow copy of argtable. */
static biosim_status_t print_synopsis(FILE *fp, const char *progname, void **argtable,
                                      size_t nstatic, const biosim_params_t *p, size_t ndyn) {
    assert(ndyn <= p->count);
    size_t nsyn = 0;
    for (size_t i = 0; i < ndyn; i++) {
        if (p->entries[i].cli_long != NULL) {
            nsyn++;
        }
    }

    void **syn = (void **)malloc((nstatic + nsyn + 1) * sizeof(void *));
    if (!syn) {
        return BIOSIM_ERR_NOMEM;
    }
    memcpy((void *)syn, (const void *)argtable, nstatic * sizeof(void *));

    size_t k = nstatic;
    for (size_t i = 0; i < ndyn; i++) {
        if (p->entries[i].cli_long != NULL) {
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
    return BIOSIM_OK;
}

/* Prints full glossary using arg_print_glossary, shallow grouped by params table. */
static biosim_status_t print_glossary(FILE *fp, void **argtable, size_t nstatic,
                                      const biosim_params_t *p, size_t ndyn) {
    assert(ndyn <= p->count);
    void **stbl = (void **)malloc((nstatic + 1) * sizeof(void *));
    if (!stbl) {
        return BIOSIM_ERR_NOMEM;
    }

    /* The convention is argtable[:nstatic] are static arguments */
    memcpy((void *)stbl, (const void *)argtable, nstatic * sizeof(void *));
    struct arg_end *stbl_end = arg_end(1);
    stbl[nstatic] = stbl_end;
    arg_print_glossary(fp, stbl, "  %-25s %s\n");
    /* We only free the shallow copy including the dedicated arg_end(1) */
    arg_freetable(&stbl[nstatic], 1);
    free((void *)stbl);

    /* Collect the tables declared by the parameters */
    size_t ntables = 0;
    const char **table_order = NULL;
    if (ndyn > 0) {
        table_order = (const char **)malloc(ndyn * sizeof(const char *));
        if (!table_order) {
            return BIOSIM_ERR_NOMEM;
        }
        collect_table_order(p, ndyn, table_order, &ntables);
    }

    for (size_t t = 0; t < ntables; t++) {
        const char *tname = table_order[t];
        size_t ntable = 0;
        for (size_t i = 0; i < ndyn; i++) {
            if (str_eq_nullable(p->entries[i].table, tname)) {
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
            free((void *)table_order);
            return BIOSIM_ERR_NOMEM;
        }
        size_t k = 0;
        for (size_t i = 0; i < ndyn; i++) {
            if (str_eq_nullable(p->entries[i].table, tname)) {
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

    return BIOSIM_OK;
}

/* Writes formatted default "default: <val>" into out buffer of size n.
 * Returns BIOSIM_OK on success, or BIOSIM_ERR_NOMEM if the output was truncated.
 */
static void format_default_to_buf(const biosim_param_entry_t *e, char *out, size_t n) {
    int needed = 0;
    switch (e->type) {
    case PARAM_INT:
    case PARAM_COUNT:
        (void)snprintf(out, n, "default: %d", e->value.i);
        break;
    case PARAM_FLOAT:
        (void)snprintf(out, n, "default: %g", e->value.f);
        break;
    case PARAM_BOOL:
        (void)snprintf(out, n, "default: %s", e->value.b ? "true" : "false");
        break;
    case PARAM_STRING:
        (void)snprintf(out, n, "default: %s", e->value.s);
        break;
    default:
        if (n > 0) out[0] = '\0';
    }
}

/* Appends argtable3 entries for every param (1-to-1: param i → argtable[nstatic+i]).
 * Auto-generated long flag names are malloc'd into generated[i]; NULL otherwise.
 * Formatted default strings are malloc'd into glossaries[i]; NULL otherwise. */
static biosim_status_t build_argtable(void **argtable, size_t nstatic, const biosim_params_t *p, size_t ndyn,
                           char **generated, char **glossaries) {
    assert(ndyn <= p->count);
    for (size_t i = 0; i < ndyn; i++) {
        const biosim_param_entry_t *e = &p->entries[i];

        generated[i] = NULL;
        glossaries[i] = NULL;

        const char *longflag;
        if (e->cli_long != NULL) {
            longflag = e->cli_long;
        } else if (e->table != NULL) {
            size_t len = strlen(e->table) + 1 + strlen(e->name) + 1;
            generated[i] = (char *)malloc(len);
            if (!generated[i]) {
                return BIOSIM_ERR_NOMEM;
            }
            (void)snprintf(generated[i], len, "%s-%s", e->table, e->name);
            longflag = generated[i];
        } else {
            longflag = e->name;
        }

        const char *glossary = "";
        if (e->has_default) {
            glossaries[i] = (char *)malloc(256);
            if (!glossaries[i]) {
                return BIOSIM_ERR_NOMEM;
            }
            format_default_to_buf(e, glossaries[i], 256);
            glossary = glossaries[i];
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
        case PARAM_COUNT:
            argtable[nstatic + i] = arg_litn(e->cli_short, longflag, 0, 10, glossary);
            break;
        }
    }
    return BIOSIM_OK;
}

/* Reads parsed CLI values back into params (1-to-1: param i → argtable[nstatic+i]). */
static void apply_cli_args(void **argtable, size_t nstatic, biosim_params_t *p, size_t ndyn) {
    assert(ndyn <= p->count);
    for (size_t i = 0; i < ndyn; i++) {
        const biosim_param_entry_t *e = &p->entries[i];
        switch (e->type) {
        case PARAM_INT: {
            struct arg_int *a = argtable[nstatic + i];
            if (a->count > 0) {
                (void)biosim_params_set_int(p, e->name, a->ival[0]);
            }
            break;
        }
        case PARAM_FLOAT: {
            struct arg_dbl *a = argtable[nstatic + i];
            if (a->count > 0) {
                (void)biosim_params_set_float(p, e->name, a->dval[0]);
            }
            break;
        }
        case PARAM_BOOL: {
            struct arg_lit *a = argtable[nstatic + i];
            if (a->count > 0) {
                (void)biosim_params_set_bool(p, e->name, true);
            }
            break;
        }
        case PARAM_STRING: {
            struct arg_str *a = argtable[nstatic + i];
            if (a->count > 0) {
                (void)biosim_params_set_string(p, e->name, a->sval[0]);
            }
            break;
        }
        case PARAM_COUNT: {
            struct arg_lit *a = argtable[nstatic + i];
            if (a->count > 0) {
                (void)biosim_params_set_int(p, e->name, a->count);
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

    size_t ndyn = p->count;
    size_t total = nstatic + ndyn + 1;

    /* alloc start here, free on exit label */
    void **argtable = NULL;
    char **generated = NULL;
    char **glossaries = NULL;
    biosim_status_t returncode = BIOSIM_OK;
    
    argtable = (void **)calloc(total, sizeof(void *));
    if (!argtable) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    generated = (char **)calloc(ndyn, sizeof(char *));
    if (!generated) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    glossaries = (char **)calloc(ndyn, sizeof(char *));
    if (!glossaries) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    memcpy((void *)argtable, (const void *)static_flags, nstatic * sizeof(void *));
    returncode = build_argtable(argtable, nstatic, p, ndyn, generated, glossaries);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }
    struct arg_end *arg_end_s = arg_end(20);
    argtable[nstatic + ndyn] = arg_end_s;

    int nerrors = arg_parse(argc, argv, argtable);
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

    /* Pass 1: TOML file overrides defaults */
    if (arg_config->count > 0) {
        returncode = params_load_toml_file(p, arg_config->filename[0]);
        if (returncode != BIOSIM_OK) {
            goto exit;
        }
        size_t path_len = strlen(arg_config->filename[0]) + 1;
        p->config_path = (char *)malloc(path_len);
        if (!p->config_path) {
            returncode = BIOSIM_ERR_NOMEM;
            goto exit;
        }
        memcpy(p->config_path, arg_config->filename[0], path_len);
    }

    /* Pass 2: CLI flags override TOML */
    apply_cli_args(argtable, nstatic, p, ndyn);

exit:
    arg_freetable(argtable, total);
    free_generated(generated, ndyn);
    free_generated(glossaries, ndyn);
    free((void *)argtable);
    if (exit_instead_of_return) {
        exit(returncode);
    }
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("parameter parsing failed (%s)", biosim_strerror(returncode));
    }
    return returncode;
}
