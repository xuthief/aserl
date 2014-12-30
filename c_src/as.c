#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "as.h"
#include <aerospike/as_arraylist_iterator.h>

char *format_as_error(char *api, as_error *err) {
    static char str_error[512];
    sprintf(str_error, "%s: %d - %s", api, err->code, err->message);
    return str_error;
}

void *as_connect_args(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    connect_args_t* args = (connect_args_t*)enif_alloc(sizeof(connect_args_t));

    unsigned arg_length;
    if (!enif_get_list_length(env, argv[0], &arg_length)) goto error0;
    args->host = (char *) malloc(arg_length + 1);
    if (!enif_get_string(env, argv[0], args->host, arg_length + 1, ERL_NIF_LATIN1)) goto error1;

    if (!enif_get_int(env, argv[1], &args->port)) goto error2;

    if (!enif_get_list_length(env, argv[2], &arg_length)) goto error2;
    args->user= (char *) malloc(arg_length + 1);
    if (!enif_get_string(env, argv[2], args->user, arg_length + 1, ERL_NIF_LATIN1)) goto error3;

    if (!enif_get_list_length(env, argv[3], &arg_length)) goto error3;
    args->pass= (char *) malloc(arg_length + 1);
    if (!enif_get_string(env, argv[3], args->pass, arg_length + 1, ERL_NIF_LATIN1)) goto error4;
     
    return (void*)args;

    error4:
    free(args->pass);
    error3:
    free(args->user);
    error2:
    error1:
    free(args->host);
    error0:
    enif_free(args);

    return NULL;
}

ERL_NIF_TERM as_connect(ErlNifEnv* env, handle_t* handle, void* obj)
{
    connect_args_t* args = (connect_args_t*)obj;

    as_status as_res;
	as_error err;
    aerospike *p_as = &(handle->instance);

    // Start with default configuration.
    as_config cfg;
    as_config_init(&cfg);
    as_config_add_host(&cfg, args->host, args->port);
    as_config_set_user(&cfg, args->user, args->pass);

    as_res = aerospike_connect(p_as, &err);

    free(args->host);
    free(args->user);
    free(args->pass);

    if (as_res != AEROSPIKE_OK) {
		aerospike_destroy(p_as);
        return enif_make_tuple2(env, enif_make_atom(env, "error"),
                enif_make_string(env, format_as_error("aerospike_connect", &err), ERL_NIF_LATIN1));
	}

    return enif_make_atom(env, "ok");
}

as_key* init_key_from_args(ErlNifEnv* env, as_key *key, const ERL_NIF_TERM argv[])
{
    unsigned arg_length;

    if (!enif_get_list_length(env, argv[0], &arg_length)) goto error0;
    char *ns = (char *) malloc(arg_length + 1);
    if (!enif_get_string(env, argv[0], ns, arg_length + 1, ERL_NIF_LATIN1)) goto error1;

    if (!enif_get_list_length(env, argv[1], &arg_length)) goto error1;
    char *set = (char *) malloc(arg_length + 1);
    if (!enif_get_string(env, argv[1], set, arg_length + 1, ERL_NIF_LATIN1)) goto error2;

    if (!enif_get_list_length(env, argv[2], &arg_length)) goto error2;
    char *key_str = (char *) malloc(arg_length + 1);
    if (!enif_get_string(env, argv[2], key_str, arg_length + 1, ERL_NIF_LATIN1)) goto error3;

    // Initialize the test as_key object. We won't need to destroy it since it
    // isn't being created on the heap or with an external as_key_value.
    if(!as_key_init_strp(key, ns, set, key_str, false)) goto error4;

    return key;

    error4:
    error3:
    free(key_str);
    error2:
    free(set);
    error1:
    free(ns);
    error0:

    return NULL;
}

