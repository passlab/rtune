#ifndef RTUNE_RUNTIME_H
#define RTUNE_RUNTIME_H

#include <stdint.h>
//#include "rtune_config.h"
//#include "rtune.h"

#define MAX_NUM_REGIONS 16
#define MAX_NUM_VARS 8
#define MAX_NUM_FUNCS 8
#define MAX_NUM_MODELS 8
#define MAX_NUM_OBJ 8

typedef enum rtune_data_type {
    RTUNE_short,
    RTUNE_int,
    RTUNE_long,
    RTUNE_float,
    RTUNE_double,
    RTUNE_void,
}rtune_data_type_t;

/**
 * internally, a variable is used to represent the independent variables of the RTune method, the dependent variables and models
 * of the method.
 */
typedef enum rtune_kind {
    //independent variables
    RTUNE_VAR,
    RTUNE_VAR_BOOLEAN,
    RTUNE_VAR_COUNTER,
    RTUNE_VAR_CONSTANT,
    RTUNE_VAR_BINARY,
    RTUNE_VAR_RANDOM,
    RTUNE_VAR_LIST,
    RTUNE_VAR_RANGE,
    RTUNE_VAR_EXT,
    RTUNE_VAR_EXT_DIFF,
    //dependent variables, which are known funcs of independent variables
    RTUNE_FUNC,
    RTUNE_FUNC_LOG,
    RTUNE_FUNC_DIFF,
    RTUNE_FUNC_ABS,
    RTUNE_FUNC_THRESHOLD,
    RTUNE_FUNC_DISTANCE,
    RTUNE_FUNC_GRADIENT,
    RTUNE_FUNC_EXT,
    RTUNE_FUNC_EXT_DIFF,
    //For models, which are function with unknown or un-modeled function.
    RTUNE_MODEL,
    RTUNE_MODEL_LINEAR,
    RTUNE_MODEL_QUADRATIC,
    RTUNE_MODEL_IMPLICIT,
    RTUNE_MODEL_UNIMODAL,
} rtune_kind_t;

/**
 * this enum can be used for representing the status of var, func, objective, and a region itself
 */
typedef enum rtune_status {
    RTUNE_STATUS_CREATED,
    RTUNE_STATUS_SAMPLING, //collect sample, profiling for a var/func/objective/region
    RTUNE_STATUS_UPDATE_COMPLETE, //update of the var/func/objective/region are completed, but in the middle of a schedule.
    RTUNE_STATUS_UPDATE_SCHEDULE_COMPLETE, //update of the var/func/objective/region are completed, the batch for the last update is complete as well.
    RTUNE_STATUS_MODELED, //models for a func, or objective funcs have been built
    RTUNE_STATUS_OBJECTIVE_TO_BE_EVALUATED, //a temp status to indicate that the obj should be evaluated in the current iteration
    RTUNE_STATUS_OBJECTIVE_EVALUATING, //an object is still being evaluated
    RTUNE_STATUS_OBJECTIVE_MET, //an objective or multiple objectives have been met
    RTUNE_STATUS_OBJECTIVE_INACTION, //a met objective is used
    RTUNE_STATUS_OBJECTIVE_RETIRED, //an objective is met, applied (inaction) and retired, only applicable for some objectives
    RTUNE_STATUS_REGION_TUNING_COMPLETE, //rtune tuning for the region has completely done and no need to rtune anymore
    RTUNE_STATUS_REGION_COMPLETE, //rtune tuning for the region has completely done and all the configuration have applied, once for all, and no need to rtune anymore
    RTUNE_STATUS_REGION_ALL_OBJECTIVES_MET,
} rtune_status_t;

//#define RTUNE_DEFAULT_NONE -1
/**
 * @brief This enum defines location, time and method for updating a variable. Accumulation is done by addition
 * 
 */
