#include <stdio.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "crowbar.h"

static CRB_Interpreter *st_current_interpreter;

CRB_Interpreter *
crb_get_current_interpreter(void)
{
    return st_current_interpreter;
}

void
crb_set_current_interpreter(CRB_Interpreter *inter)
{
    st_current_interpreter = inter;
}

/* BUGBUG
CRB_NativeFunctionProc *
crb_search_native_function(CRB_Interpreter *inter, char *name)
{
    NativeFunction *pos;

    for (pos = inter->native_function; pos; pos = pos->next) {
        if (!strcmp(pos->name, name))
            break;
    }
    if (pos) {
        return pos->proc;
    } else {
        return NULL;
    }
}
*/

FunctionDefinition *
crb_search_function(char *name)
{
    FunctionDefinition *pos;
    CRB_Interpreter *inter;

    inter = crb_get_current_interpreter();
    for (pos = inter->function_list; pos; pos = pos->next) {
        if (!strcmp(pos->name, name))
            break;
    }
    return pos;
}

void *
crb_malloc(size_t size)
{
    void *p;
    CRB_Interpreter *inter;

    inter = crb_get_current_interpreter();
    p = MEM_storage_malloc(inter->interpreter_storage, size);

    return p;
}

void *
crb_execute_malloc(CRB_Interpreter *inter, size_t size)
{
    void *p;

    p = MEM_storage_malloc(inter->execute_storage, size);

    return p;
}


Variable* CRB_add_global_variable(CRB_Interpreter *inter,
					char *identifier, CRB_Value *pvalue)
{
	Variable *variable = crb_search_scope_variable(inter,
							inter->first_env.environ_scope,
							identifier, CRB_TRUE);
	variable->value = *pvalue;
	return variable;
}

Variable* crb_search_global_variable(CRB_Interpreter *inter,
					char *identifier)
{
	return crb_search_scope_variable(inter,
						inter->first_env.environ_scope,
						identifier, CRB_FALSE);
}


Variable* crb_search_local_variable(CRB_Interpreter *inter,
					CRB_LocalEnvironment *env,
					char *identifier, CRB_Boolean can_create)
{
	CRB_Object *scope = env->environ_scope;
	CRB_Boolean is_closure = scope->u.scope_chain.is_closure;
	Variable *variable = NULL;

	//printf("before crb_search_local_variable(%s)\n", identifier);

	variable = crb_search_scope_variable(inter, scope, identifier,
											CRB_FALSE);
	if (!variable && is_closure) {
		while (1) {
			CRB_Object *prev_scope = scope->u.scope_chain.prev_scope;
			scope = prev_scope;
			if (scope == NULL) break;
			variable = crb_search_scope_variable(inter, scope, identifier,
													CRB_FALSE);
			if (variable) break;
		}
	}
	else if (!variable && !is_closure) {
		CRB_Object *global_scope = inter->first_env.environ_scope;

		variable = crb_search_scope_variable(inter, global_scope, 
										identifier, CRB_FALSE);
				
	}

	if (!variable && can_create) {
		scope = inter->top_env->environ_scope;
		variable = crb_search_scope_variable(inter, scope, identifier,
													CRB_TRUE);
	}
	
	//printf("after crb_search_local_variable(%s) = 0x%x\n", identifier,
	//							variable);
	
	return variable;
}


char *
crb_get_operator_string(ExpressionType type)
{
    char        *str;

    switch (type) {
    case BOOLEAN_EXPRESSION:    /* FALLTHRU */
    case INT_EXPRESSION:        /* FALLTHRU */
    case DOUBLE_EXPRESSION:     /* FALLTHRU */
    case STRING_EXPRESSION:     /* FALLTHRU */
    case IDENTIFIER_EXPRESSION:
        DBG_panic(("bad expression type..%d\n", type));
        break;
    case ASSIGN_EXPRESSION:
        str = "=";
        break;
    case ADD_EXPRESSION:
        str = "+";
        break;
    case SUB_EXPRESSION:
        str = "-";
        break;
    case MUL_EXPRESSION:
        str = "*";
        break;
    case DIV_EXPRESSION:
        str = "/";
        break;
    case MOD_EXPRESSION:
        str = "%";
        break;
    case LOGICAL_AND_EXPRESSION:
        str = "&&";
        break;
    case LOGICAL_OR_EXPRESSION:
        str = "||";
        break;
    case EQ_EXPRESSION:
        str = "==";
        break;
    case NE_EXPRESSION:
        str = "!=";
        break;
    case GT_EXPRESSION:
        str = ">";
        break;
    case GE_EXPRESSION:
        str = ">=";
        break;
    case LT_EXPRESSION:
        str = "<";
        break;
    case LE_EXPRESSION:
        str = "<=";
        break;
    case MINUS_EXPRESSION:
        str = "-";
        break;
    case FUNCTION_CALL_EXPRESSION:  /* FALLTHRU */
    case NULL_EXPRESSION:  /* FALLTHRU */
    case EXPRESSION_TYPE_COUNT_PLUS_1:
    default:
        DBG_panic(("bad expression type..%d\n", type));
    }

    return str;
}


void crb_vstr_clear(VString *v)
{
	v->string = NULL;
}

static int my_strlen(CRB_CHAR *str)
{
	if (str==NULL)
		return 0;
	return CRB_wcslen(str);
}

