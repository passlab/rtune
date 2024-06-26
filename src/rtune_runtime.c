#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include "rtune_runtime.h"

/**
 * This is not thread-safe
 */
rtune_region_t rtune_regions[MAX_NUM_REGIONS]; //assume all global variable/mem are initialized 0, not thread-safe
int num_regions;

/**
 * @brief Initialize a rtune region
 * 
 * @param name 
 * @return rtune_region_t* 
 */
rtune_region_t *rtune_region_init(char *name) {
    if (name == NULL) {
        return NULL; //a name must be provided;
    }
    int i;
    for (i = 0; i < MAX_NUM_REGIONS; i++) {
        rtune_region_t *region = &rtune_regions[i]; //not thread safe
        if (region->name == NULL) {
            memset(region, 0, sizeof(rtune_region_t));
            region->name = name;
            region->count = -1;
            region->num_vars = 0;
            region->num_objs = 0;
            region->num_retired_objs = 0;
            region->status = RTUNE_STATUS_CREATED;
            num_regions++;
            return region;
        }
    }
    return NULL;
}

void rtune_region_fini(rtune_region_t *region) {
    region->name = NULL;
    num_regions--; //not thread-safe
}

inline static utype_t rtune_stvar_get_value(stvar_t *stvar, int index) {
	utype_t v;
	switch (stvar->type) {
	case RTUNE_short:
		v = (utype_t)((short*) stvar->states)[index];
		break;
	case RTUNE_int:
		v = (utype_t)((int*) stvar->states)[index];
		break;
	case RTUNE_long:
		v = (utype_t)((long*) stvar->states)[index];
		break;
	case RTUNE_float:
		v = (utype_t)((float*) stvar->states)[index];
		break;
	case RTUNE_double:
		v = (utype_t)((double*) stvar->states)[index];
		break;
	default:
		v = (utype_t)((void**) stvar->states)[index];
		break;
	}
	return v;
}

utype_t rtune_var_get_value(rtune_var_t * var, int index) {
	return rtune_stvar_get_value(&(var->stvar), index);
}

utype_t rtune_func_get_value(rtune_func_t * func, int index) {
	return rtune_stvar_get_value(&(func->stvar), index);
}

static void * rtune_malloc_4_states(stvar_t *stvar) {
    //allocate memory for storing the state of the variable
    int var_size = 2;
    switch (stvar->type) {
        case RTUNE_short:
            var_size = sizeof(short); break;
        case RTUNE_int:
            var_size = sizeof(int); break;
        case RTUNE_long:
            var_size = sizeof(long); break;
        case RTUNE_float:
            var_size = sizeof(float); break;
        case RTUNE_double:
            var_size = sizeof(double); break;
        case RTUNE_void:
            var_size = sizeof(void *); break;
        default:
            var_size = sizeof(void *); break;
    }
    stvar->states = (void *)malloc(stvar->total_num_states * var_size);

    return stvar->states;
}

void *rtune_var_add_list(rtune_region_t *region, char *name, int total_num_states, rtune_data_type_t type, int num_values, void *values, char **valname) {
    int i = region->num_vars;
    rtune_var_t *var = &region->vars[i];
    var->num_unique_values = num_values;
    var->current_v_index = -1;
    var->kind = RTUNE_VAR_LIST;
    var->status = RTUNE_STATUS_CREATED;
    var->list_range_setting.list.list_values = values;
    var->list_range_setting.list.list_names = valname;

    var->update_lt = DEFAULT_VAR_update_lt;
    var->update_policy = RTUNE_UPDATE_LIST_SERIES;
    var->update_iteration_start = DEFAULT_update_iteration_start;
    var->batch_size = DEFAULT_batch_size;
    var->update_iteration_stride = DEFAULT_update_iteration_stride;
    var->apply_policy = DEFAULT_VAR_apply_policy;

    stvar_t *stvar = &var->stvar;
    stvar->name = name;
    stvar->type = type;
    stvar->num_states = 0;
    //allocate memory for both states and count_value array
    stvar->total_num_states = total_num_states;
    var->count_value = (int*) malloc(sizeof(int) * var->num_unique_values);
    rtune_malloc_4_states(stvar);

    //no need to initialize other fields since they are memset as 0 when the region is initialized

    region->num_vars++;
    return (int *)var; //since this is the first field, it has the same address of the object itself (var or stvar)
}

#define RTUNE_VAR_ADD_RANGE(TYPE, rangeBegin, rangeEnd, step) \
    var->num_unique_values = (int)(fabs((double)(*((TYPE*)rangeEnd)) - (double)(*((TYPE*)rangeBegin))) / fabs((double)(*((TYPE*)step)))) + 1;\
    var->list_range_setting.range.rangeBegin._##TYPE##_value = *(TYPE*)rangeBegin; \
    var->list_range_setting.range.step._##TYPE##_value = *(TYPE*)step; \
    var->list_range_setting.range.rangeEnd._##TYPE##_value = *(TYPE*)rangeEnd;

void *rtune_var_add_range(rtune_region_t *region, char *name, int total_num_states, rtune_data_type_t type, void *rangeBegin, void *rangeEnd, void *step) {
    int i = region->num_vars;
    rtune_var_t *var = &region->vars[i];
    var->current_v_index = -1;
    var->kind = RTUNE_VAR_RANGE;
    var->status = RTUNE_STATUS_CREATED;
    //calculate the number of unique values, use the longest number (double) since they are all casted. This should work for short, int, float, long, double, etc.
    switch (type) {
        case RTUNE_short:
            RTUNE_VAR_ADD_RANGE(short, rangeBegin, rangeEnd, step); break;
        case RTUNE_int:
            RTUNE_VAR_ADD_RANGE(int, rangeBegin, rangeEnd, step); break;
        case RTUNE_long:
            RTUNE_VAR_ADD_RANGE(long, rangeBegin, rangeEnd, step); break;
        case RTUNE_float:
            RTUNE_VAR_ADD_RANGE(float, rangeBegin, rangeEnd, step); break;
        case RTUNE_double:
            RTUNE_VAR_ADD_RANGE(double, rangeBegin, rangeEnd, step); break;
        default:
            //error
            break;
    }

    var->update_lt = DEFAULT_VAR_update_lt;
    var->update_policy = RTUNE_UPDATE_LIST_SERIES;
    var->update_iteration_start = DEFAULT_update_iteration_start;
    var->batch_size = DEFAULT_batch_size;
    var->update_iteration_stride = DEFAULT_update_iteration_stride;
    var->apply_policy = DEFAULT_VAR_apply_policy;

    stvar_t *stvar = &var->stvar;
    stvar->name = name;
    stvar->type = type;
    stvar->num_states = 0;
    //allocate memory for both states and count_value array
    var->count_value = (int*) malloc(sizeof(int) * var->num_unique_values);
    stvar->total_num_states = total_num_states;
    rtune_malloc_4_states(stvar);

    //no need to initialize other fields since they are memset as 0 when the region is initialized

    region->num_vars++;
    return (int *)var; //since this is the first field, it has the same address of the object itself (var or stvar)
}


void *rtune_var_add_ext(rtune_region_t *region, char *name, int total_num_states, rtune_data_type_t type, void *(*provider)(void *), void *provider_arg) {
    int i = region->num_vars;
    rtune_var_t *var = &region->vars[i];
    var->num_unique_values = 0;
    var->kind = RTUNE_VAR_EXT;
    var->status = RTUNE_STATUS_CREATED;

    var->update_lt = DEFAULT_VAR_update_lt;
    var->update_policy = RTUNE_UPDATE_BATCH_STRAIGHT;
    var->update_iteration_start = DEFAULT_update_iteration_start;
    var->batch_size = DEFAULT_batch_size;
    var->update_iteration_stride = DEFAULT_update_iteration_stride;
    var->apply_policy = DEFAULT_VAR_apply_policy;

    stvar_t *stvar = &var->stvar;
    stvar->provider = provider;
    stvar->provider_arg = provider_arg;
    stvar->name = name;
    stvar->type = type;
    stvar->num_states = 0;
    //allocate memory for both states and count_value array
    stvar->total_num_states = total_num_states;
    rtune_malloc_4_states(stvar);

    //no need to initialize other fields since they are memset as 0 when the region is initialized

    region->num_vars++;
    return (int *)var; //since this is the first field, it has the same address of the object itself (var or stvar)
}

#if USING_VAR_EXT_DIFF_IS_USEFUL
void *rtune_var_add_ext_diff(rtune_region_t *region, char *name, int total_num_states, rtune_data_type_t type, void *(*provider)(void *), void *provider_arg) {
    int i = region->num_vars;
    rtune_var_t *var = &region->vars[i];
    var->kind = RTUNE_VAR_EXT_DIFF;
    stvar_t *stvar = &var->stvar;
    stvar->provider = provider;
    stvar->provider_arg = provider_arg;
    stvar->name = name;
    stvar->type = type;
    stvar->num_states = 0;
    //allocate memory for both states and count_value array
    stvar->total_num_states = total_num_states;
    rtune_malloc_4_states(stvar);

    //no need to initialize other fields since they are memset as 0 when the region is initialized

    region->num_vars++;
    return (int *)var; //since this is the first field, it has the same address of the object itself (var or stvar)
}
#endif
/**
 * @brief to set the applier and policy of the var. The applier is called if the var needs to be applied 
 * @param var 
 * @param applier 
 * @param apply_policy 
 */
void  rtune_var_set_applier_policy(rtune_var_t *var, void (*applier) (void *), rtune_var_apply_policy_t apply_policy) {
    var->stvar.applier = applier;
    var->apply_policy = apply_policy;
}

void  rtune_var_set_applier(rtune_var_t * var, void (*applier) (void *)) {
    var->stvar.applier = applier;
}

/**
 * @brief set the apply policy for the variables in each iteration 
 * 
 * @param var 
 * @param apply_policy 
 */
void  rtune_var_set_apply_policy(rtune_var_t * var, rtune_var_apply_policy_t apply_policy) {
    var->apply_policy = apply_policy;
}

/**
 *
 * @param var
 * @param count the iteration count of the region_begin
 */
void rtune_var_print_list_range(rtune_var_t * var, int count) {
    stvar_t *stvar = &var->stvar;
    int num_values = var->num_unique_values;
    int num_states = stvar->num_states;

    switch (var->kind) {
        case RTUNE_VAR_LIST:
        case RTUNE_VAR_RANGE:
            printf("%d: var %s[%d]: ", count, stvar->name, num_states);
            break;
        case RTUNE_VAR_EXT:
        case RTUNE_VAR_EXT_DIFF:
        default:
            break;
    }
    int i;
    for (i = 0; i < num_states; i++) {
        switch (stvar->type) {
            case RTUNE_short:
                printf("%d", ((short*)(stvar->states))[i]);
                break;
            case RTUNE_int:
                printf("%d", ((int*)(stvar->states))[i]);
                break;
            case RTUNE_long:
                printf("%ld", ((long*)(stvar->states))[i]);
                break;
            case RTUNE_float:
                printf("%f", ((float*)(stvar->states))[i]);
                break;
            case RTUNE_double:
                printf("%f", ((double*)(stvar->states))[i]);
                break;
            default:
                //error
                break;
        }
        if (i != num_states - 2) printf(", ");
    }

    printf("\n");
}