typedef enum rtune_var_update_kind {
    RTUNE_DEFAULT_NONE = -1,
    //Update time and location
    RTUNE_UPDATE_REGION_BEGIN,           //update at the beginning of the region
    RTUNE_UPDATE_REGION_BEGIN_END,       //update at both the beginning and end of the region
    RTUNE_UPDATE_REGION_BEGIN_END_DIFF,  //update as the diff (end-begin) of the two values at the begin and end location,
                                        //one is collected at the end of the region and the other is collected at the beginning the region.
                                        //For example, when collects exec time of a region, we need to collect timer read at the begining and the end of the region, but we only care their diff
    RTUNE_UPDATE_REGION_END,             //var value is updated at the end of the region for each sample

    //batch update policy, for ext and diff var
    RTUNE_UPDATE_BATCH_STRAIGHT, //update once for each batch
    RTUNE_UPDATE_BATCH_ACCUMULATE,  //calculate for each iteration of the batch at the specified time/location (BEGIN and/or END) and
                                    //accumulate together as one update

    //update policy for list and range values
    RTUNE_UPDATE_LIST_RANDOM, //random pick a value from list/range
    RTUNE_UPDATE_LIST_RANDOM_UNIQUE, //random pick but unique
    RTUNE_UPDATE_LIST_SERIES,  //pick value one by one based on the list/range order for one round only
    RTUNE_UPDATE_LIST_SERIES_CYCLIC, //pick value one by one based on the list/range order, cycling after a round
    RTUNE_UPDATE_LIST_FOLLOW_OBJECTIVE, //according to the convergence of the objective of the objective function that uses this variable.
} rtune_var_update_kind_t;

/**
 * @brief var update policy, e.g. whether a var is applied each time is updated or read 
 * 
 */
typedef enum rtune_var_apply_policy {
    //apply policy for var configu
    RTUNE_VAR_APPLY_ON_UPDATE, //var applier is called each time the var is updated
    RTUNE_VAR_APPLY_ON_READ, //var applier is called for each iteration, i.e. when the var is read/updated or needs to be read
} rtune_var_apply_policy_t;

typedef union unified_type {
    short _short_value;
    int _int_value;
    long _long_value;
    float _float_value;
    double _double_value;
    void * _typed_value;
} utype_t;

typedef enum rtune_objective_kind {
    RTUNE_OBJECTIVE_MIN,
    RTUNE_OBJECTIVE_MAX,
    RTUNE_OBJECTIVE_INTERSECTION,
    RTUNE_OBJECTIVE_SELECT_MIN,
    RTUNE_OBJECTIVE_SEELCT_MAX,
    RTUNE_OBJECTIVE_THRESHOLD,
    RTUNE_OBJECTIVE_THRESHOLD_UP,
    RTUNE_OBJECTIVE_THRESHOLD_DOWN,
} rtune_objective_kind_t;

/**
 * var search strategy in order to meet the objective
 */
typedef enum rtune_objective_search_strategy {
    /* search strategy */
    RTUNE_OBJECTIVE_SEARCH_EXHAUSTIVE_AFTER_COMPLETE,
    RTUNE_OBJECTIVE_SEARCH_EXHAUSTIVE_ON_THE_FLY,
    RTUNE_OBJECTIVE_SEARCH_UNIMODAL_ON_THE_FLY,
    RTUNE_OBJECTIVE_SEARCH_RANDOM,
    RTUNE_OBJECTIVE_SEARCH_NELDER_MEAD, //simplex method
    RTUNE_OBJECTIVE_SEARCH_BINARY_GRADIENT,
    RTUNE_OBJECTIVE_SEARCH_QUATERNARY_GRADIENT,
    RTUNE_OBJECTIVE_SEARCH_OCTAL_GRADIENT,
    RTUNE_OBJECTIVE_SEARCH_HEX_GRADIENT,
    //The inhouse binary gradient approach: given a known number of sorted input (x1,x2,...x0,...,xn) for a variable X,
    //x0 is the value in the middle, collect f(x1) (or f(xn)) and f(x0), calculate the gradient g(x1->x0) = (f(x0) - f(x1))/(x0 - x1).
    //For minization, if g(x1->x0) > 0;
} rtune_objective_search_strategy_t;

#define RTUNE_OBJECTIVE_SEARCH_DEFAULT RTUNE_OBJECTIVE_SEARCH_EXHAUSTIVE_ON_THE_FLY

// For objectives: 10% deviation tolerance, 2 fidelity window and 4 lookup window
#define DEFAULT_deviation_tolerance 0.10
#define DEFAULT_fidelity_window 2
#define DEFAULT_lookup_window 4

// For variables and functions
#define DEFAULT_update_iteration_start 0
#define DEFAULT_batch_size 1
#define DEFAULT_update_iteration_stride 0

// For variables
#define DEFAULT_VAR_update_lt RTUNE_UPDATE_REGION_BEGIN
//#define DEFAULT_VAR_update_policy //kind specific so cannot set one for all kinds
#define DEFAULT_VAR_apply_policy RTUNE_VAR_APPLY_ON_UPDATE