as_ldt_type* init_ldt_type_from_arg(ErlNifEnv* env, as_ldt_type *p_ldt_type, const ERL_NIF_TERM arg_type)
{
    nif_as_ldt_type nif_ldt_type;
    if (!enif_get_uint(env, arg_type, &nif_ldt_type)) return NULL;

    switch (nif_ldt_type)
    {
        case NIF_AS_LDT_LLIST:
            *p_ldt_type = AS_LDT_LLIST;
            break;
        case NIF_AS_LDT_LMAP:
            *p_ldt_type = AS_LDT_LMAP;
            break;
        case NIF_AS_LDT_LSET:
            *p_ldt_type = AS_LDT_LSET;
            break;
        case NIF_AS_LDT_LSTACK:
            *p_ldt_type = AS_LDT_LSTACK;
            break;
        default:
            return NULL;
    }
    return p_ldt_type;
}

as_ldt* init_ldt_from_arg(ErlNifEnv* env, as_ldt *p_ldt, as_ldt_type ldt_type, const ERL_NIF_TERM arg_ldt)
{
    unsigned arg_length;
    if (!enif_get_list_length(env, arg_ldt, &arg_length)) goto error0;
    char* str_ldt = (char *) malloc(arg_length + 1);
    if (!enif_get_string(env, arg_ldt, str_ldt, arg_length + 1, ERL_NIF_LATIN1)) goto error1;

	if (!as_ldt_init(p_ldt, str_ldt, ldt_type, NULL)) goto error2;

    return p_ldt;

    error2:
    free(str_ldt);
    error1:
    error0:

    return NULL;
}

as_val* new_val_from_arg(ErlNifEnv* env, const ERL_NIF_TERM argv)
{
    as_val *val;

    // integer
    int64_t i64Value;
    if (enif_get_int64(env, argv, (ErlNifSInt64*)&i64Value))
    {
        val = (as_val *)as_integer_new(i64Value);
        return val;
    }

    // string
    unsigned arg_length;
    if (enif_get_list_length(env, argv, &arg_length))
    {
        char *strValue = (char *) malloc(arg_length + 1);
        if (!enif_get_string(env, argv, strValue, arg_length + 1, ERL_NIF_LATIN1)) goto error1;
        if (!(val=(as_val *)as_string_new(strValue, true))) goto error1;
        return val;

    error1:
        free(strValue);
        return NULL;
    }

    ErlNifBinary val_binary;
    if (enif_inspect_iolist_as_binary(env, argv, &val_binary))
    {
        uint8_t *binValue = malloc(sizeof(uint8_t) * val_binary.size);
        memcpy(binValue, val_binary.data, val_binary.size);
        if (!(val=(as_val *)as_bytes_new_wrap(binValue, val_binary.size, true))) goto error2;
        return val;
        
    error2:
        free(binValue);
        return NULL;
    }

    return NULL;
}

as_policies* init_policy_from_arg(ErlNifEnv* env, as_policies *p_policies, as_ldt_type ldt_type, const ERL_NIF_TERM arg_timeout)
{
    uint32_t ldt_timeout;
    if (!enif_get_uint(env, arg_timeout, &ldt_timeout)) goto error0;

    if (!as_policy_read_init(&p_policies->read)) goto error0;
    p_policies->read.timeout = ldt_timeout;

    return p_policies;

    error0:

    return NULL;
}

