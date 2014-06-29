#include "MEM.h"
#include "DBG.h"
#define GLOBAL_VARIABLE_DEFINE
#include "crowbar.h"

static void
add_native_functions(CRB_Interpreter *inter)
{
    CRB_add_native_function(inter, "print", crb_nv_print_proc);
    CRB_add_native_function(inter, "fopen", crb_nv_fopen_proc);
    CRB_add_native_function(inter, "fclose", crb_nv_fclose_proc);
    CRB_add_native_function(inter, "fgets", crb_nv_fgets_proc);
    CRB_add_native_function(inter, "fputs", crb_nv_fputs_proc);
	CRB_add_native_function(inter, "new_array", crb_nv_new_array_proc);
}


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
    interpreter->variable = NULL;
    interpreter->function_list = NULL;
    interpreter->statement_list = NULL;
    interpreter->current_line_number = 1;
	init_value_stack(&interpreter->stack);
	init_object_heap(&interpreter->heap);
	interpreter->top_env = NULL;
	interpreter->source_encoding = source_encoding;
	interpreter->env_encoding = env_encoding;

    crb_set_current_interpreter(interpreter);
    add_native_functions(interpreter);

    return interpreter;
}

static void release_value_stack(CRB_Interpreter *interpreter);

void
CRB_compile(CRB_Interpreter *interpreter, FILE *fp)
{
    extern int yyparse(void);
    extern FILE *yyin;

    crb_set_current_interpreter(interpreter);

	Encoding saved_encoding = CRB_set_encoding(interpreter->source_encoding);

    yyin = fp;
    if (yyparse()) {
        /* BUGBUG */
        fprintf(stderr, "Error ! Error ! Error !\n");
        exit(1);
    }
    crb_reset_string_literal_buffer();
	
	// use value_stack for some constant calculation
	release_value_stack(interpreter);

	CRB_set_encoding(saved_encoding);
}

static void
release_global_strings(CRB_Interpreter *interpreter) 
{
	interpreter->variable = NULL;
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


void
CRB_interpret(CRB_Interpreter *interpreter)
{
    interpreter->execute_storage = MEM_open_storage(0);
    crb_add_std_fp(interpreter);
    crb_execute_statement_list(interpreter, NULL, interpreter->statement_list);
	release_global_strings(interpreter);
	release_value_stack(interpreter);
	crb_garbage_collect(interpreter);
}



void
CRB_dispose_interpreter(CRB_Interpreter *interpreter)
{

    if (interpreter->execute_storage) {
        MEM_dispose_storage(interpreter->execute_storage);
    }
	
	if (interpreter->interpreter_storage) {
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
	Encoding source_encoding = (*pinter)->source_encoding;
	Encoding env_encoding = (*pinter)->env_encoding;
	CRB_dispose_interpreter(*pinter);
	*pinter = CRB_create_interpreter(source_encoding, env_encoding);
}