// For functions
#define DEFAULT_FUNC_update_lt RTUNE_UPDATE_REGION_END
#define DEFAULT_update_policy RTUNE_UPDATE_BATCH_STRAIGHT

/**
 * stvar stands for state trace variable, introduced for a system to keep track of its state change that can be used for other purpose
 * such as analyzing the trend of the change for building models.
 */
typedef struct stvar {//The base of var, func and model
    //current value. type cast are needed to read or write from this location. This is the first field such
    //that one can use the pointer to read/write the value of the variable just like a regular variable.
    utype_t v;

    char *name; //a meaningful name
    //an independent variable, dependent variable (func), or model since the way we list the kind in the list declaration defined before.
    rtune_data_type_t type; //var data type such as int, short, float, double
    void *states; //sampled values of this variables. /opaque typed-array, element type is determined by type
    int num_states;       //the current number of states
    int total_num_states; //total number of states to have

    void (*callback) (void *); //A callback can only use the values of the variable, i.e. it cannot change the value of
                                //of the variable. The callback must return (cannot call setjmp, etc).
    void *callback_arg;

    void (*applier) (void *); //applier is a function that take the var value as arg to apply the var to the caller env

    void *(*provider) (void *);
    void * provider_arg;  //The corresponding application variable if provided, or a function that can be called to read the value

    utype_t accu4Begin_or_base4Diff; //for ext vars/funcs that need to be accumulated at the BEGINing of the
                                    //region across iterations, this is the accumulator var. For the diff vars/funcs, this is to store
                                    //the base that is used at the END to calculate the diff
    utype_t accu4End_or_accu4Diff;  //for ext vars/funcs that need to be accumulated at the END of the
                                   //region across iterations, this is the accumulator var. For the diff vars/funcs,
                                   //this is the accumulator var to store the diff accumulated across iterations.
} stvar_t;

/**
 * For a tuning variable whose values are provided by either system measurement or users' program, we record
 * its value each time the rtune lexgion of the variable is evaluated. The values are used for analysis and to check
 * against the objective.
 *
 * If the value of the variable is self-described via min:max:step, provider and arg should all be NULL. The values of
 * these variables are evaluated as min, min+step, min+step+step, ..., max if step > 0; as max, max+step, max+step+step, ..., min if step <0;
 * In this case, the provider should be NULL.
 *
 * Otherwise, a variable provider is a pointer or a function that provides the value of the variable that rtune needs to record.
 * For short var, the pointer is of "short *" type, and the function is "short func(void *arg)" type. Internally, they are all
 * casted and stored as void * type. The runtime differentiate a pointer or a function pointer by comparing the "void * provider" with
 * the "void *arg". If they are the same, they are regular variable pointer, thus a pointer dereference will return its value.
 * If they are different, provider is a function pointer and can be called with arg as its argument.
 *
 * A variable is updated each time a rtune region is encountered. Depending on the rtune_var_update_time setting of the var, a variable may be updated
 * at the beginning or at the end of the region, or updated as diff of the two positions.
 *
 * A variable is sampled such that its values are stored, a sample_rate can be specified for a var to define how many times a region is encountered to collect
 * a sample. For example, if the sampe_rate of a var is 5, meaning for each 5 times a region is encountered, a sample of the variable value should be stored.
 * The way how sample is collected can be specified via rtune_var_sample_method_t
 */
typedef struct rtune_var {
    stvar_t stvar;
    rtune_kind_t kind; //kind of var such as boolean, range, list, etc. This filed also can be used to tell whether this is
    rtune_status_t status;

    //update attribute that determine how an var is updated, e.g. which iteration to start updating, how often and the stride to update.
    rtune_var_update_kind_t update_lt; //update location and time
    rtune_var_update_kind_t update_policy; //random, random_unique, series, series-cyclic are policies for list and range only,
    int update_iteration_start; //When the initial sample is collected, which is the sampling_init_iteration count of the rtune_region iteration
    int batch_size;      //The amount of iterations for each update of the variable or for each collection of the sample
    int update_iteration_stride;    //The number of iterations between each sample

    int current_apply_index;
    int last_apply_iteration;
    rtune_var_apply_policy_t apply_policy;

    int num_uses; //number of functions that use this var
    struct rtune_func *usedByFuncs[MAX_NUM_VARS]; /* the func/model that directly use this variable as its input */

    //struct rtune_objective *objs[MAX_NUM_OBJ];
    //int num_objs;

    //list and var-specific fields
    int num_unique_values; //number of unique values can be set for the variable, useful for list and range var
    int *count_value;      //The count of each unique value the variable is set as;
    int current_v_index; //The index of the current value in the list or the range
    int update_direction; //left or right. This is used by the objective to tell how list/range var should be updated
    union list_range_setting { //setting for independent-var of list or range
        struct list_var {
            void *list_values;
            char ** list_names;
        } list;

        struct range_var {
            utype_t rangeBegin;
            utype_t step;
            utype_t rangeEnd;
        } range;
    }list_range_setting;
} rtune_var_t;

