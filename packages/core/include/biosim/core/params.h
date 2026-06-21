#ifndef BIOSIM_CORE_PARAMS_H
#define BIOSIM_CORE_PARAMS_H

#include "biosim/core/status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    PARAM_INT,
    PARAM_FLOAT,
    PARAM_BOOL,
    PARAM_STRING,
    PARAM_COUNT,
} biosim_param_type_t;

typedef union {
    int i;
    double f;
    bool b;
    const char *s;
} biosim_param_value_t;

typedef struct {
    const char *name;
    const char *table; /* NULL = top-level TOML key; non-NULL = [table] section */
    biosim_param_value_t value;
    biosim_param_type_t type;
    bool is_set;
    bool has_default;      /* true = value holds the default to display in --help */
    const char *cli_long;  /* NULL = auto ({table}-{name} or {name}); else override */
    const char *cli_short; /* NULL = no short flag; else single-char string e.g. "p" */
} biosim_param_entry_t;

typedef struct {
    biosim_param_entry_t *entries;
    size_t count;
    size_t capacity;
    char *config_path; /* heap copy of --config path, or NULL if none */
} biosim_params_t;

/* Lifecycle */
biosim_status_t biosim_params_init(
    biosim_params_t *p, const biosim_param_entry_t *entries, size_t count
);
void biosim_params_free(biosim_params_t *p);

/* Three-pass parsing: defaults (already in entries) → TOML (--config) → CLI flags.
 * progname: argv[0] shown in --help / --version output.
 * version:  version string shown in --version output.                        */
biosim_status_t biosim_params_parse(
    biosim_params_t *p, const char *progname, const char *version, int argc, char **argv
);

/* Setters — write value and set is_set = true */
biosim_status_t biosim_params_set_int(biosim_params_t *p, const char *key, int val);
biosim_status_t biosim_params_set_float(biosim_params_t *p, const char *key, double val);
biosim_status_t biosim_params_set_bool(biosim_params_t *p, const char *key, bool val);
biosim_status_t biosim_params_set_string(biosim_params_t *p, const char *key, const char *val);

/* Getters — abort on type mismatch (programming error) */
int biosim_params_get_int(const biosim_params_t *p, const char *key);
double biosim_params_get_float(const biosim_params_t *p, const char *key);
bool biosim_params_get_bool(const biosim_params_t *p, const char *key);
const char *biosim_params_get_string(const biosim_params_t *p, const char *key);

/* Introspection */
const biosim_param_entry_t *biosim_params_find(const biosim_params_t *p, const char *key);

/* Print every registered param as "[table] name = value" (one per line), with
 * the value rendered per its type. Reusable for startup info dumps. */
void biosim_params_dump(const biosim_params_t *p, FILE *out);

/* Print a compact info card: pop, grid-size, max-generations, max-genes,
 * max-neurons, challenge kind, and barrier count. */
void biosim_params_print_info_card(const biosim_params_t *p, uint32_t n_barriers, FILE *out);

#endif /* BIOSIM_CORE_PARAMS_H */