/**
 * Set the update attribute of this variable.
 * @param var
 * @param num_states
 * @param update_lt
 * @param update_policy the update policy for the variable. For ext and diff variable, only batch update policy should be specified
 *                      For list and range variable, only RTUNE_UPDATE_LIST_* should be specified.
 * @param update_iteration_start
 * @param batch_size
 * @param update_iteration_stride
 */
void rtune_var_set_update_schedule_attr(rtune_var_t *var, rtune_var_update_kind_t update_lt,
                               rtune_var_update_kind_t update_policy, int update_iteration_start, int batch_size, int update_iteration_stride) {
    stvar_t *stvar = &var->stvar;

    var->update_lt = update_lt;
    var->update_policy = update_policy;
    var->update_iteration_start = update_iteration_start;
    var->batch_size = batch_size;
    var->update_iteration_stride = update_iteration_stride;
}

void rtune_func_set_update_schedule_attr(rtune_func_t *func, rtune_var_update_kind_t update_lt,
                                        rtune_var_update_kind_t update_policy, int update_iteration_start, int batch_size, int update_iteration_stride) {
    func->update_lt = update_lt;
    func->update_policy = update_policy;
    func->update_iteration_start = update_iteration_start;
    func->batch_size = batch_size;
    func->update_iteration_stride = update_iteration_stride;
}


/**
 * This func check whether the update schedules for the variables of a func are properly arranged. For a function, its var
 * should be evaluated one by one, i.e. schedules of any two variables cannot be overlapped. We only check the update_iteration_start,
 * update_batch and stride.
 * @param func
 *
 * @return 0: safe schedule. 1: unsafe schedule (may still be ok)
 */
int rtune_func_schedule_check(rtune_func_t * func) {
    stvar_t * stvar = &func->stvar;
    int i;
    int last_var_end = -1;
    int safe_schedule = 0;
    for (i=0; i<func->num_vars; i++) {
        rtune_var_t * var = func->input_vars[i];
        int start = var->update_iteration_start;
        if (start < last_var_end) { //overlapping schedule
            printf("The update schedule of var[%d] (%s, %p) overlap with the last var[%d]\n", i, var->stvar.name, var, i-1);
            safe_schedule = 1;
        }
        int total_num_states = var->stvar.total_num_states;
        int num_batches = (var->update_lt == RTUNE_UPDATE_REGION_BEGIN_END) ? (total_num_states+1)/2 : total_num_states;
        last_var_end = start + num_batches * (var->batch_size + var->update_iteration_stride) - var->update_iteration_stride; //calculate the last iteration count for this var;
    }
    return safe_schedule;
}

/**
 * @brief add a function that has known function operation to a region. The total number of states of the function is
 * calculated as the product of the total number of states of all input variables. This requires that the total number of
 * state is calculated/set before calling this function.
 * 
 * @param region 
 * @param func_kind 
 * @param name 
 * @param type the data type of the function
 * @param num_vars number of variables for this function
 * @param num_coefficient number of coefficient for this function
 * @param ... var and coefficient for the function, variables must be listed first and then coefficient.
 *            var should be listed in the order it is being enumerated and evaluated
 * @return void* the pointer to the rtune_func_t type
 */
void *rtune_func_add(rtune_region_t *region, rtune_kind_t kind, char *name, rtune_data_type_t type,
                     int num_vars, int num_coefs, ...) {
    int index = region->num_funcs;
    rtune_func_t *func = &region->funcs[index];

    stvar_t *stvar = &func->stvar;
    stvar->name = name;
    stvar->type = type;
    func->kind = kind;
    func->status = RTUNE_STATUS_CREATED;
    func->num_vars = num_vars;
    func->num_coefs = num_coefs;

    //set the default update time/policy/iteration_start/batch/stride
    func->update_lt = DEFAULT_FUNC_update_lt;
    func->update_policy = DEFAULT_update_policy;
    func->update_iteration_start = DEFAULT_update_iteration_start;
    func->batch_size = DEFAULT_batch_size;
    func->update_iteration_stride = DEFAULT_update_iteration_stride;

    func->unused_updates = 0;

    int i;
    va_list args;
    va_start(args, num_coefs);
    int total_num_states = 1;
    for (i = 0; i < num_vars; i++) {
        rtune_var_t *var = va_arg(args, rtune_var_t *);
        var->usedByFuncs[var->num_uses++] = func; //set the var dependency link
        total_num_states *= var->stvar.total_num_states; //The number of states of the func is the products of the states of all input vars
        func->input_vars[i] = var;
    }
    for (i=0; i < num_coefs; i++) {
        func->input_coefs[i] = (utype_t) va_arg(args, utype_t);
    }
    va_end(args);
    stvar->total_num_states = total_num_states;
    rtune_func_schedule_check(func);
    rtune_malloc_4_states(stvar); //allocate memory for storing states
    //allocate memory for storing input of variables, which are represented by the index of the state of the variable
    func->input = (int*) malloc(sizeof(int) * total_num_states * num_vars);
    region->num_funcs++;
    return func;
}

/**
 * Add a function that needs to be modeled. The function data is provided by the provider/provider_arg and the input vars
 * are specified, but no coefficient. A function value is updated with the same update init/batch/stride iterations
 * as its input variables.
 * @param region
 * @param func_kind should be either RTUNE_FUNC_MODEL or RTUNE_FUNC_MODEL_DIFF
 * @param name
 * @param type
 * @param provider
 * @param provider_arg
 * @param num_vars
 * @param ... variables of the func
 * @return
 */
rtune_func_t* rtune_func_add_model(rtune_region_t * region, rtune_kind_t kind, char * name, rtune_data_type_t type,
                           void *(*provider) (void *), void * provider_arg, int num_vars, ...) {
    int index = region->num_funcs;
    rtune_func_t *func = &region->funcs[index];

    stvar_t *stvar = &func->stvar;
    stvar->name = name;
    stvar->type = type;
    func->kind = kind; //must be either RTUNE_FUNC_MODEL or RTUNE_FUNC_MODEL_DIFF
    func->status = RTUNE_STATUS_CREATED;
    stvar->provider = provider;
    stvar->provider_arg = provider_arg;
    func->num_vars = num_vars;
    func->num_coefs = 0;

    //set the default update time/policy/iteration_start/batch/stride
    func->update_lt = DEFAULT_FUNC_update_lt;
    func->update_policy = DEFAULT_update_policy;
    func->update_iteration_start = DEFAULT_update_iteration_start;
    func->batch_size = DEFAULT_batch_size;
    func->update_iteration_stride = DEFAULT_update_iteration_stride;
    func->unused_updates = 0;

    int i;
    va_list args;
    va_start(args, num_vars);
    int total_num_states = 1;
    for (i = 0; i < num_vars; i++) {
        rtune_var_t *var = va_arg(args, rtune_var_t *);
        var->usedByFuncs[var->num_uses++] = func; //set the var dependency link
        total_num_states *= var->stvar.total_num_states; //The number of states of the func is the products of the states of all input vars
        func->input_vars[i] = var;
    }
    va_end(args);
    stvar->total_num_states = total_num_states;
    rtune_func_schedule_check(func);
    rtune_malloc_4_states(stvar); //allocate memory for storing states
    //allocate memory for storing input of variables, which are represented by the index of the state of the variable
    func->input = (int*) malloc(sizeof(int) * total_num_states * num_vars);
    region->num_funcs++;
    return func;
}

void *rtune_func_add_log(rtune_region_t *region, char *name, rtune_data_type_t type, void *var) {
    return rtune_func_add(region, RTUNE_FUNC_LOG, name, type, 1, 0, var);
}

void *rtune_func_add_abs(rtune_region_t *region, char *name, rtune_data_type_t type, void *var) {
    return rtune_func_add(region, RTUNE_FUNC_ABS, name, type, 1, 0, var);
}

/* This variable is a bin variable, if var < threshold, its value is 0, otherwise, its value is 1 */
void *rtune_func_add_threshold(rtune_region_t *region, char *name, rtune_data_type_t type, void *var, void *threshold) {
    return rtune_func_add(region, RTUNE_FUNC_THRESHOLD, name, type, 1, 1, var, threshold);
}
/* This variable is distance variable, whose value is var - target */
void *rtune_func_add_distance(rtune_region_t *region, char *name, rtune_data_type_t type, void *var, void *target) {
    return rtune_func_add(region, RTUNE_FUNC_DISTANCE, name, type, 1, 1, var, target);
}

void *rtune_func_add_gradient(rtune_region_t *region, char *name, rtune_data_type_t type, void *var) {
    return rtune_func_add(region, RTUNE_FUNC_GRADIENT, name, type, 1, 0, var);
}

void set_max(utype_t * v, rtune_data_type_t type) {
    switch (type) {
        case RTUNE_short:
            v->_short_value = SHRT_MAX; break;
        case RTUNE_int:
            v->_int_value = INT_MAX; break;
        case RTUNE_long:
            v->_long_value = LONG_MAX; break;
        case RTUNE_float:
            v->_float_value = FLT_MAX; break;
        case RTUNE_double:
            v->_double_value = DBL_MAX; break;
        default:
            //error
            break;
    }
}

void set_min(utype_t * v, rtune_data_type_t type) {
    switch (type) {
        case RTUNE_short:
            v->_short_value = SHRT_MIN; break;
        case RTUNE_int:
            v->_int_value = INT_MIN; break;
        case RTUNE_long:
            v->_long_value = LONG_MIN; break;
        case RTUNE_float:
            v->_float_value = FLT_MIN; break;
        case RTUNE_double:
            v->_double_value = DBL_MIN; break;
        default:
            //error
            break;
    }
}

/**
 * collect all the independent var of the functions of the obj so they can be used easier later on
 * @param obj
 */
static int rtune_objective_collect_vars(rtune_objective_t * obj) {
    int i;
    int usage_count[MAX_NUM_VARS] = {0};
    int num_vars = 0;
    for (i=0; i<obj->num_funcs; i++) {
        rtune_func_t *func = obj->input_funcs[i].func;
        int j;
        for (j=0; j<func->num_vars; j++) {
            rtune_var_t * var = func->input_vars[j];
            int k=0;
            while(k<num_vars && var != obj->input_vars[k].var) k++;
            if (k<num_vars) {
                usage_count[k]++;
                printf("var %p(%s) is used more than once (%d times) for the obj %p(%s) via func %p(%s)\n", (void*)var, var->stvar.name, usage_count[k], obj, obj->name, func, func->stvar.name);
            } else { // (k==num_vars)
                obj->input_vars[k].var = var;
                obj->input_vars[k].index = -1;
                obj->input_vars[k].preference_right = 1;
                obj->input_vars[k].last_iteration_applied = -1;
                obj->input_vars[k].apply_policy = RTUNE_VAR_APPLY_ON_READ;
                obj->input_vars[k].metaction = RTUNE_METACTION_CONFIG;
                num_vars++;
                usage_count[k] = 1;
            }
        }
    }
    obj->num_vars = num_vars;
    return num_vars;
}