/**
 * struct for objective function
 */
typedef struct rtune_func {
    stvar_t stvar;
    rtune_kind_t kind; //kind of var such as boolean, range, list, etc. This filed also can be used to tell whether this is
    rtune_status_t status;
    rtune_var_update_kind_t update_lt; //update location and time. We only need location/time info, no need iteration/batch info since a func is updated according to how its vars are updated
    rtune_var_update_kind_t update_policy; //accumulate or straight policy
    int update_iteration_start; //When the initial sample is collected, which is the sampling_init_iteration count of the rtune_region iteration
    int batch_size;      //how many iterations to update the variable and collect the sample
    int update_iteration_stride;    //The number of iterations between each sample

    rtune_var_t *active_var; //the variable which is being updated

    rtune_var_t *input_vars[MAX_NUM_VARS]; /* the input var and coefficient this variable */
    int num_vars;
    utype_t input_coefs[MAX_NUM_VARS];
    int num_coefs;

    int *input; //The input of var values represented by the index of the state of each variable.
                //This is a 2-D array of int [total_num_states][num_vars]

    //Objectives that use this function
    struct rtune_objective *objectives[MAX_NUM_OBJ];
    int num_objs;
} rtune_func_t;

/**
 * @brief ideally, an objective function include a variable to store the value of the function, multiple variables, and an optional array-based binary expression tree for 
 * deriving the function from variables. 
 * 
 * For a func whose values are directly derived from variables, the func is expressed using the binary expression tree
 * For a func whose values are retrieved from the variables, but with unknown function of the variables. func is just the variable itself. binary_expression_tree is NULL
 * 
 */
typedef struct rtune_objective {
    char * name; //a meaningful name
    rtune_objective_kind_t kind;
    rtune_status_t status;
    void (*callback) (void *);             //callback when the objective is met, or when the objective is used,
    void *callback_arg;
    rtune_objective_search_strategy_t search_strategy; //when the obj should be evaluated, after the funcs are completed updated or while they are being updated
    float deviation_tolerance; /* absolute deviation tolerance */
    int fidelity_window; /* consequent number of occurance of meeting the objective goal to accept that the objective is met */
    int lookup_window; //how many states to check around the posibble state that meets the objective */

    /** var configuration for this objective. To apply the configuration, the applier of each var is called according to the apply_policy of each var, the value applied is what is indexed in this struct*/
    struct var_config {
        rtune_var_t * var;
        //the value of the var that is applied, this field is also used as cache to store the temp func value that currently meets the objective, but not before all the variables of the
        //obj functions are evaluated. E.g. for min objective, it stores the min of the current objective function before it is fully updated.
        utype_t value; //
        int index; //The index for the state value of the var that will be applied to the system/app when the objective that depends on this var is met
        int preference_right; //preference of the value of the var for this objective depending on the list of values of this var, e.g. if the list values is sorted min-max, preference_right true
                              //means that for the similar value of the obj function for this objective, a value toward greater (max) should be used
        int last_iteration_applied; //the last iteration this config is applied
        rtune_var_apply_policy_t apply_policy;  //XXX: Not sure whether we need this objective-specific var apply policy since if each var is independently applied, it has its own apply_policy. We need this 
                                                //only if there is situation that we need apply a var differently according to the different objectives that use the var
    } config[MAX_NUM_VARS];
    int num_vars; //num of independent variables that impact the objective func, thus the objective

    //This is used to store the actual value of the functions when the objective is met.
    struct input_funcs {
    	rtune_func_t * func;
    	utype_t value;
    	int index; //
    } input_funcs[MAX_NUM_VARS];
    int num_funcs;                    //num of models in the input

    struct input_coefs {
    	utype_t coef;
    	rtune_data_type_t type;
    } input_coefs[MAX_NUM_VARS];
    int num_coefs;

} rtune_objective_t;