void crb_vstr_append_string(VString *v, char *str)
{
	CRB_CHAR *wstr = CRB_mbstowcs_alloc(__FILE__, __LINE__, str);
	crb_vstr_append_wstring(v, wstr);
	MEM_free(wstr);
}

void crb_vstr_append_character(VString *v, char ch)
{
	char s[2];
	s[0] = ch; s[1] = '\0';
	crb_vstr_append_string(v, s);
}


void crb_vstr_append_wstring(VString *v, CRB_CHAR *str)
{
	int new_len, old_len;

	old_len = CRB_wcslen(v->string);
	new_len = old_len + my_strlen(str);
	v->string = MEM_realloc(v->string, (new_len+1)*sizeof(CRB_CHAR));
	CRB_wcscpy(&v->string[old_len], str);
}


void crb_vstr_append_wcharacter(VString *v, CRB_CHAR ch)
{
	int new_len, old_len;

	old_len = my_strlen(v->string);
	new_len = old_len + 1;
	v->string = MEM_realloc(v->string, (new_len+1)*sizeof(CRB_CHAR));
	v->string[old_len] = ch;
	v->string[old_len+1] = L'\0';
}


CRB_CHAR* CRB_value_to_string(CRB_Value *value)
{
	VString vstr;
	char buf[LINE_BUF_SIZE];
	int i;

	crb_vstr_clear(&vstr);
	
	switch (value->type) {
	case CRB_BOOLEAN_VALUE:
		if (value->u.boolean_value)
			crb_vstr_append_string(&vstr, "true");
		else
			crb_vstr_append_string(&vstr, "false");
		break;
	case CRB_INT_VALUE:
		sprintf(buf, "%d", value->u.int_value);
		crb_vstr_append_string(&vstr, buf);
		break;
	case CRB_DOUBLE_VALUE:
		sprintf(buf, "%lf", value->u.double_value);
		crb_vstr_append_string(&vstr, buf);
		break;
	case CRB_STRING_VALUE:
		crb_vstr_append_wstring(&vstr, 
					value->u.object_value->u.string.string);
		break;
	case CRB_NATIVE_POINTER_VALUE:
		if (value->u.native_pointer.info == crb_get_regexp_info()) {
			CRB_Regexp *regexp = value->u.native_pointer.pointer;
			sprintf(buf, "%%r%c", regexp->protect_char);
			crb_vstr_append_string(&vstr, buf);
			crb_vstr_append_wstring(&vstr, regexp->pattern);
			sprintf(buf, "%c", regexp->protect_char);
			crb_vstr_append_string(&vstr, buf);
		}
		else {
			sprintf(buf, "(%s:%p)",
				value->u.native_pointer.info->name,
				value->u.native_pointer.pointer);
			crb_vstr_append_string(&vstr, buf);
		}
		break;
	case CRB_NULL_VALUE:
		crb_vstr_append_string(&vstr, "null");
		break;
	case CRB_ARRAY_VALUE:
		crb_vstr_append_string(&vstr, "(");
		for (i=0; i<value->u.object_value->u.array.length; i++) {
			CRB_CHAR *new_str;
			if (i>0)
				crb_vstr_append_string(&vstr, ", ");
			new_str = CRB_value_to_string(
					&value->u.object_value->u.array.array[i]);
			crb_vstr_append_wstring(&vstr, new_str);
			MEM_free(new_str);
		}
		crb_vstr_append_string(&vstr, ")");
		break;
	case CRB_ASSOC_VALUE:
		{
			crb_vstr_append_string(&vstr, "{");
			Variable *variable;
			for (variable = value->u.object_value->u.assoc.member;
					variable != NULL; variable = variable->next) {
				sprintf(buf, " %s : ", variable->name);
				crb_vstr_append_string(&vstr, buf);
				CRB_CHAR *new_str = CRB_value_to_string(
										&(variable->value));
				crb_vstr_append_wstring(&vstr, new_str);
				MEM_free(new_str);
				if (variable->next != NULL)
					crb_vstr_append_string(&vstr, ", ");
			}
			crb_vstr_append_string(&vstr, "}");

			break;
		}
	case CRB_SCOPE_CHAIN_VALUE:
		{
			crb_vstr_append_string(&vstr, "ScopeChain");
			break;
		}
	case CRB_CLOSURE_VALUE:
		{
			crb_vstr_append_string(&vstr, "CLOSURE");
			break;
		}
	case CRB_FAKE_METHOD_VALUE:
		{
			crb_vstr_append_string(&vstr, "FAKE_METHOD");
			break;
		}
	default:
		DBG_panic(("value->type..%d\n", value->type));
	}
	return vstr.string;
}


CRB_ValueType crb_object_type_to_value_type(ObjectType type)
{
	CRB_ValueType ret_type;

	switch (type) {
	case STRING_OBJECT:
		ret_type = CRB_STRING_VALUE; break;
	case ARRAY_OBJECT:
		ret_type = CRB_ARRAY_VALUE; break;
	case ASSOC_OBJECT:
		ret_type = CRB_ASSOC_VALUE; break;
	case SCOPE_CHAIN_OBJECT:
		ret_type = CRB_SCOPE_CHAIN_VALUE; break;
	case OBJECT_TYPE_COUNT_PLUS_1:  // fall through
	default:
		DBG_panic(("unexpected object_type: %d\n", type));
	}

	return ret_type;
}
