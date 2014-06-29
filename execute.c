#include <math.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "crowbar.h"

static StatementResult
execute_statement(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                  Statement *statement);

static StatementResult
execute_expression_statement(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                             Statement *statement)
{
    StatementResult result;

    result.type = NORMAL_STATEMENT_RESULT;

    crb_eval_expression(inter, env, statement->u.expression_s);

    return result;
}

static StatementResult
execute_global_statement(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                         Statement *statement)
{
    IdentifierList *pos;
    StatementResult result;

    result.type = NORMAL_STATEMENT_RESULT;

    if (env == NULL) {
        crb_runtime_error(statement->filename,
						statement->line_number,
                          GLOBAL_STATEMENT_IN_TOPLEVEL_ERR,
                          MESSAGE_ARGUMENT_END);
    }
    for (pos = statement->u.global_s.identifier_list; pos; pos = pos->next) {
		CRB_Boolean ret = crb_add_scope_global_ref(inter, 
								env->environ_scope, pos->name);
		if (ret == CRB_FALSE) {
			crb_runtime_error(
							statement->filename,
							statement->line_number,
							GLOBAL_VARIABLE_NOT_FOUND_ERR,
							STRING_MESSAGE_ARGUMENT, "name",
							pos->name, MESSAGE_ARGUMENT_END);
		}
    }

    return result;
}

static StatementResult
execute_elsif(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
              Elsif *elsif_list, CRB_Boolean *executed)
{
    StatementResult result;
    CRB_Value   cond;
    Elsif *pos;

    *executed = CRB_FALSE;
    result.type = NORMAL_STATEMENT_RESULT;
    for (pos = elsif_list; pos; pos = pos->next) {
        cond = crb_eval_expression(inter, env, pos->condition);
        if (cond.type != CRB_BOOLEAN_VALUE) {
            crb_runtime_error(
								pos->condition->filename,
								pos->condition->line_number,
                              NOT_BOOLEAN_TYPE_ERR, MESSAGE_ARGUMENT_END);
        }
        if (cond.u.boolean_value) {
            result = execute_statement(inter, env,
                                                pos->statement);
            *executed = CRB_TRUE;
            if (result.type != NORMAL_STATEMENT_RESULT)
                goto FUNC_END;
			break;
        }
    }

  FUNC_END:
    return result;
}

static StatementResult
execute_if_statement(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                     Statement *statement)
{
    StatementResult result;
    CRB_Value   cond;

    result.type = NORMAL_STATEMENT_RESULT;
    cond = crb_eval_expression(inter, env, statement->u.if_s.condition);
    if (cond.type != CRB_BOOLEAN_VALUE) {
        crb_runtime_error(
							statement->u.if_s.condition->filename,
							statement->u.if_s.condition->line_number,
                          NOT_BOOLEAN_TYPE_ERR, MESSAGE_ARGUMENT_END);
    }
    DBG_assert(cond.type == CRB_BOOLEAN_VALUE, ("cond.type..%d", cond.type));

    if (cond.u.boolean_value) {
        result = execute_statement(inter, env,
                                       statement->u.if_s.then_statement);
    } else {
        CRB_Boolean elsif_executed;
        result = execute_elsif(inter, env, statement->u.if_s.elsif_list,
                               &elsif_executed);
        if (result.type != NORMAL_STATEMENT_RESULT)
            goto FUNC_END;
        if (!elsif_executed && statement->u.if_s.else_statement) {
            result = execute_statement(inter, env,
                                        statement->u.if_s.else_statement);
                                               
        }
    }

  FUNC_END:
    return result;
}