rtune_objective_t *rtune_objective_add_min(rtune_region_t *region, char *name, rtune_func_t *func) {
    int i = region->num_objs;
    rtune_objective_t *obj = &region->objs[i];
    obj->name = name;
    obj->status = RTUNE_STATUS_CREATED;
    region->num_objs++;
    obj->deviation_tolerance = DEFAULT_deviation_tolerance;
    obj->fidelity_window = DEFAULT_fidelity_window;
    obj->lookup_window = DEFAULT_lookup_window;
    obj->search_strategy = RTUNE_OBJECTIVE_SEARCH_DEFAULT;
    obj->max_num_mets = 1;
    obj->num_mets = 0;
    obj->metaction = RTUNE_METACTION_NOACTION;

    func->objectives[func->num_objs++] = obj;

    obj->num_funcs = 1;
    obj->input_funcs[0].func = func;
    obj->input_funcs[0].metaction = RTUNE_METACTION_NOACTION;

    obj->kind = RTUNE_OBJECTIVE_MIN;
    rtune_objective_collect_vars(obj);
    set_max(&(obj->input_funcs[0].value), func->stvar.type); obj->input_vars[0].index = -1;
    return obj;
}

rtune_objective_t *rtune_objective_add_max(rtune_region_t *region, char *name, rtune_func_t *func) {
    int i = region->num_objs;
    rtune_objective_t *obj = &region->objs[i];
    obj->name = name;
    obj->status = RTUNE_STATUS_CREATED;
    region->num_objs++;
    obj->deviation_tolerance = DEFAULT_deviation_tolerance;
    obj->fidelity_window = DEFAULT_fidelity_window;
    obj->lookup_window = DEFAULT_lookup_window;
    obj->search_strategy = RTUNE_OBJECTIVE_SEARCH_DEFAULT;
    obj->max_num_mets = 1;
    obj->num_mets = 0;
    obj->metaction = RTUNE_METACTION_NOACTION;

    func->objectives[func->num_objs++] = obj;

    obj->num_funcs = 1;
    obj->input_funcs[0].func = func;
    obj->input_funcs[0].metaction = RTUNE_METACTION_NOACTION;

    obj->kind = RTUNE_OBJECTIVE_MAX;
    rtune_objective_collect_vars(obj);
    set_min(&(obj->input_funcs[0].value), func->stvar.type); obj->input_vars[0].index = -1;
    return obj;
}

rtune_objective_t *rtune_objective_add_intersection(rtune_region_t *region, char *name, rtune_func_t *model1, rtune_func_t *model2) {
    int i = region->num_objs;
    rtune_objective_t *obj = &region->objs[i];
    obj->name = name;
    obj->status = RTUNE_STATUS_CREATED;
    region->num_objs++;
    obj->deviation_tolerance = DEFAULT_deviation_tolerance;
    obj->fidelity_window = DEFAULT_fidelity_window;
    obj->lookup_window = DEFAULT_lookup_window;
    obj->search_strategy = RTUNE_OBJECTIVE_SEARCH_DEFAULT;
    obj->max_num_mets = 1;
    obj->num_mets = 0;
    obj->metaction = RTUNE_METACTION_NOACTION;

    model1->objectives[model1->num_objs++] = obj;
    model2->objectives[model2->num_objs++] = obj;

    obj->num_funcs = 2;
    obj->input_funcs[0].func = model1;
    obj->input_funcs[0].metaction = RTUNE_METACTION_NOACTION;
    obj->input_funcs[1].func = model2;
    obj->input_funcs[1].metaction = RTUNE_METACTION_NOACTION;

    obj->kind = RTUNE_OBJECTIVE_INTERSECTION;
    rtune_objective_collect_vars(obj);
    return obj;
}

/** the purpose of selecting which model to use is dependent on the select
 *  Work in progress
 */
rtune_objective_t *rtune_objective_add_select2(rtune_region_t *region, char *name, rtune_objective_kind_t select_kind, rtune_func_t *model1, rtune_func_t *model2, int *model1_select, int *model2_select) {
    if (select_kind != RTUNE_OBJECTIVE_SELECT_MIN && select_kind != RTUNE_OBJECTIVE_SEELCT_MAX) return NULL;
    int i = region->num_objs;
    rtune_objective_t *obj = &region->objs[i];
    obj->name = name;
    obj->status = RTUNE_STATUS_CREATED;
    region->num_objs++;
    obj->deviation_tolerance = DEFAULT_deviation_tolerance;
    obj->fidelity_window = DEFAULT_fidelity_window;
    obj->lookup_window = DEFAULT_lookup_window;
    obj->search_strategy = RTUNE_OBJECTIVE_SEARCH_DEFAULT;
    obj->max_num_mets = 1;
    obj->num_mets = 0;
    obj->metaction = RTUNE_METACTION_NOACTION;

    model1->objectives[model1->num_objs++] = obj;
    model2->objectives[model2->num_objs++] = obj;

    obj->num_funcs = 2;
    obj->input_funcs[0].func = model1;
    obj->input_funcs[0].metaction = RTUNE_METACTION_NOACTION;
    obj->input_funcs[1].func = model2;
    obj->input_funcs[1].metaction = RTUNE_METACTION_NOACTION;
    //obj->input_funcoefs[2] = select_kind;
    //obj->input_funcoefs[3] = model1_select;
    //obj->input_funcoefs[4] = model2_select;

    obj->kind = select_kind;
    rtune_objective_collect_vars(obj);
    return obj;
}

/** the purpose of selecting which model to use is dependent on the select
 * Work in progress
 */
rtune_objective_t *rtune_objective_add_select(rtune_region_t *region, char *name, rtune_objective_kind_t select_kind, int num_models, rtune_func_t *models[], int model_select[]) {
    if (select_kind != RTUNE_OBJECTIVE_SELECT_MIN && select_kind != RTUNE_OBJECTIVE_SEELCT_MAX) return NULL;
    int i = region->num_objs;
    rtune_objective_t *obj = &region->objs[i];
    obj->name = name;
    obj->status = RTUNE_STATUS_CREATED;
    region->num_objs++;
    obj->deviation_tolerance = DEFAULT_deviation_tolerance;
    obj->fidelity_window = DEFAULT_fidelity_window;
    obj->lookup_window = DEFAULT_lookup_window;
    obj->search_strategy = RTUNE_OBJECTIVE_SEARCH_DEFAULT;
    obj->max_num_mets = 1;
    obj->num_mets = 0;
    obj->metaction = RTUNE_METACTION_NOACTION;

    for (i=0; i<num_models;i++) {
        rtune_func_t * tmp = models[i];
        tmp->objectives[tmp->num_objs++] = obj;
    }

    obj->num_funcs = num_models;

    //TODO:
    //obj->input_funcs[0].func = models;
    //obj->input_funcs[0].metaction = RTUNE_METACTION_NOACTION;
    //obj->input_funcoefs[1] = select;
    //obj->input_funcoefs[2] = model_select; //a mask to show which model is selected

    obj->kind = select_kind;
    rtune_objective_collect_vars(obj);
    return obj;
}

//going up to reach a threshold
rtune_objective_t *rtune_objective_add_threshold(rtune_region_t *region, char *name, rtune_objective_kind_t threshold_kind, rtune_func_t *model, void *threshold) {
    if (threshold_kind != RTUNE_OBJECTIVE_THRESHOLD && threshold_kind != RTUNE_OBJECTIVE_THRESHOLD_UP && threshold_kind != RTUNE_OBJECTIVE_THRESHOLD_DOWN)
        return NULL;

    int i = region->num_objs;
    rtune_objective_t *obj = &region->objs[i];
    obj->name = name;
    obj->status = RTUNE_STATUS_CREATED;
    region->num_objs++;
    obj->deviation_tolerance = DEFAULT_deviation_tolerance;
    obj->fidelity_window = DEFAULT_fidelity_window;
    obj->lookup_window = DEFAULT_lookup_window;
    obj->search_strategy = RTUNE_OBJECTIVE_SEARCH_DEFAULT;
    obj->max_num_mets = 1;
    obj->num_mets = 0;
    obj->metaction = RTUNE_METACTION_NOACTION;

    model->objectives[model->num_objs++] = obj;

    obj->num_funcs = 1;
    obj->input_funcs[0].func = model;
    obj->input_funcs[0].metaction = RTUNE_METACTION_NOACTION;
    obj->num_coefs = 1;
    obj->input_coefs[0].coef = (utype_t)threshold;

    obj->kind = threshold_kind;
    rtune_objective_collect_vars(obj);
    return obj;
}

//going down to reach a threshold
rtune_objective_t *rtune_objective_add_threshold_down(rtune_region_t *region, char *name, rtune_func_t *model, void *threshold) {
    int i = region->num_objs;
    rtune_objective_t *obj = &region->objs[i];
    obj->name = name;
    obj->status = RTUNE_STATUS_CREATED;
    region->num_objs++;
    obj->deviation_tolerance = DEFAULT_deviation_tolerance;
    obj->fidelity_window = DEFAULT_fidelity_window;
    obj->lookup_window = DEFAULT_lookup_window;
    obj->search_strategy = RTUNE_OBJECTIVE_SEARCH_DEFAULT;
    obj->max_num_mets = 1;
    obj->num_mets = 0;
    obj->metaction = RTUNE_METACTION_NOACTION;

    model->objectives[model->num_objs++] = obj;

    obj->num_funcs = 1;
    obj->input_funcs[0].func = model;
    obj->input_funcs[0].metaction = RTUNE_METACTION_NOACTION;
    obj->num_coefs = 1;
    obj->input_coefs[0].coef = (utype_t)threshold;

    obj->kind = RTUNE_OBJECTIVE_THRESHOLD_DOWN;
    rtune_objective_collect_vars(obj);
    return obj;
}

void rtune_objective_set_fidelity_attr(rtune_objective_t *obj, float deviation_tolerance, int fidelity_window, int lookup_window) {
    obj->deviation_tolerance = deviation_tolerance;
    obj->fidelity_window = fidelity_window;
    obj->lookup_window = lookup_window;
}

void rtune_objective_set_search_strategy(rtune_objective_t *obj, rtune_objective_attribute_t search_strategy) {
    obj->search_strategy = search_strategy;
}

void rtune_objective_set_metaction(rtune_objective_t *obj, rtune_action_t metaction) {
    obj->metaction = metaction;
}

void rtune_objective_set_metaction_var(rtune_objective_t *obj, rtune_var_t * var, rtune_action_t metaction) {
	int i;
	for (i=0; i<obj->num_vars; i++) {
		if (obj->input_vars[i].var == var) {
			obj->input_vars[i].metaction = metaction;
			break;
		}
	}
}

