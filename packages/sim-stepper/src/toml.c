#include "biosim/stepper/toml.h"
#include "biosim/core/params.h"
#include "tomlc17.h"

static void apply_table(biosim_params_t *p, toml_datum_t toptab) {
    for (size_t i = 0; i < biosim_params_count(p); i++) {
        const biosim_param_entry_t *e = biosim_params_entry(p, i);
        toml_datum_t d = toml_get(toptab, e->name);

        switch (e->type) {
        case PARAM_INT:
            if (d.type == TOML_INT64) {
                biosim_params_set_int(p, e->name, (int)d.u.int64);
            }
            break;
        case PARAM_FLOAT:
            if (d.type == TOML_FP64) {
                biosim_params_set_float(p, e->name, d.u.fp64);
            } else if (d.type == TOML_INT64) {
                biosim_params_set_float(p, e->name, (double)d.u.int64);
            }
            break;
        case PARAM_BOOL:
            if (d.type == TOML_BOOLEAN) {
                biosim_params_set_bool(p, e->name, d.u.boolean);
            }
            break;
        case PARAM_STRING:
            if (d.type == TOML_STRING) {
                biosim_params_set_string(p, e->name, d.u.s);
            }
            break;
        }
    }
}

biosim_status_t stepper_load_toml_file(biosim_params_t *p, const char *path) {
    toml_result_t result = toml_parse_file_ex(path);
    if (!result.ok) {
        return BIOSIM_ERR_NOTFOUND;
    }
    apply_table(p, result.toptab);
    toml_free(result);
    return BIOSIM_OK;
}