static StatementResult
execute_while_statement(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                        Statement *statement)
{
    StatementResult result;
    CRB_Value   cond;

    result.type = NORMAL_STATEMENT_RESULT;
    for (;;) {
        cond = crb_eval_expression(inter, env, statement->u.while_s.condition);
        if (cond.type != CRB_BOOLEAN_VALUE) {
            crb_runtime_error(
							statement->u.while_s.condition->filename,
							statement->u.while_s.condition->line_number,
                              NOT_BOOLEAN_TYPE_ERR, MESSAGE_ARGUMENT_END);
        }
        DBG_assert(cond.type == CRB_BOOLEAN_VALUE,
                   ("cond.type..%d", cond.type));
        if (!cond.u.boolean_value)
            break;

        result = execute_statement(inter, env,
                                   statement->u.while_s.statement);
                                           
        if (result.type == RETURN_STATEMENT_RESULT) {
            break;
        } 
		else if (result.type == BREAK_STATEMENT_RESULT) {
            result.type = NORMAL_STATEMENT_RESULT;
            break;
        }
		else if (result.type == CONTINUE_STATEMENT_RESULT) {
			result.type = NORMAL_STATEMENT_RESULT;
		}
    }

    return result;
}

static StatementResult
execute_for_statement(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                      Statement *statement)
{
    StatementResult result;
    CRB_Value   cond;

    result.type = NORMAL_STATEMENT_RESULT;
	result.u.return_value.type = CRB_NULL_VALUE;

    if (statement->u.for_s.init) {
        crb_eval_expression(inter, env, statement->u.for_s.init);
    }
    for (;;) {
        if (statement->u.for_s.condition) {
            cond = crb_eval_expression(inter, env,
                                       statement->u.for_s.condition);
            if (cond.type != CRB_BOOLEAN_VALUE) {
                crb_runtime_error(
						statement->u.for_s.condition->filename,
						statement->u.for_s.condition->line_number,
                        NOT_BOOLEAN_TYPE_ERR, MESSAGE_ARGUMENT_END);
            }
            DBG_assert(cond.type == CRB_BOOLEAN_VALUE,
                       ("cond.type..%d", cond.type));
            if (!cond.u.boolean_value)
                break;
        }
        result = execute_statement(inter, env,
                                   statement->u.for_s.statement);
        if (result.type == RETURN_STATEMENT_RESULT) {
            break;
        } 
		else if (result.type == BREAK_STATEMENT_RESULT) {
            result.type = NORMAL_STATEMENT_RESULT;
            break;
        }
		else if (result.type == CONTINUE_STATEMENT_RESULT) {
			result.type = NORMAL_STATEMENT_RESULT;
		}

        if (statement->u.for_s.post) {
            crb_eval_expression(inter, env, statement->u.for_s.post);
        }
    }

    return result;
}

static StatementResult
execute_return_statement(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                         Statement *statement)
{
    StatementResult result;

    result.type = RETURN_STATEMENT_RESULT;
    if (statement->u.return_s.return_value) {
        result.u.return_value
            = crb_eval_expression(inter, env,
                                  statement->u.return_s.return_value);
    } else {
        result.u.return_value.type = CRB_NULL_VALUE;
    }

    return result;
}

static StatementResult
execute_break_statement(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                        Statement *statement)
{
    StatementResult result;

    result.type = BREAK_STATEMENT_RESULT;
	result.u.return_value.type = CRB_NULL_VALUE;

    return result;
}

static StatementResult
execute_continue_statement(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                           Statement *statement)
{
    StatementResult result;

    result.type = CONTINUE_STATEMENT_RESULT;
	result.u.return_value.type = CRB_NULL_VALUE;

    return result;
}

static StatementResult execute_try_statement(CRB_Interpreter *inter,
											CRB_LocalEnvironment *env,
											Statement *statement)
{
	StatementResult result;
	RecoverEnvironment recover;
	int throw_after_finally = 0;

	crb_save_environment(inter, &recover);
	int jmp_value = setjmp(inter->exception_jumper);

	if (jmp_value == 0) {
		result = execute_statement(inter, env, statement->u.try_s.run_st);
		
		crb_recover_environment(inter, &recover);
	}
	else {
		crb_recover_environment(inter, &recover);

		if (statement->u.try_s.identifier != NULL) {
			char *identifier = statement->u.try_s.identifier;

			CRB_Object *obj = inter->throwed_exception;
			Variable *variable = crb_search_scope_variable(inter, 
									env->environ_scope, identifier,
									CRB_TRUE);
			variable->value.type = CRB_ASSOC_VALUE;
			variable->value.u.object_value = obj;
		}
		inter->throwed_exception = NULL;

		if (statement->u.try_s.catch_st) {
			
			crb_save_environment(inter, &recover);
				
			jmp_value = setjmp(inter->exception_jumper);
			if (jmp_value == 0) {
				result = execute_statement(inter, env, statement->u.try_s.catch_st);

				crb_recover_environment(inter, &recover);
			}
			else {
				crb_recover_environment(inter, &recover);
				throw_after_finally = 1;
			}

		}
	}


	if (statement->u.try_s.final_st)
		result = execute_statement(inter, env, statement->u.try_s.final_st);

	if (throw_after_finally)
		longjmp(inter->exception_jumper, 1);

	return result;
}