typedef struct rtune_region {
    char * name;
    rtune_status_t status;
    const void *codeptr_ra;
    const void *end_codeptr;
    const void *end_codeptr2;
    int count; /* total number of execution of the region */

    int num_vars; //number of variables for a tuning region
    //rtune variables include both system/perf variable and user variables. System/perf variable are those
    //related to performance objectives, e.g. timestamp, frequency, power/energy read, and even CPU counters
    //user variables are user provided variables for tuning certain objectives
    rtune_var_t vars[MAX_NUM_VARS];

    rtune_func_t funcs[MAX_NUM_FUNCS];
    int num_funcs;

    //model definition
    rtune_objective_t objs[MAX_NUM_OBJ];
    int num_objs;

    FILE * rtune_logfile;
} rtune_region_t;

//extern rtune_region_t * rtune_regions;
//extern int num_regions;

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * @brief low-level design API.
 *
 * RTune variables stores both its recent values and sampled values by RTune:
 *    1) the most recent value can be read or write by referencing the variable with * operator
 *    2) variable is also the pointer to the internal variable object maintained by RTune
 *    3) The variable object maintains a list of sampled values of the variable when a RTune region is evaluated in
 *       in the call to rtune_region_begin or rtune_region_end
 *
 * RTune variables, func, models, and objectives are all internally stored as the internal variable since access to
 * them have the same property of variables.
 */
 
rtune_region_t * rtune_region_init(char * name);
void rtune_region_begin(rtune_region_t * region);
void rtune_region_end(rtune_region_t * end);
void rtune_regin_begin_sync(rtune_region_t * region); //the call will synced across multiple process, e.g. via MPI_Barrier
void rtune_region_end_sync(rtune_region_t * end);

//API for creating independent variables. A variable has its predefined set of values. The current value of the variable is updated
//by either the pre-set values or from external provider
// a variable pointer can be used for access both its current value, its sampled values, and the object of the variable
//void* rtune_var_add_random(rtune_region_t *region, char * name, int total_num_states, rtune_data_type_t type, void * rangeBegin, void * rangeEnd);
void *rtune_var_add_list(rtune_region_t *region, char *name, int total_num_states, rtune_data_type_t type, int num_values, void *values, char **valname);
void* rtune_var_add_range(rtune_region_t *region, char * name, int total_num_states, rtune_data_type_t type, void * rangeBegin, void * rangeEnd, void * step);
void* rtune_var_add_ext(rtune_region_t *region, char * name, int total_num_states, rtune_data_type_t type, void *(*provider) (void *), void * provider_arg);
void* rtune_var_add_ext_diff(rtune_region_t *region, char * name, int total_num_states, rtune_data_type_t type, void *(*provider) (void *), void * provider_arg);
void  rtune_var_set_update_schedule_attr(rtune_var_t * var, rtune_var_update_kind_t update_lt, rtune_var_update_kind_t update_policy, int update_iteration_start, int update_batch, int update_iteration_stride);
void  rtune_var_set_callback(rtune_var_t *var, void (*callback) (void *), void *arg); //can be used for adding callbacks for var, func and models
void  rtune_var_set_applier_policy(rtune_var_t *var, void (*applier) (void *), rtune_var_apply_policy_t apply_policy); //to set the applier and policy of the var
void  rtune_var_set_applier(rtune_var_t *var, void (*applier) (void *)); //to set the applier of the var. the applier is called when the var is updated.
void  rtune_var_set_apply_policy(rtune_var_t * var, rtune_var_apply_policy_t apply_policy); //set the apply policy for the variables in each iteration 
//helper
void rtune_var_print_list_range(rtune_var_t * var, int count);

//API for creating functions/models. A function is a variable, whose value is determined by the function with specified input variables
void* rtune_func_add_log(rtune_region_t *region, char * name, rtune_data_type_t type, void * var);
void* rtune_func_add_abs(rtune_region_t *region, char * name, rtune_data_type_t type, void * var);
void* rtune_func_add_gradient(rtune_region_t *region, char * name, rtune_data_type_t type, void * var);

void* rtune_func_add_diff(rtune_region_t *region, char * name, rtune_data_type_t type, void * var1, void *var2);
void* rtune_func_add_threshold(rtune_region_t *region, char * name, rtune_data_type_t type, void * var, void *threshold);  /* This variable is a bin variable, if var < threshold, its value is 0, otherwise, its value is 1 */
void* rtune_func_add_distance(rtune_region_t *region, char * name, rtune_data_type_t type, void * var, void *target);  /* This variable is distance variable, whose value is var - target */

