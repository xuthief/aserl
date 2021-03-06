#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "as_lset.h"
#include <aerospike/as_arraylist_iterator.h>

void* as_ldt_store_args(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    DEBUG_TRACE("begin args");

    ldt_store_args_t* args = (ldt_store_args_t*)enif_alloc(sizeof(ldt_store_args_t));

    // ns, set, key
    if (!init_key_from_args(env, &args->key, argv)) goto error0;

    // ldt
    if (!init_ldt_from_arg(env, &args->ldt, AS_LDT_LSET, argv[3])) goto error1;

    // timeout
    if (!init_policy_apply_from_arg(env, &args->policy, argv[4])) goto error2;

    // value
    if (!(args->p_value=new_val_from_arg(env, argv[5]))) goto error3;

    DEBUG_TRACE("end args");

    return args;

    error3:
    error2:
    as_ldt_destroy(&args->ldt);
    error1:
    as_key_destroy(&args->key);
    error0:
    enif_free(args);

    return NULL;
}

void as_ldt_clean_store_args(ErlNifEnv* env, ldt_store_args_t* args)
{
    DEBUG_TRACE("begin clean args");
    as_val_destroy(args->p_value);
    as_ldt_destroy(&args->ldt);
    as_key_destroy(&args->key);
    DEBUG_TRACE("end clean args");
}

ERL_NIF_TERM as_ldt_lset_add(ErlNifEnv* env, handle_t* handle, void* obj)
{
    DEBUG_TRACE("begin");

    ldt_store_args_t * args = (ldt_store_args_t *)obj;

    as_status res;
	as_error err;

	// Add an integer value to the set.
    res = aerospike_lset_add(&handle->instance, &err, &args->policy, &args->key, &args->ldt, args->p_value);
    as_ldt_clean_store_args(env, args);

    DEBUG_TRACE("end res: %d", res);

    if(res != AEROSPIKE_OK)
        return A_AS_ERROR(env, err);

    return A_OK(env);
}

ERL_NIF_TERM as_ldt_lset_remove(ErlNifEnv* env, handle_t* handle, void* obj)
{
    ldt_store_args_t * args = (ldt_store_args_t *)obj;

    as_status res;
	as_error err;

	// Add an integer value to the set.
    res = aerospike_lset_remove(&handle->instance, &err, &args->policy, &args->key, &args->ldt, args->p_value);
    as_ldt_clean_store_args(env, args);

    if(res != AEROSPIKE_OK)
        return A_AS_ERROR(env, err);

    return A_OK(env);
}

void* as_ldt_get_args(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ldt_get_args_t* args = (ldt_get_args_t*)enif_alloc(sizeof(ldt_get_args_t));

    // ns, set, key
    if (!init_key_from_args(env, &args->key, argv)) goto error0;

    // ldt
    if (!init_ldt_from_arg(env, &args->ldt, AS_LDT_LSET, argv[3])) goto error1;

    // policy
    if (!init_policy_apply_from_arg(env, &args->policy, argv[4])) goto error2;

    return args;

    error2:
    as_ldt_destroy(&args->ldt);
    error1:
    as_key_destroy(&args->key);
    error0:
    enif_free(args);

    return NULL;
}

void as_ldt_clean_get_args(ErlNifEnv* env, ldt_get_args_t* args)
{
    as_ldt_destroy(&args->ldt);
    as_key_destroy(&args->key);
}

ERL_NIF_TERM as_ldt_lset_get(ErlNifEnv* env, handle_t* handle, void* obj)
{
    ldt_get_args_t* args = (ldt_get_args_t*)obj;

    as_status res;
	as_error err;
	as_list* p_list = NULL;

    res = aerospike_lset_filter(&handle->instance, &err, NULL, &args->key, &args->ldt, NULL, NULL,
            &p_list);
    as_ldt_clean_get_args(env, args);

    if(res != AEROSPIKE_OK)
        return A_AS_ERROR(env, err);

    ERL_NIF_TERM* results;
    uint32_t nresults = as_list_size(p_list);
    results = malloc(sizeof(ERL_NIF_TERM) * nresults);

	as_arraylist_iterator it;
	as_arraylist_iterator_init(&it, (const as_arraylist*)p_list);

    int i = 0;
	// See if the elements match what we expect.
	while (as_arraylist_iterator_has_next(&it)) {
		const as_val* p_val = as_arraylist_iterator_next(&it);
        results[i++] = make_nif_term_from_as_val(env, p_val);
	}

	as_list_destroy(p_list);
	p_list = NULL;

    ERL_NIF_TERM returnValue;
    returnValue = enif_make_list_from_array(env, results, nresults);
    
    free(results);

    return A_OK_VALUE(env, returnValue);
}

ERL_NIF_TERM as_ldt_lset_size(ErlNifEnv* env, handle_t* handle, void* obj)
{
    ldt_get_args_t* args = (ldt_get_args_t*)obj;

    as_status res;
	as_error err;
    uint32_t nsize;

    res = aerospike_lset_size(&handle->instance, &err, &args->policy, &args->key, &args->ldt, &nsize); 
    as_ldt_clean_get_args(env, args);

    if(res != AEROSPIKE_OK)
        return A_AS_ERROR(env, err);

    ERL_NIF_TERM returnValue;
    returnValue = enif_make_uint(env, nsize);
    
    return A_OK_VALUE(env, returnValue);
}