static StatementResult execute_throw_statement(CRB_Interpreter *inter,
											CRB_LocalEnvironment *env,
											Statement *statement)
{
	StatementResult result;

	CRB_Value *pv = crb_eval_and_peek_expression(inter, env, 
						statement->u.throw_s.throw_expr);

	if (pv->type != CRB_ASSOC_VALUE || crb_search_assoc_variable(
				inter, pv->u.object_value, "is_exception", 
				CRB_FALSE) == NULL) {
		crb_runtime_error(
				statement->filename,
				statement->line_number, 
				THROW_NOT_EXCEPTION_TYPE_ERR, MESSAGE_ARGUMENT_END);
	}

	exception_build_stack_trace(inter, env,
								pv->u.object_value,
								statement->line_number);

	inter->throwed_exception = pv->u.object_value;

	crb_stack_shrink_size(inter, 1);
	
	longjmp(inter->exception_jumper, 1);

	// code will never execute.
	result.type = NORMAL_STATEMENT_RESULT;
	result.u.return_value.type = CRB_NULL_VALUE;

	return result;
		
}


static void build_and_call_method_expression(CRB_Interpreter *inter,
											CRB_LocalEnvironment *env,
												char *assoc_name,
												char *method_name)
{
	Expression id_expr;
	crb_init_identifier_expression(&id_expr, assoc_name,
									__FILE__, __LINE__);
	Expression member_expr;
	crb_init_member_expression(&member_expr, &id_expr, method_name,
									__FILE__, __LINE__);
	Expression func_call_expr;
	crb_init_function_call_expression(&func_call_expr, &member_expr,
									NULL, __FILE__, __LINE__);

	crb_eval_function_call_expression(inter, env, &func_call_expr);
}
												


