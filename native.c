#include <stdio.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "CRB_dev.h"
#include "crowbar.h"

#define NATIVE_LIB_NAME "crowbar.lang.file"

static CRB_NativePointerInfo st_native_lib_info = {
    NATIVE_LIB_NAME
};



static void check_argument_count(int arg_count, int true_count,
								char *filename, int line_number)
{
	if (arg_count < true_count)
		crb_runtime_error(filename, line_number, 
						ARGUMENT_TOO_FEW_ERR, MESSAGE_ARGUMENT_END);
	else if (arg_count > true_count)
		crb_runtime_error(filename, line_number, 
						ARGUMENT_TOO_MANY_ERR, MESSAGE_ARGUMENT_END);
}
void crb_nv_print_proc(CRB_Interpreter *interpreter,
						CRB_LocalEnvironment *env,
                            int arg_count,
							char *filename, int line_number)
{
    CRB_Value value;
	CRB_Value *args;

	Encoding saved_encoding = CRB_set_encoding(interpreter->env_encoding);

    value.type = CRB_NULL_VALUE;

	check_argument_count(arg_count, 1, filename, line_number);

	args = crb_stack_peek_value(interpreter, 0);

	CRB_CHAR *str = CRB_value_to_string(&args[0]);
	CRB_print_wcs(stdout, str);
	MEM_free(str);

	crb_stack_shrink_size(interpreter, 1);
	crb_stack_push_value(interpreter, &value);

	CRB_set_encoding(saved_encoding);
}

void crb_nv_println_proc(CRB_Interpreter *interpreter,
						CRB_LocalEnvironment *env,
                            int arg_count,
							char *filename, int line_number)
{
    CRB_Value value;
	CRB_Value *args;

	Encoding saved_encoding = CRB_set_encoding(interpreter->env_encoding);

    value.type = CRB_NULL_VALUE;

	check_argument_count(arg_count, 1, filename, line_number);

	args = crb_stack_peek_value(interpreter, 0);

	CRB_CHAR *str = CRB_value_to_string(&args[0]);
	CRB_println_wcs(stdout, str);
	MEM_free(str);

	crb_stack_shrink_size(interpreter, 1);
	crb_stack_push_value(interpreter, &value);

	CRB_set_encoding(saved_encoding);
}


void crb_nv_fopen_proc(CRB_Interpreter *interpreter,
						CRB_LocalEnvironment *env,
                            int arg_count,
							char *filename, int line_number)
{
    CRB_Value value;
    FILE *fp;
	CRB_Value *args;
	
	check_argument_count(arg_count, 2, filename, line_number);
	
	args = crb_stack_peek_value(interpreter, 1);

    if (args[0].type != CRB_STRING_VALUE
        || args[1].type != CRB_STRING_VALUE) {
        crb_runtime_error(filename, line_number, FOPEN_ARGUMENT_TYPE_ERR,
                          MESSAGE_ARGUMENT_END);
    }

	char *fname = CRB_wcstombs_alloc(filename, line_number, 
							args[0].u.object_value->u.string.string);
	char *fmode = CRB_wcstombs_alloc(filename, line_number,
							args[1].u.object_value->u.string.string);

    fp = fopen(fname, fmode);

	MEM_free(fname);
	MEM_free(fmode);
    
	if (fp == NULL) {
        value.type = CRB_NULL_VALUE;
    } else {
        value.type = CRB_NATIVE_POINTER_VALUE;
        value.u.native_pointer.info = &st_native_lib_info;
        value.u.native_pointer.pointer = fp;
    }

    crb_stack_shrink_size(interpreter, 2);
	crb_stack_push_value(interpreter, &value);
}

static CRB_Boolean
check_native_pointer(CRB_Value *value)
{
    return value->u.native_pointer.info == &st_native_lib_info;
}