void* rtune_func_add(rtune_region_t * region, rtune_kind_t kind, char * name, rtune_data_type_t type, int num_vars, int num_coefficients, ...);
//add a function that will be modeled based on the input and function value, input are knowns, but not the function.
rtune_func_t* rtune_func_add_model(rtune_region_t * region, rtune_kind_t kind, char * name, rtune_data_type_t type,void *(*provider) (void *), void * provider_arg, int num_vars, ...);
void  rtune_func_set_update_schedule_attr(rtune_func_t * var, rtune_var_update_kind_t update_lt, rtune_var_update_kind_t update_policy, int update_iteration_start, int update_batch, int update_iteration_stride);

//API for objectives, an objective is basically a flag to indicate whether a variable (var, func, model) meets certain criteria
rtune_objective_t * rtune_objective_add_min(rtune_region_t *region, char *name, rtune_func_t *func);
rtune_objective_t * rtune_objective_add_max(rtune_region_t *region, char *name, rtune_func_t *func);
rtune_objective_t * rtune_objective_add_intersection(rtune_region_t *region, char *name, rtune_func_t * func1, rtune_func_t * func2);
rtune_objective_t * rtune_objective_add_select2(rtune_region_t *region, char *name, rtune_objective_kind_t select_kind, rtune_func_t * model1, rtune_func_t * model2, int *model1_select, int *model2_select); /* the purpose of selecting which model to use is dependent on the select */

rtune_objective_t * rtune_objective_add_select(rtune_region_t *region, char *name, rtune_objective_kind_t select_kind, int num_models, rtune_func_t * models[], int model_select[]); /* the purpose of selecting which model to use is dependent on the select */
rtune_objective_t * rtune_objective_add_threshold(rtune_region_t *region, char * name, rtune_objective_kind_t threshold_kind, rtune_func_t *model, void *threshold); //going up/down to reach a threshold
void rtune_objective_add_callback(rtune_objective_t * obj, void (*callback) (void *), void *arg);

/**
 * API for setting the fidelity attr of an objective: to manage the fidelity of the model, 
 * tolerance is a range that objective should be in,
 * and window is the number of consecutive occurance that objective is met
 */
void rtune_objective_set_var_sample_attr(rtune_objective_t *obj, int sample_start_iteration, int num_samples, int sample_rate, int sample_stride); //start from the start_iteration, for every sample_rate+stride iterations, we pick one update of the variables. The update is for sample_rate iterations
void rtune_objective_set_fidelity_attr(rtune_objective_t *obj, float deviation_tolerance, int fidelity_window, int lookup_window);
int  rtune_objective_is_met(rtune_objective_t *obj); //check whether objective is met or not */
void rtune_objective_set_search_strategy(rtune_objective_t *obj, rtune_objective_search_strategy_t search_strategy);

void rtune_objective_set_apply_policy(rtune_objective_t * obj,  rtune_var_apply_policy_t apply_policy); //set the apply policy for all the variables that are the input for the object func

//API for callback, which is a function to be called when a var/obj/end is updated/evaluated, etc. TODO: need more scenario to show its usage
void rtune_region_begin_add_callback(rtune_region_t * region, void *(*callback) (void *), void *arg);
void rtune_region_end_add_callback(rtune_region_t * region, void *(*callback) (void *), void *arg);

//The points that one can add callback include: when an objective is met, when a variable is updated, and when a region is synchronized (begin or end).  

//High level API
//For the best performance over OpenMP num_threads
rtune_objective_t * rtune_objective_perf_numThreads(rtune_region_t * region, short min_num_threads, short max_num_threads, short step, int update_rate);
//For the best energy efficiency over CPU frequency
rtune_objective_t * rtune_objective_energy_cpuFrequency(rtune_region_t * region, unsigned long min_freq, unsigned long max_freq, unsigned long step, int update_rate);
//For the best edp (perf gradient * energy gradient over CPU frequency. By changing the CPU frequency, to get product of energy change and performance change
rtune_objective_t * rtune_objective_edp_cpuFrequency(rtune_region_t * region, unsigned long min_freq, unsigned long max_freq, unsigned long step, int update_rate);
//For weak scaling: overall performance(or per thread performance) over OpenMP num_threads and problem size
rtune_objective_t * rtune_objective_weak_numThreads_size(rtune_region_t * region, unsigned long min_freq, unsigned long max_freq, unsigned long step, int update_rate);


#ifdef  __cplusplus
};
#endif

#endif
