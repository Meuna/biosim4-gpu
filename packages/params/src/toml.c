#include "biosim/core/log.h"
#include "biosim/params/params.h"
#include "tomlc17.h"

static void apply_table(biosim_params_t *p, toml_datum_t toptab) {
    for (size_t i = 0; i < p->count; i++) {
        const biosim_param_entry_t *e = &p->entries[i];

        toml_datum_t src;
        if (e->table != NULL) {
            toml_datum_t subtab = toml_get(toptab, e->table);
            if (subtab.type != TOML_TABLE) {
                continue;
            }
            src = toml_get(subtab, e->name);
        } else {
            src = toml_get(toptab, e->name);
        }

        switch (e->type) {
        case PARAM_INT:
            if (src.type == TOML_INT64) {
                biosim_params_set_int(p, e->name, (int)src.u.int64);
            }
            break;
        case PARAM_FLOAT:
            if (src.type == TOML_FP64) {
                biosim_params_set_float(p, e->name, src.u.fp64);
            } else if (src.type == TOML_INT64) {
                biosim_params_set_float(p, e->name, (double)src.u.int64);
            }
            break;
        case PARAM_BOOL:
            if (src.type == TOML_BOOLEAN) {
                biosim_params_set_bool(p, e->name, src.u.boolean);
            }
            break;
        case PARAM_STRING:
            if (src.type == TOML_STRING) {
                biosim_params_set_string(p, e->name, src.u.s);
            }
            break;
        case PARAM_COUNT:
            if (src.type == TOML_INT64) {
                biosim_params_set_int(p, e->name, (int)src.u.int64);
            }
            break;
        }
    }
}

biosim_status_t params_load_toml_file(biosim_params_t *p, const char *path) {
    toml_result_t result = toml_parse_file_ex(path);
    if (!result.ok) {
        BIOSIM_ERRORF("failed to parse TOML file '%s' (%s)", path, result.errmsg);
        return BIOSIM_ERR_INVALID;
    }
    apply_table(p, result.toptab);
    toml_free(result);
    return BIOSIM_OK;
}