static StatementResult execute_foreach_statement(CRB_Interpreter *inter,
					CRB_LocalEnvironment *env, Statement *statement)
{
	StatementResult result;
	
	CRB_Value *pv = crb_eval_and_peek_expression(inter, env, 
						statement->u.foreach_s.array_expr);

	if (pv->type != CRB_ARRAY_VALUE) {
		crb_runtime_error(
				statement->filename,
				statement->line_number, 
				FOREACH_NOT_ARRAY_TYPE_ERR, MESSAGE_ARGUMENT_END);
	}
	
	char array_name[100];
	char iterator_name[100];
	
	Variable *array_var = NULL;
	Variable *iterator_var = NULL;
	Variable *assign_var = NULL;
	int id;

	id = 0;
	do {
		id++;
		// well, user cannot create variable with dot.
		sprintf(array_name, "crowbar.temp_array_%d", id);
		array_var = crb_search_scope_variable(inter, env->environ_scope,
									array_name, CRB_FALSE);
	} while (array_var != NULL);

	array_var = crb_search_scope_variable(inter, env->environ_scope,
									array_name, CRB_TRUE);

	id = 0;
	do {
		id++;
		sprintf(iterator_name, "crowbar.temp_iterator_%d", id);
		iterator_var = crb_search_scope_variable(inter, env->environ_scope,
							iterator_name, CRB_FALSE);
	} while (iterator_var != NULL);

	iterator_var = crb_search_scope_variable(inter, env->environ_scope,
									iterator_name, CRB_TRUE);

	array_var->value = *pv;
	crb_stack_shrink_size(inter, 1);

	/* set iterator = array.iterator(); */
	build_and_call_method_expression(inter, env, array_name, "iterator");
	pv = crb_stack_peek_value(inter, 0);
	iterator_var->value = *pv;
	crb_stack_shrink_size(inter, 1);

	assign_var = crb_search_local_variable(inter, env,
									statement->u.foreach_s.identifier,
									CRB_TRUE);

	result.type = NORMAL_STATEMENT_RESULT;
	result.u.return_value.type = CRB_NULL_VALUE;

	while (1) {
		/* if (iterator.is_done()) break; */
		build_and_call_method_expression(inter, env,
										iterator_name, "is_done");
		pv = crb_stack_peek_value(inter, 0);
		if (pv->u.boolean_value == CRB_TRUE) {
			crb_stack_shrink_size(inter, 1);
			break;
		}
		crb_stack_shrink_size(inter, 1);

		/* set a = iterator.current_item(); */
		build_and_call_method_expression(inter, env,
										iterator_name, "current_item");
		pv = crb_stack_peek_value(inter, 0);
		assign_var->value = *pv;
		crb_stack_shrink_size(inter, 1);

		/* execute statement */
		result = execute_statement(inter, env, 
					statement->u.foreach_s.sub_st);	

		if (result.type == RETURN_STATEMENT_RESULT)
			break;
		else if (result.type == BREAK_STATEMENT_RESULT) {
			result.type = NORMAL_STATEMENT_RESULT;
			break;
		}
		else if (result.type == CONTINUE_STATEMENT_RESULT) {
			result.type = NORMAL_STATEMENT_RESULT;
		}

		/* iterator.next(); */
		build_and_call_method_expression(inter, env,
										iterator_name, "next");
		crb_stack_shrink_size(inter, 1);

	}

	crb_remove_scope_variable(inter, env->environ_scope,
								array_name);
	crb_remove_scope_variable(inter, env->environ_scope,
								iterator_name);
	

	return result;
}


static StatementResult
execute_statement(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                  Statement *statement)
{
    StatementResult result;

    result.type = NORMAL_STATEMENT_RESULT;

    switch (statement->type) {
    case EXPRESSION_STATEMENT:
        result = execute_expression_statement(inter, env, statement);
        break;
    case GLOBAL_STATEMENT:
        result = execute_global_statement(inter, env, statement);
        break;
    case IF_STATEMENT:
        result = execute_if_statement(inter, env, statement);
        break;
    case WHILE_STATEMENT:
        result = execute_while_statement(inter, env, statement);
        break;
    case FOR_STATEMENT:
        result = execute_for_statement(inter, env, statement);
        break;
    case RETURN_STATEMENT:
        result = execute_return_statement(inter, env, statement);
        break;
    case BREAK_STATEMENT:
        result = execute_break_statement(inter, env, statement);
        break;
    case CONTINUE_STATEMENT:
        result = execute_continue_statement(inter, env, statement);
        break;
	case BLOCK_STATEMENT:
		result = crb_execute_statement_list(inter, env, 
				statement->u.block_s.block->statement_list);
		break;
	case TRY_STATEMENT:
		result = execute_try_statement(inter, env, statement);
		break;
	case THROW_STATEMENT:
		result = execute_throw_statement(inter, env, statement);
		break;
	case FOREACH_STATEMENT:
		result = execute_foreach_statement(inter, env, statement);
		break;
    case STATEMENT_TYPE_COUNT_PLUS_1:   /* FALLTHRU */
    default:
        DBG_panic(("bad case...%d", statement->type));
    }

    return result;
}

StatementResult
crb_execute_statement_list(CRB_Interpreter *inter, CRB_LocalEnvironment *env,
                           StatementList *list)
{
    StatementList *pos;
    StatementResult result;

    result.type = NORMAL_STATEMENT_RESULT;
    for (pos = list; pos; pos = pos->next) {
        result = execute_statement(inter, env, pos->statement);
        if (result.type != NORMAL_STATEMENT_RESULT)
            goto FUNC_END;
    }

  FUNC_END:
    return result;
}

