#include <string.h>
#include "MEM.h"
#include "DBG.h"
#define GLOBAL_VARIABLE_DEFINE
#include "crowbar.h"



static void init_value_stack(Stack *stack)
{
	stack->stack_alloc_size = 0;
	stack->stack_pointer = 0;
	stack->stack = NULL;

}

static void init_object_heap(Heap *heap)
{
	heap->current_heap_size = 0;
	heap->current_threshold = 0;
	heap->header = NULL;
	heap->gc_enabled = 1;
}


CRB_Interpreter *
CRB_create_interpreter(Encoding source_encoding, Encoding env_encoding)
{
    MEM_Storage storage;
    CRB_Interpreter *interpreter;

    storage = MEM_open_storage(0);
    interpreter = MEM_storage_malloc(storage,
                                     sizeof(struct CRB_Interpreter_tag));
    interpreter->interpreter_storage = storage;
    interpreter->execute_storage = NULL;
    interpreter->function_list = NULL;
    interpreter->statement_list = NULL;
	interpreter->current_file_name = NULL;
    interpreter->current_line_number = 1;
	init_value_stack(&interpreter->stack);
	init_object_heap(&interpreter->heap);
	interpreter->top_env = NULL;
	interpreter->source_encoding = source_encoding;
	interpreter->env_encoding = env_encoding;
	memset(&(interpreter->exception_jumper), 0, sizeof(jmp_buf));
	interpreter->throwed_exception = NULL;
	interpreter->compile_state = 0;
	interpreter->regexp_literals = NULL;

    crb_set_current_interpreter(interpreter);
    crb_add_native_functions(interpreter);
	crb_add_regexp_functions(interpreter);


    return interpreter;
}

static void release_value_stack(CRB_Interpreter *interpreter);

int CRB_compile(CRB_Interpreter *interpreter, char *filename)
{
    extern int yyparse(void);

    crb_set_current_interpreter(interpreter);

	Encoding saved_encoding = CRB_set_encoding(interpreter->source_encoding);

    if (crb_push_parse_file(filename)) {
		fprintf(stderr, "crb_push_parse_file(%s) error", filename);
		exit(1);
	}
	if (yyparse() || interpreter->compile_state != 0) {
        /* BUGBUG */
        fprintf(stderr, "Error ! Error ! Error !\n");
        exit(1);
    }
    crb_reset_string_literal_buffer();
	
	// use value_stack for some constant calculation
	release_value_stack(interpreter);

	CRB_set_encoding(saved_encoding);


	return 0;
	
}

static void
release_global_strings(CRB_Interpreter *interpreter) 
{
	interpreter->first_env.environ_scope = NULL;
	return;

}

static void release_value_stack(CRB_Interpreter *interpreter) 
{
	if (interpreter->stack.stack) {
		MEM_free(interpreter->stack.stack);
		interpreter->stack.stack = NULL;
		interpreter->stack.stack_alloc_size = 0;
		interpreter->stack.stack_pointer = 0;
	}
}

static void init_environment(CRB_Interpreter *interpreter)
{
	interpreter->first_env.environ_scope = 
			crb_create_scope_chain(interpreter, NULL, CRB_FALSE);
	interpreter->first_env.parent_env = NULL;
	interpreter->first_env.func_name = "(top level)";
	interpreter->first_env.caller_line_number = 0;
	interpreter->top_env = &(interpreter->first_env);
}


int CRB_interpret(CRB_Interpreter *interpreter)
{
	int return_value = 0;

	init_environment(interpreter);
    interpreter->execute_storage = MEM_open_storage(0);
    crb_add_std_fp(interpreter);
	
	RecoverEnvironment recover;
	crb_save_environment(interpreter, &recover);
	int jmp_value = setjmp(interpreter->exception_jumper);

	if (jmp_value == 0) {

    	crb_execute_statement_list(interpreter, interpreter->top_env, 
								interpreter->statement_list);

		crb_recover_environment(interpreter, &recover);
	}
	else {

		CRB_Object *obj = interpreter->throwed_exception;

		exception_print_stack_trace(interpreter, obj);	

		interpreter->throwed_exception = NULL;

		crb_recover_environment(interpreter, &recover);

		return_value = 1;
	}
	

	release_global_strings(interpreter);
	release_value_stack(interpreter);
	crb_garbage_collect(interpreter);

	return return_value;
}



void
CRB_dispose_interpreter(CRB_Interpreter *interpreter)
{

	crb_dispose_regexp_literals(interpreter);
    
	if (interpreter->execute_storage) {
        MEM_dispose_storage(interpreter->execute_storage);
    }
	
	
	if (interpreter->interpreter_storage) {
		/* include interpreter itself */
    	MEM_dispose_storage(interpreter->interpreter_storage);
	}


	MEM_check_all_blocks_empty();
}

void
CRB_add_native_function(CRB_Interpreter *interpreter,
                        char *name, CRB_NativeFunctionProc *proc)
{
    FunctionDefinition *fd;

    fd = crb_malloc(sizeof(FunctionDefinition));
    fd->name = name;
    fd->type = NATIVE_FUNCTION_DEFINITION;
    fd->u.native_f.proc = proc;
    fd->next = interpreter->function_list;

    interpreter->function_list = fd;
}


void CRB_reset_interpreter(CRB_Interpreter **pinter)
{
	//printf("CRB_reset_interpreter\n");
	Encoding source_encoding = (*pinter)->source_encoding;
	Encoding env_encoding = (*pinter)->env_encoding;
	CRB_dispose_interpreter(*pinter);
	*pinter = CRB_create_interpreter(source_encoding, env_encoding);
}