void crb_nv_fclose_proc(CRB_Interpreter *interpreter,
						CRB_LocalEnvironment *env,
                             int arg_count,
							 char *filename, int line_number)
{
    CRB_Value value;
    FILE *fp;
	CRB_Value *args;

    value.type = CRB_NULL_VALUE;
	
	check_argument_count(arg_count, 1, filename, line_number);

	args = crb_stack_peek_value(interpreter, 0);

    if (args[0].type != CRB_NATIVE_POINTER_VALUE
        || !check_native_pointer(&args[0])) {
        crb_runtime_error(filename, line_number, FCLOSE_ARGUMENT_TYPE_ERR,
                          MESSAGE_ARGUMENT_END);
    }
    fp = args[0].u.native_pointer.pointer;
    fclose(fp);

    crb_stack_shrink_size(interpreter, 1);
	crb_stack_push_value(interpreter, &value);
}

void crb_nv_fgets_proc(CRB_Interpreter *interpreter,
						CRB_LocalEnvironment *env,
                            int arg_count,
							char *filename, int line_number)
{
    CRB_Value value;
    FILE *fp;
    char buf[LINE_BUF_SIZE];
    char *ret_buf = NULL;
    int ret_len = 0;
	CRB_Value *args;
	
	check_argument_count(arg_count, 1, filename, line_number);


	args = crb_stack_peek_value(interpreter, 0);

    if (args[0].type != CRB_NATIVE_POINTER_VALUE
        || !check_native_pointer(&args[0])) {
        crb_runtime_error(filename, line_number, FGETS_ARGUMENT_TYPE_ERR,
                          MESSAGE_ARGUMENT_END);
    }
    fp = args[0].u.native_pointer.pointer;

    while (fgets(buf, LINE_BUF_SIZE, fp)) {
        int new_len;
        new_len = ret_len + strlen(buf);
        ret_buf = MEM_realloc(ret_buf, new_len + 1);
        if (ret_len == 0) {
            strcpy(ret_buf, buf);
        } else {
            strcat(ret_buf, buf);
        }
        ret_len = new_len;
        if (ret_buf[ret_len-1] == '\n')
            break;
    }
    if (ret_len > 0) {
        value.type = CRB_STRING_VALUE;

		CRB_CHAR *wstr = CRB_mbstowcs_alloc(filename, line_number, ret_buf);
		MEM_free(ret_buf);

        value.u.object_value = 
			crb_create_crowbar_string(interpreter, wstr);
    } else {
        value.type = CRB_NULL_VALUE;
    }

    crb_stack_shrink_size(interpreter, 1);
	crb_stack_push_value(interpreter, &value);
}

void crb_nv_fputs_proc(CRB_Interpreter *interpreter,
						CRB_LocalEnvironment *env,
                            int arg_count,
							char *filename, int line_number)
{
    CRB_Value value;
    FILE *fp;
	CRB_Value *args;

    value.type = CRB_NULL_VALUE;
	check_argument_count(arg_count, 2, filename, line_number);

	args = crb_stack_peek_value(interpreter, 1);

    if (args[0].type != CRB_STRING_VALUE
        || (args[1].type != CRB_NATIVE_POINTER_VALUE
            || !check_native_pointer(&args[1]))) {
        crb_runtime_error(filename, line_number, FPUTS_ARGUMENT_TYPE_ERR,
                          MESSAGE_ARGUMENT_END);
    }
    fp = args[1].u.native_pointer.pointer;

	CRB_print_wcs(fp, args[0].u.object_value->u.string.string);


    crb_stack_shrink_size(interpreter, 2);
	crb_stack_push_value(interpreter, &value);
}

void
crb_add_std_fp(CRB_Interpreter *inter)
{
    CRB_Value fp_value;

    fp_value.type = CRB_NATIVE_POINTER_VALUE;
    fp_value.u.native_pointer.info = &st_native_lib_info;

    fp_value.u.native_pointer.pointer = stdin;
    CRB_add_global_variable(inter, "STDIN", &fp_value);

    fp_value.u.native_pointer.pointer = stdout;
    CRB_add_global_variable(inter, "STDOUT", &fp_value);

    fp_value.u.native_pointer.pointer = stderr;
    CRB_add_global_variable(inter, "STDERR", &fp_value);
}