void* as_ldt_store_args(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ldt_store_args_t* args = (ldt_store_args_t*)enif_alloc(sizeof(ldt_store_args_t));

    // ns, set, key
    if (!init_key_from_args(env, &args->key, argv)) goto error0;

    // ldt_type 
    if (!init_ldt_type_from_arg(env, &args->ldt_type, argv[3])) goto error1;

    // ldt
    if (!init_ldt_from_arg(env, &args->ldt, args->ldt_type, argv[4])) goto error1;

    // policy
    if (!init_policy_from_arg(env, &args->policies, args->ldt_type, argv[5])) goto error2;

    // value
    if (!(args->p_value=new_val_from_arg(env, argv[6]))) goto error3;

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

ERL_NIF_TERM as_ldt_store(ErlNifEnv* env, handle_t* handle, void* obj)
{
    ldt_store_args_t * args = (ldt_store_args_t *)obj;

    as_status res;
	as_error err;

	// Add an integer value to the set.
    char errmsg[255];
    switch (args->ldt_type)
    {
        case AS_LDT_LSET:
            res = aerospike_lset_add(&handle->instance, &err, &args->policies.apply, &args->key, &args->ldt, args->p_value);
            break;
        case AS_LDT_LLIST:
        case AS_LDT_LMAP:
        case AS_LDT_LSTACK:
        default:
            sprintf(errmsg, "unsupported ldt type: %d", args->ldt_type);
            return enif_make_tuple2(env, enif_make_atom(env, "error"),
                    enif_make_string(env, errmsg, ERL_NIF_LATIN1));
    }

    if(res != AEROSPIKE_OK) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"),
                enif_make_string(env, format_as_error("aerospike_lset_add", &err), ERL_NIF_LATIN1));
    }

    return A_OK(env);
}

void* as_ldt_get_args(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    ldt_get_args_t* args = (ldt_get_args_t*)enif_alloc(sizeof(ldt_get_args_t));

    // ns, set, key
    if (!init_key_from_args(env, &args->key, argv)) goto error0;

    // ldt_type 
    if (!init_ldt_type_from_arg(env, &args->ldt_type, argv[3])) goto error1;

    // ldt
    if (!init_ldt_from_arg(env, &args->ldt, args->ldt_type, argv[4])) goto error1;

    // policy
    if (!init_policy_from_arg(env, &args->policies, args->ldt_type, argv[5])) goto error2;

    return args;

    error2:
    as_ldt_destroy(&args->ldt);
    error1:
    as_key_destroy(&args->key);
    error0:
    enif_free(args);

    return NULL;
}

ERL_NIF_TERM make_nif_term_from_as_val(ErlNifEnv* env, const as_val *p_val)
{
    if(p_val->type == AS_INTEGER)
    {
        return enif_make_int(env, ((as_integer*)p_val)->value);
    }
    else if (p_val->type == AS_STRING)
    {
        as_string *p_str = (as_string *)p_val;
        return enif_make_string(env, p_str->value, ERL_NIF_LATIN1);
    }
    else if(p_val->type == AS_BYTES)
    {
        as_bytes *p_bytes = (as_bytes*)p_val;

        ErlNifBinary val_binary;
        enif_alloc_binary(p_bytes->size, &val_binary);
        memcpy(val_binary.data, p_bytes->value, p_bytes->size);
        return enif_make_binary(env, &val_binary);
    }
    return enif_make_string(env, "unkown", ERL_NIF_LATIN1);
}

ERL_NIF_TERM as_ldt_get(ErlNifEnv* env, handle_t* handle, void* obj)
{
    ldt_get_args_t* args = (ldt_get_args_t*)obj;

    as_status res;
	as_error err;
	as_list* p_list = NULL;

	// Add an integer value to the set.
    char errmsg[255];
    switch (args->ldt_type)
    {
        case AS_LDT_LSET:
            res = aerospike_lset_filter(&handle->instance, &err, NULL, &args->key, &args->ldt, NULL, NULL,
			&p_list);
            break;
        case AS_LDT_LLIST:
        case AS_LDT_LMAP:
        case AS_LDT_LSTACK:
        default:
            sprintf(errmsg, "unsupported ldt type: %d", args->ldt_type);
            return enif_make_tuple2(env, enif_make_atom(env, "error"),
                    enif_make_string(env, errmsg, ERL_NIF_LATIN1));
    }

    if(res != AEROSPIKE_OK) {
        return enif_make_tuple2(env, enif_make_atom(env, "error"),
                enif_make_string(env, format_as_error("aerospike_lset_filter", &err), ERL_NIF_LATIN1));
    }

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

    return enif_make_tuple2(env, A_OK(env), returnValue);
}