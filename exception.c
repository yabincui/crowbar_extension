#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "crowbar.h"

static int count_stack_frame_num(CRB_LocalEnvironment *env)
{
	int count = 0;
	while (env != NULL) {
		count++;
		env = env->parent_env;
	}
	return count;
}


void exception_build_stack_trace(CRB_Interpreter *inter,
							CRB_LocalEnvironment *env,
							CRB_Object *assoc_obj,
							int line_number)
{
	Variable *variable = crb_search_assoc_variable(inter,
								assoc_obj, "stack_trace_array",
								CRB_TRUE);

	// test if build already
	if (variable->value.type != CRB_NULL_VALUE)
		return;

	crb_gc_disable(inter);

	int frame_num = count_stack_frame_num(env);
	int i;

	CRB_Object *stack_trace_array = crb_create_array(inter,
											frame_num);
	CRB_Value value;

	CRB_LocalEnvironment *it_env = env;
	int func_line_number = line_number;
	for (i=0; i<frame_num; i++) {
		char *func_name = it_env->func_name;
		if (func_name == NULL) {
			func_name = "(anonymous closure)";
		}

		CRB_Object *trace_assoc = crb_create_assoc(inter);

		value.type = CRB_STRING_VALUE;
		value.u.object_value = crb_create_crowbar_string(inter,
								CRB_mbstowcs_alloc(__FILE__,
									__LINE__, func_name));
		
		crb_set_assoc_variable(inter, trace_assoc, "func_name",
								value);

		value.type = CRB_INT_VALUE;
		value.u.int_value = func_line_number;
		crb_set_assoc_variable(inter, trace_assoc, "line_number",
								value);

		value.type = CRB_ASSOC_VALUE;
		value.u.object_value = trace_assoc;
		
		stack_trace_array->u.array.array[i] = value;



		func_line_number = it_env->caller_line_number;
		it_env = it_env->parent_env;
	}

	value.type = CRB_ARRAY_VALUE;
	value.u.object_value = stack_trace_array;

	variable->value = value;

	crb_gc_enable(inter);
}


void exception_print_stack_trace(CRB_Interpreter *inter,
									CRB_Object *assoc_obj)
{
	fprintf(stderr, "exception: ");

	Variable *variable = crb_search_assoc_variable(inter, 
							assoc_obj, "exception_msg",
							CRB_FALSE);
	CRB_print_wcs(stderr, variable->value.u.object_value->u.string.string);
	fprintf(stderr, "\n");

	variable = crb_search_assoc_variable(inter, assoc_obj,
					"stack_trace_array", CRB_FALSE);
	CRB_Object *trace_array = variable->value.u.object_value;

	int i;
	for (i=0; i<trace_array->u.array.length; i++) {
		CRB_Value *pv = &(trace_array->u.array.array[i]);
		CRB_Object *trace_obj = pv->u.object_value;

		Variable *name_variable = crb_search_assoc_variable(inter,
									trace_obj, "func_name",
									CRB_FALSE);
		Variable *line_variable = crb_search_assoc_variable(inter,
									trace_obj, "line_number",
									CRB_FALSE);
		fprintf(stderr, "\tbacktrace in function ");
		CRB_print_wcs(stderr, name_variable->value.u.object_value->u.string.string);
		fprintf(stderr, " line %d\n", line_variable->value.u.int_value);
									
	}
	fprintf(stderr, "\n\n");


}


void crb_save_environment(CRB_Interpreter *inter, RecoverEnvironment *recover)
{
	memcpy(&(recover->save_jumper), &(inter->exception_jumper), sizeof(jmp_buf));
	recover->stack_pos = inter->stack.stack_pointer;
	recover->env_pos = inter->top_env;

}


void crb_recover_environment(CRB_Interpreter *inter, RecoverEnvironment *recover)
{
	inter->stack.stack_pointer = recover->stack_pos;
	while (inter->top_env != recover->env_pos) {
		CRB_LocalEnvironment *tmp_env = inter->top_env;
		inter->top_env = inter->top_env->parent_env;
		MEM_free(tmp_env);

	}
	memcpy(&(inter->exception_jumper), &(recover->save_jumper), sizeof(jmp_buf));
}