void rtune_objective_set_metaction_func(rtune_objective_t *obj, rtune_func_t * func, rtune_action_t metaction) {
	int i;
	for (i=0; i<obj->num_funcs; i++) {
		if (obj->input_funcs[i].func == func) {
			obj->input_funcs[i].metaction = metaction;
			break;
		}
	}
}

void rtune_objective_set_max_mets(rtune_objective_t *obj, int max) {
    obj->max_num_mets = max;
}

//set the apply policy for all the variables that are the input for the object func. Not sure whether it is useful yet
void rtune_objective_set_apply_policy(rtune_objective_t * obj,  rtune_var_apply_policy_t apply_policy) {
    
}

void rtune_objective_add_callback(rtune_objective_t * obj, void (*callback) (rtune_objective_t *, void *), void *arg) {
    obj->callback = callback;
    obj->callback_arg = arg;
}

inline static utype_t rtune_stvar_apply(stvar_t * stvar, int index) {
	utype_t v = rtune_stvar_get_value(stvar, index);
    if (stvar->applier) stvar->applier(v._typed_value);
    return v;
}

static utype_t rtune_var_apply(rtune_var_t * var, int index, int iteration) {
    var->current_apply_index = index;
    var->last_apply_iteration = iteration;
    return rtune_stvar_apply(&var->stvar, index);
}

void rtune_var_reset(rtune_var_t * var) {
	var->status = RTUNE_STATUS_RESETTED;
	var->stvar.num_states = 0;
	if (var->kind == RTUNE_VAR_LIST || var->kind == RTUNE_VAR_RANGE) {
		var->current_v_index = -1;
		memset(var->count_value, var->num_unique_values, 0);
	}
}

void rtune_func_reset(rtune_func_t * func) {
	func->status = RTUNE_STATUS_RESETTED;
	func->stvar.num_states = 0;
}

void rtune_func_reset_deep(rtune_func_t * func) {
	func->status = RTUNE_STATUS_RESETTED;
	func->stvar.num_states = 0;
	int i;
	for (i=0; i<func->num_vars; i++) {
		rtune_var_reset(func->input_vars[i]);
	}
}

void rtune_objective_reset(rtune_objective_t * obj) {
	obj->status = RTUNE_STATUS_RESETTED;
}

void rtune_objective_reset_deep(rtune_objective_t * obj) {
	obj->status = RTUNE_STATUS_RESETTED;
	int i;
	for (i=0; i<obj->num_funcs; i++) {
		rtune_func_reset_deep(obj->input_funcs[i].func);
	}
}

/**
 * the following set of ugly macro is just for code reuse and refactoring to retrieve values in different situations
 * a variable named __state__ is used across multiple macros. So those macros must be used together
 */

#define RTUNE_VAR_GET_NEXT_STATE_LIST(TYPE, var, index) \
    TYPE __state__ = ((TYPE *)(var->list_range_setting.list.list_values))[index]

#define RTUNE_VAR_GET_NEXT_STATE_RANGE(TYPE, var, index) \
    TYPE __state__ = var->list_range_setting.range.rangeBegin._##TYPE##_value + index * var->list_range_setting.range.step._##TYPE##_value

#define STVAR_GET_NEXT_STATE(TYPE, stvar) \
    void * provider = stvar->provider;\
    void * provider_arg = stvar->provider_arg;\
    TYPE __state__;\
    if (provider == provider_arg) {\
        __state__ = *((TYPE *)(provider));\
    } else {\
        __state__ = ((TYPE(*)(void *))(provider))(provider_arg);\
    }

#define STVAR_UPDATE_NEXT_STATE(TYPE, stvar) \
    stvar->v._##TYPE##_value = ((TYPE *)stvar->states)[stvar->num_states++] = __state__; \
    if (stvar->callback) stvar->callback(stvar->callback_arg);                           \


#define RTUNE_VAR_UPDATE_LIST(TYPE, var, stvar, index) \
    {                                                  \
        RTUNE_VAR_GET_NEXT_STATE_LIST(TYPE, var, index); \
        STVAR_UPDATE_NEXT_STATE(TYPE, stvar);          \
    }

#define RTUNE_VAR_UPDATE_RANGE(TYPE, var, stvar, index)\
    {                                                  \
        RTUNE_VAR_GET_NEXT_STATE_RANGE(TYPE, var, index); \
        STVAR_UPDATE_NEXT_STATE(TYPE, stvar);          \
    }

/**
 * update the list or range variable
 * @param var
 * @return the index (sequence number) of the state, -1 indicate failure of update
 */
static int rtune_var_update_list_range(rtune_var_t *var) {
    stvar_t *stvar = &var->stvar;
    int index = -1;
    int num_values = var->num_unique_values;

    //find the index for the next value based on the specified update policy
    if (var->update_policy == RTUNE_UPDATE_LIST_SERIES) {
        index = var->current_v_index + 1;
    } else if (var->update_policy == RTUNE_UPDATE_LIST_SERIES_CYCLIC) {
        index = (var->current_v_index + 1) % num_values;
    } else if (var->update_policy == RTUNE_UPDATE_LIST_RANDOM) {
        index = random() % num_values; //TODO: random call need seed call for the whole program with srand(time(0)). right now, we do not know where to put this srand call
    } else if (var->update_policy == RTUNE_UPDATE_LIST_RANDOM_UNIQUE) {
        if (num_values > stvar->num_states) {
            while (1) {
                index = random() % num_values;
                if (var->count_value[index] == 0)
                    break;
            }
        } else {
            return -1;
        }
    } else {
        return -1;
    }

    var->current_v_index = index;
    var->count_value[index] ++;

    if (var->kind == RTUNE_VAR_LIST) {
        switch (stvar->type) {
            case RTUNE_short:
                RTUNE_VAR_UPDATE_LIST(short, var, stvar, index); break;
            case RTUNE_int:
                RTUNE_VAR_UPDATE_LIST(int, var, stvar, index); break;
            case RTUNE_long:
                RTUNE_VAR_UPDATE_LIST(long, var, stvar, index); break;
            case RTUNE_float:
                RTUNE_VAR_UPDATE_LIST(float, var, stvar, index); break;
            case RTUNE_double:
                RTUNE_VAR_UPDATE_LIST(double, var, stvar, index); break;
            default:
                //error
                break;
        }
    }

    if (var->kind == RTUNE_VAR_RANGE) {
        switch (stvar->type) {
            case RTUNE_short:
                RTUNE_VAR_UPDATE_RANGE(short, var, stvar, index); break;
            case RTUNE_int:
                RTUNE_VAR_UPDATE_RANGE(int, var, stvar, index); break;
            case RTUNE_long:
                RTUNE_VAR_UPDATE_RANGE(long, var, stvar, index); break;
            case RTUNE_float:
                RTUNE_VAR_UPDATE_RANGE(float, var, stvar, index); break;
            case RTUNE_double:
                RTUNE_VAR_UPDATE_RANGE(double, var, stvar, index); break;
            default:
                //error
                break;
        }
    }

    return index;
}

#define RTUNE_STVAR_UPDATE_EXT(TYPE, stvar)  \
    {                                      \
        STVAR_GET_NEXT_STATE(TYPE, stvar);                                   \
        STVAR_UPDATE_NEXT_STATE(TYPE, stvar); \
    }

void * rtune_var_update_ext_straight (stvar_t *stvar) {
    switch (stvar->type) {
        case RTUNE_short:
        RTUNE_STVAR_UPDATE_EXT(short, stvar); break;
        case RTUNE_int:
        RTUNE_STVAR_UPDATE_EXT(int, stvar); break;
        case RTUNE_long:
        RTUNE_STVAR_UPDATE_EXT(long, stvar); break;
        case RTUNE_float:
        RTUNE_STVAR_UPDATE_EXT(float, stvar); break;
        case RTUNE_double:
        RTUNE_STVAR_UPDATE_EXT(double, stvar); break;
        default:
            //error
            break;
    }
}

#define RTUNE_STVAR_UPDATE_EXT_accu4Begin(TYPE, stvar, update)  \
    {\
        STVAR_GET_NEXT_STATE(TYPE, stvar)           \
        __state__ += stvar->accu4Begin_or_base4Diff._##TYPE##_value;\
        stvar->accu4Begin_or_base4Diff._##TYPE##_value = __state__;\
        if (update) {                                       \
            STVAR_UPDATE_NEXT_STATE(TYPE, stvar);           \
            stvar->accu4Begin_or_base4Diff._##TYPE##_value = 0;   \
        }            \
    }

void rtune_stvar_update_ext_accu4Begin (stvar_t *stvar, int update) {
    switch (stvar->type) {
        case RTUNE_short:
        RTUNE_STVAR_UPDATE_EXT_accu4Begin(short, stvar, update); break;
        case RTUNE_int:
        RTUNE_STVAR_UPDATE_EXT_accu4Begin(int, stvar, update); break;
        case RTUNE_long:
        RTUNE_STVAR_UPDATE_EXT_accu4Begin(long, stvar, update); break;
        case RTUNE_float:
        RTUNE_STVAR_UPDATE_EXT_accu4Begin(float, stvar, update); break;
        case RTUNE_double:
        RTUNE_STVAR_UPDATE_EXT_accu4Begin(double, stvar, update); break;
        default:
            //error
            break;
    }
}

#define RTUNE_STVAR_UPDATE_DIFF_base4Diff(TYPE, stvar)  \
    {\
        STVAR_GET_NEXT_STATE(TYPE, stvar)           \
        stvar->accu4Begin_or_base4Diff._##TYPE##_value = __state__;\
    }

void rtune_stvar_update_diff_base4Diff (stvar_t *stvar) {
    switch (stvar->type) {
        case RTUNE_short:
        RTUNE_STVAR_UPDATE_DIFF_base4Diff(short, stvar); break;
        case RTUNE_int:
        RTUNE_STVAR_UPDATE_DIFF_base4Diff(int, stvar); break;
        case RTUNE_long:
        RTUNE_STVAR_UPDATE_DIFF_base4Diff(long, stvar); break;
        case RTUNE_float:
        RTUNE_STVAR_UPDATE_DIFF_base4Diff(float, stvar); break;
        case RTUNE_double:
        RTUNE_STVAR_UPDATE_DIFF_base4Diff(double, stvar); break;
        default:
            //error
            break;
    }
}

#define RTUNE_STVAR_UPDATE_EXT_accu4End(TYPE, stvar, update)  \
    {\
        STVAR_GET_NEXT_STATE(TYPE, stvar)           \
        __state__ += stvar->accu4End_or_accu4Diff._##TYPE##_value;\
        stvar->accu4End_or_accu4Diff._##TYPE##_value = __state__;\
        if (update) {                                     \
            STVAR_UPDATE_NEXT_STATE(TYPE, stvar);         \
            stvar->accu4End_or_accu4Diff._##TYPE##_value = 0;          \
        }            \
    }

