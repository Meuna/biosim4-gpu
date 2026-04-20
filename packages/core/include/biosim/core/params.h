#ifndef BIOSIM_CORE_PARAMS_H
#define BIOSIM_CORE_PARAMS_H

#include "biosim/core/status.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PARAM_INT,
    PARAM_FLOAT,
    PARAM_BOOL,
    PARAM_STRING,
} biosim_param_type_t;

typedef union {
    int i;
    double f;
    bool b;
    const char *s;
} biosim_param_value_t;

typedef struct {
    const char *name;
    biosim_param_value_t default_value;
    biosim_param_value_t value;
    biosim_param_type_t type;
    bool is_set;
} biosim_param_entry_t;

typedef struct {
    biosim_param_entry_t *entries;
    size_t count;
    size_t capacity;
} biosim_params_t;

/* Lifecycle */
biosim_status_t biosim_params_init(biosim_params_t *p);
biosim_status_t biosim_params_extend(biosim_params_t *p, const biosim_param_entry_t *extras,
                                     size_t count);
void biosim_params_free(biosim_params_t *p);

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
size_t biosim_params_count(const biosim_params_t *p);
const biosim_param_entry_t *biosim_params_entry(const biosim_params_t *p, size_t index);

#endif /* BIOSIM_CORE_PARAMS_H */
