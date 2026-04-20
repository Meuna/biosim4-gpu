#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static char *str_dup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

#include "biosim/core/params.h"

#define INITIAL_CAPACITY 16

/* ── internal helpers ───────────────────────────────────────────────────── */

static biosim_status_t params_grow(biosim_params_t *p, size_t needed) {
    if (needed <= p->capacity)
        return BIOSIM_OK;
    size_t cap = p->capacity ? p->capacity * 2 : INITIAL_CAPACITY;
    while (cap < needed)
        cap *= 2;
    biosim_param_entry_t *buf = realloc(p->entries, cap * sizeof(*buf));
    if (!buf)
        return BIOSIM_ERR_NOMEM;
    p->entries  = buf;
    p->capacity = cap;
    return BIOSIM_OK;
}

static biosim_param_entry_t *find_mutable(biosim_params_t *p, const char *key) {
    for (size_t i = 0; i < p->count; i++)
        if (strcmp(p->entries[i].name, key) == 0)
            return &p->entries[i];
    return NULL;
}

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_params_init(biosim_params_t *p) {
    p->entries  = NULL;
    p->count    = 0;
    p->capacity = 0;

    static const struct {
        const char          *name;
        biosim_param_type_t  type;
        biosim_param_value_t def;
    } defaults[] = {
        {"population",         PARAM_INT,   {.i = 3000}  },
        {"size-x",             PARAM_INT,   {.i = 128}   },
        {"size-y",             PARAM_INT,   {.i = 128}   },
        {"steps-per-gen",      PARAM_INT,   {.i = 300}   },
        {"max-genome-length",  PARAM_INT,   {.i = 24}    },
        {"mutation-rate",      PARAM_FLOAT, {.f = 0.001} },
        {"challenge",          PARAM_INT,   {.i = 0}     },
    };
    size_t n = sizeof(defaults) / sizeof(defaults[0]);

    biosim_status_t st = params_grow(p, n);
    if (st != BIOSIM_OK)
        return st;

    for (size_t i = 0; i < n; i++) {
        biosim_param_entry_t *e = &p->entries[p->count++];
        e->name          = defaults[i].name;
        e->type          = defaults[i].type;
        e->default_value = defaults[i].def;
        e->value         = defaults[i].def;
        e->is_set        = false;
    }
    return BIOSIM_OK;
}

biosim_status_t biosim_params_extend(biosim_params_t *p,
                                     const biosim_param_entry_t *extras,
                                     size_t count) {
    biosim_status_t st = params_grow(p, p->count + count);
    if (st != BIOSIM_OK)
        return st;
    for (size_t i = 0; i < count; i++) {
        biosim_param_entry_t *e = &p->entries[p->count++];
        *e        = extras[i];
        e->value  = extras[i].default_value;
        e->is_set = false;
    }
    return BIOSIM_OK;
}

void biosim_params_free(biosim_params_t *p) {
    for (size_t i = 0; i < p->count; i++) {
        biosim_param_entry_t *e = &p->entries[i];
        if (e->type == PARAM_STRING && e->is_set)
            free((void *)e->value.s);
    }
    free(p->entries);
    p->entries  = NULL;
    p->count    = 0;
    p->capacity = 0;
}

/* ── setters ────────────────────────────────────────────────────────────── */

biosim_status_t biosim_params_set_int(biosim_params_t *p,
                                      const char *key, int val) {
    biosim_param_entry_t *e = find_mutable(p, key);
    if (!e)
        return BIOSIM_WARN_UNKNOWN_KEY;
    if (e->type != PARAM_INT)
        return BIOSIM_ERR_TYPE;
    e->value.i = val;
    e->is_set  = true;
    return BIOSIM_OK;
}

biosim_status_t biosim_params_set_float(biosim_params_t *p,
                                        const char *key, double val) {
    biosim_param_entry_t *e = find_mutable(p, key);
    if (!e)
        return BIOSIM_WARN_UNKNOWN_KEY;
    if (e->type != PARAM_FLOAT)
        return BIOSIM_ERR_TYPE;
    e->value.f = val;
    e->is_set  = true;
    return BIOSIM_OK;
}

biosim_status_t biosim_params_set_bool(biosim_params_t *p,
                                       const char *key, bool val) {
    biosim_param_entry_t *e = find_mutable(p, key);
    if (!e)
        return BIOSIM_WARN_UNKNOWN_KEY;
    if (e->type != PARAM_BOOL)
        return BIOSIM_ERR_TYPE;
    e->value.b = val;
    e->is_set  = true;
    return BIOSIM_OK;
}

biosim_status_t biosim_params_set_string(biosim_params_t *p,
                                         const char *key, const char *val) {
    biosim_param_entry_t *e = find_mutable(p, key);
    if (!e)
        return BIOSIM_WARN_UNKNOWN_KEY;
    if (e->type != PARAM_STRING)
        return BIOSIM_ERR_TYPE;
    if (e->is_set)
        free((void *)e->value.s);
    char *copy = str_dup(val);
    if (!copy)
        return BIOSIM_ERR_NOMEM;
    e->value.s = copy;
    e->is_set  = true;
    return BIOSIM_OK;
}

/* ── getters ────────────────────────────────────────────────────────────── */

int biosim_params_get_int(const biosim_params_t *p, const char *key) {
    const biosim_param_entry_t *e = biosim_params_find(p, key);
    assert(e && e->type == PARAM_INT);
    return e->value.i;
}

double biosim_params_get_float(const biosim_params_t *p, const char *key) {
    const biosim_param_entry_t *e = biosim_params_find(p, key);
    assert(e && e->type == PARAM_FLOAT);
    return e->value.f;
}

bool biosim_params_get_bool(const biosim_params_t *p, const char *key) {
    const biosim_param_entry_t *e = biosim_params_find(p, key);
    assert(e && e->type == PARAM_BOOL);
    return e->value.b;
}

const char *biosim_params_get_string(const biosim_params_t *p, const char *key) {
    const biosim_param_entry_t *e = biosim_params_find(p, key);
    assert(e && e->type == PARAM_STRING);
    return e->value.s;
}

/* ── introspection ──────────────────────────────────────────────────────── */

const biosim_param_entry_t *biosim_params_find(const biosim_params_t *p,
                                               const char *key) {
    for (size_t i = 0; i < p->count; i++)
        if (strcmp(p->entries[i].name, key) == 0)
            return &p->entries[i];
    return NULL;
}

size_t biosim_params_count(const biosim_params_t *p) {
    return p->count;
}

const biosim_param_entry_t *biosim_params_entry(const biosim_params_t *p,
                                                size_t index) {
    assert(index < p->count);
    return &p->entries[index];
}