void rtune_stvar_update_ext_accu4End (stvar_t *stvar, int update) {
    switch (stvar->type) {
        case RTUNE_short:
        RTUNE_STVAR_UPDATE_EXT_accu4End(short, stvar, update); break;
        case RTUNE_int:
        RTUNE_STVAR_UPDATE_EXT_accu4End(int, stvar, update); break;
        case RTUNE_long:
        RTUNE_STVAR_UPDATE_EXT_accu4End(long, stvar, update); break;
        case RTUNE_float:
        RTUNE_STVAR_UPDATE_EXT_accu4End(float, stvar, update); break;
        case RTUNE_double:
        RTUNE_STVAR_UPDATE_EXT_accu4End(double, stvar, update); break;
        default:
            //error
            break;
    }
}

#define RTUNE_STVAR_UPDATE_DIFF_accu4Diff(TYPE, stvar, update)  \
    {\
        STVAR_GET_NEXT_STATE(TYPE, stvar)                  \
        __state__ = __state__ - stvar->accu4Begin_or_base4Diff._##TYPE##_value + stvar->accu4End_or_accu4Diff._##TYPE##_value; \
        stvar->accu4End_or_accu4Diff._##TYPE##_value = __state__;                                                             \
        if (update) {                                      \
            STVAR_UPDATE_NEXT_STATE(TYPE, stvar);               \
            stvar->accu4End_or_accu4Diff._##TYPE##_value = 0;          \
        }                                                       \
    }

void rtune_stvar_update_diff_accu4Diff (stvar_t *stvar, int update) {
    switch (stvar->type) {
        case RTUNE_short:
        RTUNE_STVAR_UPDATE_DIFF_accu4Diff(short, stvar, update); break;
        case RTUNE_int:
        RTUNE_STVAR_UPDATE_DIFF_accu4Diff(int, stvar, update); break;
        case RTUNE_long:
        RTUNE_STVAR_UPDATE_DIFF_accu4Diff(long, stvar, update); break;
        case RTUNE_float:
        RTUNE_STVAR_UPDATE_DIFF_accu4Diff(float, stvar, update); break;
        case RTUNE_double:
        RTUNE_STVAR_UPDATE_DIFF_accu4Diff(double, stvar, update); break;
        default:
            //error
            break;
    }
}

/**
 *
 * @param stvar
 * @param update_policy
 * @param batch_index
 * @param batch_size
 * @return the sequence number (index) of the updated value of the stvar, -1 if not updated
 */
static int rtune_stvar_ext_update_begin(stvar_t * stvar, rtune_var_update_kind_t update_policy, int batch_index, int batch_size) {
    int index = -1;
    switch (update_policy) {
        case RTUNE_UPDATE_BATCH_STRAIGHT:
            if (batch_index == 0) {//only update the var if it is batch_straight
                rtune_var_update_ext_straight(stvar);
                index = stvar->num_states-1;
            }
            break;
        case RTUNE_UPDATE_BATCH_ACCUMULATE:;
            int update = batch_index == batch_size - 1;
            rtune_stvar_update_ext_accu4Begin(stvar, update) ;
            if (update) index = stvar->num_states-1;
            break;
        default:
            break;
    }
    return index;
}

static void rtune_stvar_ext_diff_update_begin(stvar_t * stvar, rtune_var_update_kind_t update_policy, int batch_index) {
    switch (update_policy) {
        case RTUNE_UPDATE_BATCH_STRAIGHT:
            if (batch_index == 0) {//only update the var if it is batch_straight
                rtune_stvar_update_diff_base4Diff(stvar);
            }
            break;
        case RTUNE_UPDATE_BATCH_ACCUMULATE:
            rtune_stvar_update_diff_base4Diff(stvar);
            break;
        default:
            break;
    }
}

/**
 *
 * @param stvar
 * @param update_policy
 * @param batch_index
 * @param batch_size
 * @return the index of the new state, -1 if update is not performed (but intermediate value collected)
 */
static int rtune_stvar_ext_update_end(stvar_t * stvar, rtune_var_update_kind_t update_policy, int batch_index, int batch_size) {
    int index = -1;
    switch (update_policy) {
        case RTUNE_UPDATE_BATCH_STRAIGHT:
            if (batch_index == 0) {//only update the var if it is batch_straight
                rtune_var_update_ext_straight(stvar);
                index = stvar->num_states-1;
            }
            break;
        case RTUNE_UPDATE_BATCH_ACCUMULATE:;
            int update = batch_index == batch_size - 1;
            rtune_stvar_update_ext_accu4End(stvar, update) ;
            if (update) index = stvar->num_states-1;
            break;
        default:
            break;
    }
    return index;
}

static int rtune_stvar_ext_diff_update_end(stvar_t * stvar, rtune_var_update_kind_t update_policy, int batch_index, int batch_size) {
    int index = -1;
    switch (update_policy) {
        case RTUNE_UPDATE_BATCH_STRAIGHT:
            if (batch_index == 0) {//only update the var if it is batch_straight
                rtune_stvar_update_diff_base4Diff(stvar);
                index = stvar->num_states-1;
            }
            break;
        case RTUNE_UPDATE_BATCH_ACCUMULATE:;
            int update = batch_index == batch_size - 1;
            rtune_stvar_update_diff_accu4Diff(stvar, update);
            if (update) index = stvar->num_states-1;
            break;
        default:
            break;
    }
    return index;
}

static void rtune_func_print_doubleFunc_intVar(rtune_func_t * func, int count) {
    int i;
    stvar_t *stvar = &func->stvar;
    int num_states = stvar->num_states;
    printf("=============== values for func %s at iteration: %d ====================\n", stvar->name, count);
    printf("\t\t\t");
    for (i=0; i<num_states; i++) {
        printf("%d\t", i);
    }
    printf("\n");

    printf("func %s: \t", stvar->name);

    for (i=0; i<num_states; i++) {
        printf("%.2f\t", ((double*)stvar->states)[i]);
    }
    printf("\n");

    int j;
    for (j=0; j<func->num_vars; j++) {
        rtune_var_t * var = func->input_vars[j];
        printf("var %s: ", var->stvar.name);
        for (i=0; i<num_states; i++) {
            int vi = func->input[i*func->num_vars + j];
            printf("\t%d", ((int*)var->stvar.states)[vi]);
        }
        printf("\n");
    }
    printf("================================================================================\n");
}

/**
 * optimization of a unimodal function to find the min. A unimodal function has only one min/max and
 * @param obj
 * @return We would like the function return the index if the min is found. If the min is not found, the function should
 *         return a value that representing the direction of searching (increment or decrement).
 */
static int rtune_objective_min_unimodal_gradient_1var(rtune_objective_t *obj) {
    rtune_func_t *func = obj->input_funcs[0].func;
    stvar_t *func_stvar = &func->stvar;

    rtune_var_t *avar = func->active_var;
    stvar_t *avar_stvar = &avar->stvar;
    rtune_var_t *var = func->input_vars[0]; //this should be the active avar

    //assert func_stvar->num_states == var_stvar->num_states;
    int num_states = func_stvar->num_states;
    int i;

    //requires at least fidelity-trust-window number of states to calculate the gradient
    int window_end = num_states - obj->lookup_window;
    //calculate gradient of the last fidelity_window number of states
    int trend_decreasing = 0;
    int trend_increasing = 0;
    int trend_flat = 0;
    if (func_stvar->type == RTUNE_double) {
        double *func_states = (double *) func_stvar->states;
        for (i = num_states - 1; i > window_end; i--) {
            double func1 = ((double *) (func_stvar->states))[i];
            double func0 = ((double *) (func_stvar->states))[i-1];

            float deviation_tolerance = obj->deviation_tolerance;
            double deviation = func1 - func0;
            double deviationPercentage = fabs(deviation)/func0;
            if (deviationPercentage >= deviation_tolerance && deviation >= 0) {//must see consecutive increasing
                trend_increasing++;
                printf("trend increasing from [%d]:%.2f->[%d]:%.2f (%.2f%%) and greater tolerance(%0.2f%%)\n", i-1, func0, i, func1, deviationPercentage * 100, deviation_tolerance*100);
            } else { //deviation == 0,
        //    	trend_increasing = 0;
            }
        }
        if (trend_increasing >= obj->fidelity_window) { //trend_increasing should be at least the same as fidelity window consecutively to be considered as obj being met
            int index = num_states - trend_increasing - 1;
            printf("********* min (%d) reached within %d (fidelity window) increasing ****************\n", index, obj->fidelity_window);
            return index;
        }
    }

    return -1;
}

/**
 * This function find the min in the state starting from index start for count number of elements. If a pointer (minValue) to a variable
 * is provided, the value in the variable will be included for finding the min, and the min found by this function will be copied
 * back to the minValue pointer variable
 * @param stvar: the stvar
 * @param start: starting states
 * @param count: the number of states to look for the min
 * @param minValue: if provided, to provide the initial min value for comparision and be used for returning the initial value
 * @return: the index of the state that has the min, if not found, return -1
 */