static CRB_Value new_array(CRB_Interpreter *inter,
								CRB_LocalEnvironment *env,
								CRB_Value *args, int arg_count,
								int pos)
{
	CRB_Value value;
	if (pos == arg_count) {
		value.type = CRB_NULL_VALUE;
	}
	else {
		value.type = CRB_ARRAY_VALUE;
		value.u.object_value = crb_create_array(inter, args[pos].u.int_value);
		int i;
		for (i=0; i<args[pos].u.int_value; i++) {
			value.u.object_value->u.array.array[i] =
				new_array(inter, env, args, arg_count, pos+1);
		}
	}
	return value;
}


void crb_nv_new_array_proc(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							int arg_count,
							char *filename, int line_number)
{
	crb_gc_disable(inter);

	if (arg_count == 0) {
		CRB_Value value;
		value.type = CRB_INT_VALUE;
		value.u.int_value = 0;
		crb_stack_push_value(inter, &value);
		arg_count = 1;
	}

	CRB_Value *args;

	args = crb_stack_peek_value(inter, arg_count-1);

	int i;
	for (i=0; i<arg_count; i++) {
		if (args[i].type != CRB_INT_VALUE || args[i].u.int_value < 0)
			crb_runtime_error(filename, line_number, 
					NEW_ARRAY_ARGUMENT_TYPE_ERR,
					MESSAGE_ARGUMENT_END);
	}


	CRB_Value array_val;

	array_val = new_array(inter, env, args, arg_count, 0);

	crb_stack_shrink_size(inter, arg_count);

	crb_stack_push_value(inter, &array_val);

	crb_gc_enable(inter);

}

void crb_nv_new_object_proc(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							int arg_count,
							char *filename, int line_number)
{
	crb_gc_disable(inter);

	check_argument_count(arg_count, 0, filename, line_number);

	CRB_Value assoc_value;

	assoc_value.type = CRB_ASSOC_VALUE;
	assoc_value.u.object_value = crb_create_assoc(inter);

	crb_stack_push_value(inter, &assoc_value);

	crb_gc_enable(inter);
}

void crb_nv_new_exception_proc(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							int arg_count,
							char *filename, int line_number)
{
	crb_gc_disable(inter);

	check_argument_count(arg_count, 1, filename, line_number);
	
	CRB_Value *args;

	args = crb_stack_peek_value(inter, arg_count-1);

	CRB_CHAR *wstr = CRB_value_to_string(&args[0]);
		
	CRB_Value msg_val;
	msg_val.type = CRB_STRING_VALUE;
	msg_val.u.object_value = crb_create_crowbar_string(inter, wstr);

	CRB_Value assoc_val;
	assoc_val.type = CRB_ASSOC_VALUE;
	assoc_val.u.object_value = crb_create_assoc(inter);
	
	CRB_Value value;
	value.type = CRB_BOOLEAN_VALUE;
	value.u.boolean_value = CRB_TRUE;

	crb_set_assoc_variable(inter, assoc_val.u.object_value,
							"is_exception", value);
	crb_set_assoc_variable(inter, assoc_val.u.object_value,
							"exception_msg", msg_val);
	
	crb_stack_shrink_size(inter, 1);
	crb_stack_push_value(inter, &assoc_val);

	crb_gc_enable(inter);
}




static void
add_native_functions(CRB_Interpreter *inter)
{
    CRB_add_native_function(inter, "print", crb_nv_print_proc);
    CRB_add_native_function(inter, "println", crb_nv_println_proc);
	CRB_add_native_function(inter, "fopen", crb_nv_fopen_proc);
    CRB_add_native_function(inter, "fclose", crb_nv_fclose_proc);
    CRB_add_native_function(inter, "fgets", crb_nv_fgets_proc);
    CRB_add_native_function(inter, "fputs", crb_nv_fputs_proc);
	CRB_add_native_function(inter, "new_array", crb_nv_new_array_proc);
	CRB_add_native_function(inter, "new_object", crb_nv_new_object_proc);
	CRB_add_native_function(inter, "new_exception", crb_nv_new_exception_proc);

}


void crb_add_native_functions(CRB_Interpreter *inter)
{
	add_native_functions(inter);
}