#define STVAR_FIND_MIN_EXHAUSTIVE(TYPE, stvar, start, count, minIndex, minValue) \
    TYPE *states = (TYPE*) stvar->states;          \
    TYPE temp = minValue->_##TYPE##_value; \
    int foundNewMin = 0; \
    int i;                          \
    for(i=start; i<start+count; i++) {           \
        if (states[i] <= temp ) {            \
            temp = states[i];                \
            minIndex = i;                  \
            foundNewMin = 1; \
        }                                 \
    } \
	if (foundNewMin) { minValue->_##TYPE##_value = temp; }


int rtune_stvar_find_min(stvar_t * stvar, int start, int count, utype_t *minValue) {
	int num_states = stvar->num_states;
	if (start + count > num_states) return -1;

    int index = -1;
    if (stvar->type == RTUNE_short) {
        STVAR_FIND_MIN_EXHAUSTIVE(short, stvar, start, count, index, minValue);
    } else if (stvar->type == RTUNE_int) {
        STVAR_FIND_MIN_EXHAUSTIVE(int, stvar, start, count, index, minValue);
    } else if (stvar->type == RTUNE_long) {
        STVAR_FIND_MIN_EXHAUSTIVE(long, stvar, start, count, index, minValue);
    } else if (stvar->type == RTUNE_float) {
        STVAR_FIND_MIN_EXHAUSTIVE(float, stvar, start, count, index, minValue);
    } else if (stvar->type == RTUNE_double) {
        STVAR_FIND_MIN_EXHAUSTIVE(double, stvar, start, count, index, minValue);
    } else index = -1;
    return index;
}

/**
 * optimization of a unimodal function to find the max. A unimodal function has only one min/max and
 * @param obj
 * @return We would like the function return the index if the max is found. If the max is not found, the function should
 *         return a value that representing the direction of searching (increment or decrement).
 */
static int rtune_objective_max_unimodal_gradient_1var(rtune_objective_t *obj) {
    rtune_func_t *func = obj->input_funcs[0].func;
    stvar_t *func_stvar = &func->stvar;

    rtune_var_t *avar = func->active_var;
    stvar_t *avar_stvar = &avar->stvar;
    rtune_var_t *var = func->input_vars[0]; //this should be the active avar

    //assert func_stvar->num_states == var_stvar->num_states;
    int num_states = func_stvar->num_states;
    int i;

    //requires at least fidelity-trust-window number of states to calculate the gradient
    int window_end;
    if (num_states < obj->lookup_window) { //as long as the last fidelity_window amount of sampling data shows increase, we are good for meeting the obj
        return -1;
    } else {
        window_end = num_states - obj->lookup_window;
    }
    //calculate gradient of the last fidelity_window number of states
    int trend_decreasing = 0;
    int trend_increasing = 0;
    int trend_flat = 0;
    if (func_stvar->type == RTUNE_double) {
        double *func_states = (double *) func_stvar->states;
        for (i = num_states - 1; i > window_end; i--) {
            double func1 = ((double *) (func_stvar->states))[i];
            double func0 = ((double *) (func_stvar->states))[i-1];

            float deviation_tolerance = obj->deviation_tolerance;
            double deviation = func1 - func0;
            double deviationPercentage = fabs(deviation)/func0;
            if (deviationPercentage >= deviation_tolerance && deviation <= 0) {//must see consecutive decreasing
            	trend_decreasing++;
                printf("trend decreasing from [%d]:%.2f->[%d]:%.2f (%.2f%%) and greater than tolerance(%0.2f%%)\n", i-1, func0, i, func1, deviationPercentage * 100, deviation_tolerance*100);
            } else { //deviation == 0,
        //    	trend_increasing = 0;
            }
        }
        if (trend_decreasing >= obj->fidelity_window) { //trend_increasing should be at least the same as fidelity window consecutively to be considered as obj being met
            int index = num_states - trend_decreasing - 1;
            printf("********* max (%d) reached within %d (fidelity window) decreasing ****************\n", index, obj->fidelity_window);
            return index;
        }
    }

    return -1;
}

/**
 * This function find the max in the state starting from index start for count number of elements. If a pointer (minValue) to a variable
 * is provided, the value in the variable will be included for finding the max, and the max found by this function will be copied
 * back to the minValue pointer variable
 * @param stvar: the stvar
 * @param start: starting states
 * @param count: the number of states to look for the max
 * @param maxValue: if provided, to provide the initial max value for comparison and be used for returning the initial value
 * @return: the index of the state that has the max, if not found, return -1
 */
#define STVAR_FIND_MAX_EXHAUSTIVE(TYPE, stvar, start, count, maxIndex, maxValue) \
    TYPE *states = (TYPE*) stvar->states;          \
    TYPE temp = maxValue->_##TYPE##_value; \
    int foundNewMax = 0; \
    int i;                          \
    for(i=start; i<start+count; i++) {           \
        if (states[i] <= temp ) {            \
            temp = states[i];                \
            maxIndex = i;                  \
            foundNewMax = 1; \
        }                                 \
    } \
	if (foundNewMax) { maxValue->_##TYPE##_value = temp; }


int rtune_stvar_find_max(stvar_t * stvar, int start, int count, utype_t *maxValue) {
	int num_states = stvar->num_states;
	if (start + count > num_states) return -1;

    int index = -1;
    if (stvar->type == RTUNE_short) {
        STVAR_FIND_MAX_EXHAUSTIVE(short, stvar, start, count, index, maxValue);
    } else if (stvar->type == RTUNE_int) {
        STVAR_FIND_MAX_EXHAUSTIVE(int, stvar, start, count, index, maxValue);
    } else if (stvar->type == RTUNE_long) {
        STVAR_FIND_MAX_EXHAUSTIVE(long, stvar, start, count, index, maxValue);
    } else if (stvar->type == RTUNE_float) {
        STVAR_FIND_MAX_EXHAUSTIVE(float, stvar, start, count, index, maxValue);
    } else if (stvar->type == RTUNE_double) {
        STVAR_FIND_MAX_EXHAUSTIVE(double, stvar, start, count, index, maxValue);
    } else index = -1;
    return index;
}

void rtune_region_begin(rtune_region_t * region) {
    //printf("RTune region: %s(%x) begin, count: %d\n", region->name, region, count);
    //we need a flag to simply ignore the rest if rtune's job is done
    int count = ++region->count;
    if (region->status == RTUNE_STATUS_RETIRED) {
        //TODO: check to see whether we need to apply the config for those var that are needed to do for each iteration
    	return;
    }

    rtune_var_update_kind_t update_lt;
    rtune_var_update_kind_t update_policy;
    int update_iteration_start;
    int batch_size;
    int update_iteration_stride;
    int batch_index;

    //process each variable
    int i;
    for (i=0; i<region->num_vars; i++) {
        rtune_var_t *var = &region->vars[i];
        if (var->status >= RTUNE_STATUS_UPDATE_COMPLETE) continue; //update is complete or retired
        //if (var->status >= RTUNE_STATUS_RETIRED) continue; //no more updated needed

        stvar_t * stvar = &var->stvar;
        update_lt = var->update_lt;
        if (update_lt != RTUNE_UPDATE_REGION_BEGIN && update_lt != RTUNE_UPDATE_REGION_BEGIN_END &&
            update_lt != RTUNE_UPDATE_REGION_BEGIN_END_DIFF)
            continue;

        //Check whether this is the iteration for the update based on the schedule
        update_iteration_start = var->update_iteration_start;
        batch_index = count - update_iteration_start;
        if (batch_index < 0) continue; //not its turn, the var-status should be RTUNE_STATUS_CREATED
        else if (batch_index == 0) { //It is for this var's turn
            //the setting of enabling a variable update is solely determined by the update_iteration_start. Thus if a function
            //has two more more variables, they need to be updated one by one. Their schedules should NOT overlap, see rtune_func_schedule_check func
            var->status = RTUNE_STATUS_SAMPLING;
        }

        batch_size = var->batch_size;
        update_iteration_stride = var->update_iteration_stride;
        batch_index = batch_index % (batch_size + update_iteration_stride);
        if (batch_index > batch_size) {
            //in the stride, skip
            continue; //No need to update this var since it is not its turn stvar on
        }

        rtune_kind_t kind = var->kind;
        update_policy = var->update_policy;

        int index = -1; //the index to access the state of the new var of the variable

        switch (kind) {
            case RTUNE_VAR_LIST:
            case RTUNE_VAR_RANGE: {
                if (batch_index == 0) {
                    index = rtune_var_update_list_range(var);
                    //rtune_var_print_list_range(var, count);
                }
                break;
            }
            case RTUNE_VAR_EXT:
                index = rtune_stvar_ext_update_begin(stvar, update_policy, batch_index, batch_size);
                break;
#if USING_VAR_EXT_DIFF_IS_USEFUL
            case RTUNE_VAR_EXT_DIFF: {
                rtune_stvar_ext_diff_update_begin(stvar, update_policy, batch_index);
                break;
            }
#endif
            default:
                break;
        }
        if (index >=0 ) { //update this config in the config and apply this var config
            rtune_var_apply(var, index, count);
            if (stvar->total_num_states == stvar->num_states) {//update completed and this is last iteration of the last batch.
            	var->status = RTUNE_STATUS_UPDATE_COMPLETE;
            	//rtune_var_print_list_range(var, count);
            }
        }
    }

    //update the states of the function that needs to be modeled if the function is in the state of sampling
    //var->func usage dependency forms a tree/graph data structure, but most cases two-level tree. Right now, we only consider var->func two level dependency
    // For an arbitrary tree, we need to traverse this tree to update each unmodeled function. Depth-first algorithm should be used since in
    // the situation that an unmodeled funcs depends on more than one variables, the function should be updated according
    // to one variable first, and then the other.
    for (i = 0; i < region->num_funcs; i++) {
        rtune_func_t *func = &region->funcs[i];
        if (func->status >= RTUNE_STATUS_UPDATE_COMPLETE) continue; //no more updated needed

        rtune_var_t * avar = NULL;
        if (func->num_vars > 0) {
            //check the variables to see which one is used to follow the schedule if the func's schedule is not set
            //XXX: right now, at exactly only one variable of the function should be in the sampling stage.
            int j;
            avar = func->active_var; //active variable
            if (avar == NULL || avar->status != RTUNE_STATUS_SAMPLING || avar->status != RTUNE_STATUS_UPDATE_COMPLETE) { //find another active var
            	avar = NULL;
                for (j=0; j<func->num_vars; j++) {
                    rtune_var_t * tmp = func->input_vars[j];
                    if (tmp->status == RTUNE_STATUS_SAMPLING || tmp->status == RTUNE_STATUS_UPDATE_COMPLETE) {
                        func->active_var = tmp;
                        avar = tmp;
                        break;
                    }
                }
                if (avar == NULL) { func->active_var = NULL; }
            }
        }

        if (func->update_lt == RTUNE_DEFAULT_NONE && avar != NULL)
            update_lt = avar->update_lt;
        else update_lt = func->update_lt;

        if (update_lt != RTUNE_UPDATE_REGION_BEGIN && update_lt != RTUNE_UPDATE_REGION_BEGIN_END &&
            update_lt != RTUNE_UPDATE_REGION_BEGIN_END_DIFF)
            continue;

        if (func->update_policy == RTUNE_DEFAULT_NONE && avar != NULL)
            update_policy = avar->update_policy;
        else update_policy = func->update_policy;

        if (func->update_iteration_start == RTUNE_DEFAULT_NONE && avar != NULL)
            update_iteration_start = avar->update_iteration_start;
        else update_iteration_start = func->update_iteration_start;

        batch_index = count - update_iteration_start;
        if (batch_index < 0) continue;
        else if (batch_index == 0) {
            func->status = RTUNE_STATUS_SAMPLING;
        }

        if (func->batch_size == RTUNE_DEFAULT_NONE && avar != NULL)
            batch_size = avar->batch_size;
        else batch_size = func->batch_size;

        if (func->update_iteration_stride == RTUNE_DEFAULT_NONE && avar != NULL)
            update_iteration_stride = avar->update_iteration_stride;
        else update_iteration_stride = func->update_iteration_stride;

        batch_index = batch_index % (batch_size + update_iteration_stride);

        if (batch_index > batch_size) {
            //in the stride, skip
            continue; //No need to update this var since it is not its turn stvar on
        }

        stvar_t * stvar = &func->stvar;
        int index = -1;
        switch (func->kind) {
            case RTUNE_FUNC_EXT:
                index = rtune_stvar_ext_update_begin(&func->stvar, update_policy, batch_index, batch_size);
                break;
            case RTUNE_FUNC_EXT_DIFF:
                rtune_stvar_ext_diff_update_begin(&func->stvar, update_policy, batch_index);
                break;
            default:
                break;
        }

        if (index >= 0) {//update the input of this new func value
        	func->unused_updates++;
            int k;
            int * input = &func->input[index*func->num_vars]; //input is a 2-D array of int [total_num_states][num_vars]
            for (k=0; k<func->num_vars; k++) {
                //The input of the func from the var is always the last state of the var as it is latest update since
                //the var of a func is only updated one a time (by restricting their schedule to not overlap)
                input[k] = func->input_vars[k]->stvar.num_states - 1;
            }
            if (stvar->total_num_states == stvar->num_states) {//Only for RTUNE_FUNC_EXT
            	func->status = RTUNE_STATUS_UPDATE_COMPLETE;
            	rtune_func_print_doubleFunc_intVar(func, count);
            	//TODO: we might not use total_num_states for condition check
            }
        }
    }
}

void rtune_region_end(rtune_region_t * region) {
    if (region->status == RTUNE_STATUS_RETIRED) return;
    int count = region->count;
    int i;
    int num_objs = region->num_objs;
    rtune_objective_t *objs = region->objs;
    rtune_var_update_kind_t update_lt;
    rtune_var_update_kind_t update_policy;
    int update_iteration_start;
    int batch_size;
    int update_iteration_stride;

    int batch_index;

    //update the states of the function that needs to be modeled if the function is in the state of sampling
    //var->func usage dependency forms a tree/graph data structure, but most cases two-level tree. Rigth now, we only consider var->func two level dependency
    // For an arbitrary tree, we need to traverse this tree to update each unmodeled function. Depth-first algorithm should be used since in
    // the situation that an unmodeled funcs depends on more than one variables, the function should be updated according
    // to one variable first, and then the other.
    for (i = 0; i < region->num_funcs; i++) {
        rtune_func_t *func = &region->funcs[i];
        if (func->status == RTUNE_STATUS_RETIRED) continue;

        rtune_var_t * avar = NULL;
        if (func->num_vars > 0) {
            //check the variables to see which one is used to follow the schedule if the func's schedule is not set
            //XXX: right now, at exactly only one variable of the function should be in the sampling stage.
            int j;
            avar = func->active_var; //active variable
            if (avar == NULL || avar->status != RTUNE_STATUS_SAMPLING || avar->status != RTUNE_STATUS_UPDATE_COMPLETE) { //find another active var
            	avar = NULL;
                for (j=0; j<func->num_vars; j++) {
                    rtune_var_t * tmp = func->input_vars[j];
                    if (tmp->status == RTUNE_STATUS_SAMPLING || tmp->status == RTUNE_STATUS_UPDATE_COMPLETE) {
                        func->active_var = tmp;
                        avar = tmp;
                        break;
                    }
                }
                if (avar == NULL) { func->active_var = NULL; }
            }
        }

        if (func->update_lt == RTUNE_DEFAULT_NONE && avar != NULL)
        	update_lt = avar->update_lt;
        else update_lt = func->update_lt;

        if (update_lt != RTUNE_UPDATE_REGION_END && update_lt != RTUNE_UPDATE_REGION_BEGIN_END &&
            update_lt != RTUNE_UPDATE_REGION_BEGIN_END_DIFF)
            continue;

        if (func->update_policy == RTUNE_DEFAULT_NONE && avar != NULL)
        	update_policy = avar->update_policy;
        else update_policy = func->update_policy;

        if (func->update_iteration_start == RTUNE_DEFAULT_NONE && avar != NULL)
        	update_iteration_start = avar->update_iteration_start;
        else update_iteration_start = func->update_iteration_start;

        batch_index = count - update_iteration_start;
        if (batch_index < 0) continue;
        else if (batch_index == 0) {
            if (update_lt == RTUNE_UPDATE_REGION_END) func->status = RTUNE_STATUS_SAMPLING;
        }

        if (func->batch_size == RTUNE_DEFAULT_NONE && avar != NULL) batch_size = avar->batch_size;
        else batch_size = func->batch_size;
        if (func->update_iteration_stride == RTUNE_DEFAULT_NONE)
            update_iteration_stride = avar->update_iteration_stride;
        else update_iteration_stride = func->update_iteration_stride;

        batch_index = batch_index % (batch_size + update_iteration_stride);
        if (batch_index > batch_size) {
            //in the stride, skip
            continue; //No need to update this var since it is not its turn stvar on
        }

        stvar_t *stvar = &func->stvar;
        int index = -1;
        switch (func->kind) {
            case RTUNE_FUNC_EXT:
                index = rtune_stvar_ext_update_end(&func->stvar, update_policy, batch_index, batch_size);
                break;
            case RTUNE_FUNC_EXT_DIFF:
                index = rtune_stvar_ext_diff_update_end(&func->stvar, update_policy, batch_index, batch_size);
                break;
            default:
                break;
        }

        if (index >= 0) {//update the input of this new func value
        	func->unused_updates++;
            int *input = &func->input[index * func->num_vars]; //input is a 2-D array of int [total_num_states][num_vars]
            int j;
            for (j = 0; j < func->num_vars; j++) {
                //The input of the func from the var is always the last state of the var as it is latest update since
                //the var of a func is only updated one a time (by restricting their schedule to not overlap)
                input[j] = func->input_vars[j]->stvar.num_states - 1;
            }

            if (stvar->total_num_states == stvar->num_states) {//update completed at the beginning of the last batch
                func->status = RTUNE_STATUS_UPDATE_COMPLETE;
                rtune_func_print_doubleFunc_intVar(func, count);
                //TODO: we might not use total_num_states for condition check
            }
        }
    }

    //check objective to see whether anyone is met. An objective is only check when its objective func is complete,
    //which is complete only if the variable have all the values for the func
    for (i = 0; i < num_objs; i++) {
        rtune_objective_t *obj = &objs[i];
        if (obj->status == RTUNE_STATUS_RETIRED) continue;
        int j;
        int num_updated_funcs = 0;
        for (j=0; j<obj->num_funcs; j++) { //Check to see whether it needs to check the objective in this iteration by checking whether a func is updated or not
        	rtune_func_t *func = obj->input_funcs[j].func;
        	if (func->unused_updates) {
        		num_updated_funcs ++;
        	}
        }
        if (num_updated_funcs == 0) continue; //No func has been updated this time, no need to evaluate objectives

        switch (obj->kind) {
            case RTUNE_OBJECTIVE_MIN: {
                //printf("Evaluating min threshold ...: ");
                //int index = rtune_objective_evaluate_min(obj);
                //int var_index = obj->input_funcs[0].func->input[index];
                //if (index >= 0)
                //    printf("min objective is met: index: %d, var: %d, func: %.2f\n", index,
                //           ((short *) (obj->input_funcs[0].func->input_vars[0]->stvar.states))[var_index],
                //           ((double *) (obj->input_funcs[0].func->stvar.states))[index]);

                rtune_func_t *func = obj->input_funcs[0].func;
                rtune_var_t * var = func->input_vars[0]; //This should be the same as obj->config[0].var;
                int index = -1;
                int var_index = -1;

                if (obj->search_strategy == RTUNE_OBJECTIVE_SEARCH_UNIMODAL_ON_THE_FLY) {
                	if (func->stvar.num_states < obj->lookup_window) continue;
                    printf("########## Evaluating min objective with unimodal on the fly ...: ##################################\n");
                    printf("########## Lookup Window: %d, Fidelity Window: %d ###############################################\n",
                           obj->lookup_window, obj->fidelity_window);
                    rtune_func_print_doubleFunc_intVar(func, count);
                    index = rtune_objective_min_unimodal_gradient_1var(obj);
                    func->unused_updates = 0;
                    if (index >= 0) {
                        obj->status = RTUNE_STATUS_OBJECTIVE_MET;
                        var_index = func->input[index];
                        printf("min objective is met: index: %d, var: %d, func: %.2f\n", index,
                        		rtune_var_get_value(var, var_index)._short_value, rtune_func_get_value(func, index)._double_value);
                    	//apply the variable configuration for the objective that is just met
                    	obj->input_vars[0].value = rtune_var_apply(var, var_index, count);
                    	obj->input_vars[0].var = var;
                    	obj->input_vars[0].index = var_index;
                    	obj->input_vars[0].preference_right = 1;
                    	obj->input_vars[0].last_iteration_applied = count;

                    	obj->input_funcs[0].index = index;
                    	obj->input_funcs[0].value = rtune_func_get_value(func, index);

                    	//call the callback of the objective
                    	if (obj->callback != NULL) obj->callback(obj, obj->callback_arg);
                    }
                    printf("######################################################################################################\n");
                } else if (obj->search_strategy == RTUNE_OBJECTIVE_SEARCH_EXHAUSTIVE_AFTER_COMPLETE) {
                    if (func->status != RTUNE_STATUS_UPDATE_COMPLETE ) continue;
                    printf("####### Evaluating min objective with exhaustive search after sampling complete ...: #######\n");
                    //index = rtune_objective_evaluate_min_exhaustive_after_complete(obj);
                    utype_t *minValue = &(obj->input_funcs[0].value); //For getting the current min value
                    index = rtune_stvar_find_min(&(func->stvar), 0, func->stvar.num_states, minValue);
                    obj->status = RTUNE_STATUS_OBJECTIVE_MET;
                    func->unused_updates = 0;
                    if (index >= 0) {
                    	var_index = func->input[index];
                        printf("min objective is met: index: %d, var: %d, func: %.2f\n", index,
                        		rtune_var_get_value(var, var_index)._short_value, rtune_func_get_value(func, index)._double_value);
                    }
                	//apply the variable configuration for the objective that is just met
                	obj->input_vars[0].value = rtune_var_apply(var, var_index, count);
                	obj->input_vars[0].var = var;
                	obj->input_vars[0].index = var_index;
                	obj->input_vars[0].preference_right = 1;
                	obj->input_vars[0].last_iteration_applied = count;

                	obj->input_funcs[0].index = index;
                   	obj->input_funcs[0].value = rtune_func_get_value(func, index);

                	//call the callback of the objective
                	if (obj->callback != NULL) obj->callback(obj, obj->callback_arg);
                    printf("######################################################################################################\n");
                } else if (obj->search_strategy == RTUNE_OBJECTIVE_SEARCH_EXHAUSTIVE_ON_THE_FLY) {
                    printf("##### Evaluating min objective with exhaustive search on the fly ...: ########\n");
                    utype_t *minValue = &(obj->input_funcs[0].value); //For getting the current min value
                    index = rtune_stvar_find_min(&(func->stvar), func->stvar.num_states-1, 1, minValue);
                    func->unused_updates = 0;
                    //index = rtune_objective_evaluate_min_exhaustive_on_the_fly(obj, minValue);
                    if (index >= 0) { //Here we store the temp min in the config and input_funcs
                    	obj->input_funcs[0].index = index;
                    	var_index = func->input[index];
                    	obj->input_vars[0].value = rtune_var_get_value(var, var_index);
                    	obj->input_vars[0].index = var_index;
                    	obj->input_vars[0].var = var;
                    	obj->input_vars[0].preference_right = 1;
                    	//obj->config[0].last_iteration_applied = count;

                    	printf("min objective is met: index: %d, var: %d, func: %.2f\n", index,
                        		rtune_var_get_value(var, var_index)._short_value, obj->input_funcs[0].value._double_value);
                    }
                    if (func->status == RTUNE_STATUS_UPDATE_COMPLETE) {
                        obj->status = RTUNE_STATUS_OBJECTIVE_MET;
                        var_index = obj->input_vars[0].index;
                    	rtune_var_apply(var, var_index, count);
                    	obj->input_vars[0].last_iteration_applied = count;
                    	//apply the variable configuration for the objective that is just met


                    	//call the callback of the objective
                    	if (obj->callback != NULL) obj->callback(obj, obj->callback_arg);
                    }
                    printf("######################################################################################################\n");
                } else {
                	//unsupported min search strategy
                	printf("unsupported min search strategy\n");
                }
                break;
            }
            case RTUNE_OBJECTIVE_MAX: {
                rtune_func_t *func = obj->input_funcs[0].func;
                rtune_var_t * var = func->input_vars[0]; //This should be the same as obj->config[0].var;
                int index = -1;
                int var_index = -1;

                if (obj->search_strategy == RTUNE_OBJECTIVE_SEARCH_UNIMODAL_ON_THE_FLY) {
                	if (func->stvar.num_states < obj->lookup_window) continue;
                    printf("########## Evaluating max objective with unimodal on the fly ...: ##################################\n");
                    printf("########## Lookup Window: %d, Fidelity Window: %d ###############################################\n",
                           obj->lookup_window, obj->fidelity_window);
                    rtune_func_print_doubleFunc_intVar(func, count);
                    index = rtune_objective_max_unimodal_gradient_1var(obj);
                    func->unused_updates = 0;
                    if (index >= 0) {
                        obj->status = RTUNE_STATUS_OBJECTIVE_MET;
                        var_index = func->input[index];
                        printf("max objective is met: index: %d, var: %d, func: %.2f\n", index,
                        		rtune_var_get_value(var, var_index)._short_value, rtune_func_get_value(func, index)._double_value);
                    	//apply the variable configuration for the objective that is just met
                    	obj->input_vars[0].value = rtune_var_apply(var, var_index, count);
                    	obj->input_vars[0].var = var;
                    	obj->input_vars[0].index = var_index;
                    	obj->input_vars[0].preference_right = 1;
                    	obj->input_vars[0].last_iteration_applied = count;

                    	obj->input_funcs[0].index = index;
                    	obj->input_funcs[0].value = rtune_func_get_value(func, index);

                    	//call the callback of the objective
                    	if (obj->callback != NULL) obj->callback(obj, obj->callback_arg);
                    }
                    printf("######################################################################################################\n");
                } else if (obj->search_strategy == RTUNE_OBJECTIVE_SEARCH_EXHAUSTIVE_AFTER_COMPLETE) {
                    if (func->status != RTUNE_STATUS_UPDATE_COMPLETE)  continue;
                    printf("####### Evaluating max objective with exhaustive search after sampling complete ...: #######\n");
                    //index = rtune_objective_evaluate_min_exhaustive_after_complete(obj);
                    utype_t *maxValue = &(obj->input_funcs[0].value); //For getting the current max value
                    index = rtune_stvar_find_max(&(func->stvar), 0, func->stvar.num_states, maxValue);
                    func->unused_updates = 0;
                    obj->status = RTUNE_STATUS_OBJECTIVE_MET;
                    if (index >= 0) {
                    	var_index = func->input[index];
                        printf("max objective is met: index: %d, var: %d, func: %.2f\n", index,
                        		rtune_var_get_value(var, var_index)._short_value, rtune_func_get_value(func, index)._double_value);
                    }
                	//apply the variable configuration for the objective that is just met
                	obj->input_vars[0].value = rtune_var_apply(var, var_index, count);
                	obj->input_vars[0].var = var;
                	obj->input_vars[0].index = var_index;
                	obj->input_vars[0].preference_right = 1;
                	obj->input_vars[0].last_iteration_applied = count;

                	obj->input_funcs[0].index = index;
                   	obj->input_funcs[0].value = rtune_func_get_value(func, index);

                	//call the callback of the objective
                	if (obj->callback != NULL) obj->callback(obj, obj->callback_arg);
                    printf("######################################################################################################\n");
                } else if (obj->search_strategy == RTUNE_OBJECTIVE_SEARCH_EXHAUSTIVE_ON_THE_FLY) {
                    printf("##### Evaluating max objective with exhaustive search on the fly ...: ########\n");
                    utype_t *maxValue = &(obj->input_funcs[0].value); //For getting the current max value
                    index = rtune_stvar_find_max(&(func->stvar), func->stvar.num_states-1, 1, maxValue);
                    func->unused_updates = 0;
                    //index = rtune_objective_evaluate_min_exhaustive_on_the_fly(obj, minValue);
                    if (index >= 0) { //Here we store the temp max in the config and input_funcs
                    	obj->input_funcs[0].index = index;
                    	var_index = func->input[index];
                    	obj->input_vars[0].value = rtune_var_get_value(var, var_index);
                    	obj->input_vars[0].index = var_index;
                    	obj->input_vars[0].var = var;
                    	obj->input_vars[0].preference_right = 1;
                    	//obj->config[0].last_iteration_applied = count;

                    	printf("min objective is met: index: %d, var: %d, func: %.2f\n", index,
                        		rtune_var_get_value(var, var_index)._short_value, obj->input_funcs[0].value._double_value);
                    }
                    if (func->status == RTUNE_STATUS_UPDATE_COMPLETE) {
                        obj->status = RTUNE_STATUS_OBJECTIVE_MET;
                        var_index = obj->input_vars[0].index;
                    	rtune_var_apply(var, var_index, count);
                    	obj->input_vars[0].last_iteration_applied = count;
                    	//apply the variable configuration for the objective that is just met
                    	//call the callback of the objective
                    	if (obj->callback != NULL) obj->callback(obj, obj->callback_arg);
                    }
                    printf("######################################################################################################\n");
                } else {
                	printf("unsupported max search strategy\n");
                }
                break;
            }
            case RTUNE_OBJECTIVE_INTERSECTION:
                break;
            case RTUNE_OBJECTIVE_THRESHOLD_DOWN:
                break;
            case RTUNE_OBJECTIVE_THRESHOLD_UP:
                break;
            case RTUNE_OBJECTIVE_SELECT_MIN:
                break;
            case RTUNE_OBJECTIVE_SEELCT_MAX:
                break;
            case RTUNE_OBJECTIVE_THRESHOLD:
                break;
            default:
                break;
        }
    }

    //At this point, all the objectives that are met are applied and in action. TODO: we should apply config/reset action here
    int num_mets = 0;
    for (i = 0; i < num_objs; i++) {
        rtune_objective_t *obj = &objs[i];
        if (obj->status == RTUNE_STATUS_OBJECTIVE_MET) {
        	obj->num_mets++; num_mets++;
        	switch (obj->metaction) {
        		case RTUNE_METACTION_RESET:
        			rtune_objective_reset(obj); //TODO: should we do deep reset or just shallow reset
        			obj->status = RTUNE_STATUS_RESETTED;
        			break;
        		default:
        			break;
        	}
        	if (obj->num_mets == obj->max_num_mets) {
        		obj->status = RTUNE_STATUS_RETIRED; //No need this objective anymore
        		region->num_retired_objs ++;
        		if (region->num_retired_objs == region->num_objs) { //when all objectives are retired, region is retired
        			region->status = RTUNE_STATUS_RETIRED;
        		}
        	}

        	int j;
        	for (j=0; j<obj->num_funcs; j++) {
        		rtune_func_t *func = obj->input_funcs[j].func;
        		switch (obj->input_funcs[j].metaction) {
        			case RTUNE_METACTION_RESET:  //If we need to reset the func
        				rtune_func_reset(func);
        				break;
        			default:
        				break;
        		}
        	}
        	for (j=0; j<obj->num_vars; j++) {
        		rtune_var_t *var = obj->input_vars[j].var;
        		switch (obj->input_vars[j].metaction) {
        			case RTUNE_METACTION_RESET:
        				rtune_var_reset(var);
        				break;
        			case RTUNE_METACTION_CONFIG:
        				//apply the applier of the var
        				break;
        			case RTUNE_METACTION_CONFIG_RESET:
        				//apply the applier of the var
        				rtune_var_reset(var);
        				break;
        			default:
        				break;
        		}
        	}
        }
    }

    //Here we need to stop updating the var and func if the objectives that use them all meet
    for (i = 0; i < region->num_funcs; i++) {
        rtune_func_t *func = &region->funcs[i];
        if (func->status == RTUNE_STATUS_RETIRED) continue;
        int j;
        for (j = 0; j < func->num_objs; j++) {
            rtune_objective_t *obj = func->objectives[j];
            if (obj->status != RTUNE_STATUS_RETIRED) break; //check each obj to see whether it is met
        }
        if (j == func->num_objs) { //if all objs are retired, var update is complete, set it.
            func->status = RTUNE_STATUS_RETIRED;
        }
    }

    //process each variable
    for (i = 0; i < region->num_vars; i++) {
        rtune_var_t *var = &region->vars[i];
        if (var->status == RTUNE_STATUS_RETIRED) continue;
        int j;
        for (j = 0; j < var->num_uses; j++) {
            rtune_func_t *func = var->usedByFuncs[j];
            if (func->status != RTUNE_STATUS_RETIRED) break; //check each obj to see whether it is met
        }
        if (j == var->num_uses) { //if all objs are met, var update is complete, set it.
            var->status = RTUNE_STATUS_RETIRED;
        }
    }
}

float rtune_calcuate_scalability(rtune_region_t *lgp, int exeTimeVar, int numThreadVar, int problemSizeVar)
{
}

#if 0
#define MAX_NUM_VAL_DEFAULT 5000
void rtune_region_add_sysvar(rtune_region_t *lgp, int num_val)
{
    int num_var = lgp->num_var;
    int exeTimeVar = rtune_region_add_double_var(lgp, "ExeTime", num_val, 0.0, 0.0, 0.0);
    rtune_region_set_var_provider(lgp, exeTimeVar, (void *)&read_timer_ms, NULL);
    rtune_objective_min_unimodal_gradient_1var
    int maxNumThreads = 72;
    int numThreadVar = rtune_region_add_int_var(lgp, "NumThread", num_val, 1, maxNumThreads, 2);
    rtune_region_set_var_provider(lgp, numThreadVar, NULL, NULL);

    int cpuFreqVar = rtune_region_add_float_var(lgp, "CPUFreq", num_val, 1, maxNumThreads, 2);
    rtune_region_set_var_provider(lgp, cpuFreqVar, NULL, NULL);

    int sysEnergyVar = rtune_region_add_float_var(lgp, "SysEnergy", num_val, 1, maxNumThreads, 2);
    rtune_region_set_var_provider(lgp, sysEnergyVar, NULL, NULL);

    int problemSizeVar = rtune_region_add_long_var(lgp, "ProblemSize", num_val, 1, maxNumThreads, 2);
    rtune_region_set_var_provider(lgp, problemSizeVar, NULL, NULL);

    int scalabilityVar = rtune_region_add_float_var(lgp, "Scalability", num_val, 1, maxNumThreads, 2);
    rtune_region_set_var_provider(lgp, scalabilityVar, NULL, NULL);
}
#endif

