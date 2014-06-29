#include <stdio.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "crowbar.h"


static CRB_Object* alloc_object(CRB_Interpreter *inter, ObjectType type)
{
	CRB_Object *object;

	crb_check_gc(inter);
	object = MEM_malloc(sizeof(CRB_Object));
	inter->heap.current_heap_size += sizeof(CRB_Object);
	object->type = type;
	object->marked = CRB_FALSE;
	object->prev = NULL;
	object->next = inter->heap.header;
	inter->heap.header = object;
	//printf("inter->heap.header = 0x%x\n", inter->heap.header);
	if (object->next)
		object->next->prev = object;

	//printf("object(0x%x) , type = %d\n", object, type);

	return object;
}


CRB_Object* crb_literal_to_crb_string(CRB_Interpreter *inter, CRB_CHAR *str)
{
	CRB_Object *object;

	object = alloc_object(inter, STRING_OBJECT);
	object->u.string.string = str;
	object->u.string.is_literal = CRB_TRUE;
	return object;
}

/*
void crb_refer_string(CRB_Object *object)
{
	DBG_assert(object->type==STRING_OBJECT, 
			("crb_refer_string with object type..%d\n",
			object->type));

	object->u.string.ref_count++;
	
}


void crb_release_string(CRB_Interpreter *inter, CRB_Object *object)
{

	DBG_assert(object->type==STRING_OBJECT, 
			("crb_release_string with object type..%d\n",
			object->type));
	
	DBG_assert(object->u.string.ref_count >= 1,
			("crb_release_string with ref_count..%d\n",
			 object->u.string.ref_count));

	if (--object->u.string.ref_count == 0) {
		if (!object->u.string.is_literal) {
			inter->heap.current_heap_size -= 
						strlen(object->u.string.string)+1;
			MEM_free(object->u.string.string);
		}
		if (object->prev) object->prev->next = object->next;
		else inter->heap.header = object->next;

		if (object->next) object->next->prev = object->prev;
		object->prev = object->next = NULL;
		MEM_free(object);
		inter->heap.current_heap_size -= sizeof(CRB_Object);
	}

}

*/


CRB_Object* crb_create_crowbar_string(CRB_Interpreter *inter, CRB_CHAR *str)
{
	CRB_Object *object;

	object = alloc_object(inter, STRING_OBJECT);
	object->u.string.string = str;
	object->u.string.is_literal = CRB_FALSE;
	inter->heap.current_heap_size += sizeof(CRB_CHAR) * (CRB_wcslen(str)+1);

	return object;
}

CRB_Object* crb_string_substr(CRB_Interpreter *inter, CRB_Object *obj,
								int begin, int len)
{
	DBG_assert(obj->type == STRING_OBJECT, (""));
	int orig_len = CRB_wcslen(obj->u.string.string);
	DBG_assert(orig_len >= begin + len, (""));

	CRB_CHAR *str = MEM_malloc(sizeof(CRB_CHAR)*(len+1));
	CRB_wcsncpy(str, obj->u.string.string + begin, len);
	str[len] = L'\0';

	return crb_create_crowbar_string(inter, str);

}


CRB_Object* crb_create_array(CRB_Interpreter *inter, int array_size)
{
	CRB_Object *object;

	object = alloc_object(inter, ARRAY_OBJECT);
	object->u.array.length = array_size;
	object->u.array.alloc_size = array_size;
	object->u.array.array = MEM_malloc(sizeof(CRB_Value)*array_size);
	inter->heap.current_heap_size += sizeof(CRB_Value)*array_size;

	int i;
	for (i=0; i<array_size; i++)
		object->u.array.array[i].type = CRB_NULL_VALUE;

	return object;
}

void crb_array_add(CRB_Interpreter *inter, CRB_Object *obj, CRB_Value *val)
{
	int new_size;

	//printf("before crb_array_add\n");

	DBG_assert(obj->type == ARRAY_OBJECT, ("bad type..%d\n", obj->type));

	crb_check_gc(inter);

	//printf("length=%d, alloc_size=%d\n", 
	//		obj->u.array.length, obj->u.array.alloc_size);

	if (obj->u.array.length + 1 > obj->u.array.alloc_size) {
		new_size = obj->u.array.alloc_size * 2;
		if (new_size == 0 || 
				new_size - obj->u.array.alloc_size > ARRAY_ALLOC_SIZE)
			new_size = obj->u.array.alloc_size + ARRAY_ALLOC_SIZE;

	//	printf("crb_array_add, length=%d, alloc_size=%d, new_size=%d\n",
	//			obj->u.array.length, obj->u.array.alloc_size,
	//			new_size);

		obj->u.array.array = MEM_realloc(obj->u.array.array,
										new_size * sizeof(CRB_Value));
		inter->heap.current_heap_size += 
				(new_size - obj->u.array.alloc_size)*sizeof(CRB_Value);
		obj->u.array.alloc_size = new_size;
	}

	obj->u.array.array[obj->u.array.length++] = *val;
	
	//printf("after crb_array_add\n");

}


int crb_array_size(CRB_Interpreter *inter, CRB_Object *obj)
{
	return obj->u.array.length;
}


void crb_array_resize(CRB_Interpreter *inter, CRB_Object *obj, int new_size)
{
	int new_alloc_size;
	CRB_Boolean need_realloc;

	DBG_assert(new_size >=0, (""));
	
	crb_check_gc(inter);
	if (new_size > obj->u.array.alloc_size) {
		new_alloc_size = obj->u.array.alloc_size * 2;
		if (new_alloc_size < new_size)
			new_alloc_size = new_size + ARRAY_ALLOC_SIZE;
		else if (new_alloc_size - new_size > ARRAY_ALLOC_SIZE)
			new_alloc_size = new_size + ARRAY_ALLOC_SIZE;

		need_realloc = CRB_TRUE;
	}
	else  if (new_size < obj->u.array.alloc_size - ARRAY_ALLOC_SIZE) {
		new_alloc_size = new_size;
		need_realloc = CRB_TRUE;
	}
	else {
		need_realloc = CRB_FALSE;
	}

	if (need_realloc) {
		crb_check_gc(inter);
		obj->u.array.array = MEM_realloc(obj->u.array.array,
								new_alloc_size * sizeof(CRB_Value));
		inter->heap.current_heap_size += 
			(new_alloc_size - obj->u.array.alloc_size)*sizeof(CRB_Value);
		obj->u.array.alloc_size = new_alloc_size;
	}

	int i;
	for (i=obj->u.array.length; i<new_size; i++)
		obj->u.array.array[i].type = CRB_NULL_VALUE;

	obj->u.array.length = new_size;

}

void crb_array_set(CRB_Interpreter *inter, CRB_Object *obj,
					int pos, CRB_Value *val)
{
	DBG_assert(pos >= 0 && pos < obj->u.array.length, (""));
	obj->u.array.array[pos] = *val;
}

void crb_array_insert(CRB_Interpreter *inter, CRB_Object *obj,
					int pos, CRB_Value *val)
{
	DBG_assert(pos >= 0 && pos <= obj->u.array.length, (""));

	crb_array_resize(inter, obj, obj->u.array.length+1);
	int i;
	for (i = obj->u.array.length-1; i > pos; i--) {
		obj->u.array.array[i] = obj->u.array.array[i-1];
	}
	obj->u.array.array[i] = *val;
}

void crb_array_remove(CRB_Interpreter *inter, CRB_Object *obj,
					int pos)
{
	DBG_assert(pos >= 0 && pos < obj->u.array.length, (""));

	int i;
	for (i = pos; i < obj->u.array.length-1; i++) {
		obj->u.array.array[i] = obj->u.array.array[i+1];
	}
	crb_array_resize(inter, obj, obj->u.array.length-1);
}


CRB_Object* crb_create_assoc(CRB_Interpreter *inter)
{
	CRB_Object *obj = alloc_object(inter, ASSOC_OBJECT);
	obj->u.assoc.member_count = 0;
	obj->u.assoc.member = NULL;
	return obj;
}


CRB_Object* crb_create_scope_chain(CRB_Interpreter *inter,
									CRB_Object *prev_scope,
									CRB_Boolean is_closure)
{
	CRB_Value value;

	value.type = CRB_SCOPE_CHAIN_VALUE;

	CRB_Object *obj = alloc_object(inter, SCOPE_CHAIN_OBJECT);

	value.u.object_value = obj;
	/* protect the obj from gc */
	crb_stack_push_value(inter, &value);

	obj->u.scope_chain.assoc_namespace = NULL;
	obj->u.scope_chain.global_ref = NULL;
	obj->u.scope_chain.prev_scope = prev_scope;
	obj->u.scope_chain.is_closure = is_closure;
	
	obj->u.scope_chain.assoc_namespace = crb_create_assoc(inter);

	crb_stack_shrink_size(inter, 1);

	return obj;
}


CRB_Boolean crb_add_scope_global_ref(CRB_Interpreter *inter, 
								CRB_Object *scope,
								char *identifier)
{
	GlobalVariableRef *ref_pos;

	for (ref_pos = scope->u.scope_chain.global_ref; 
				ref_pos != NULL; ref_pos = ref_pos->next) {
		if (!strcmp(ref_pos->variable->name, identifier))
			break;
	}

	if (ref_pos == NULL) {
		GlobalVariableRef *new_ref;
		Variable *variable;

		variable = crb_search_global_variable(inter, identifier);
		if (variable == NULL) {
			return CRB_FALSE;
		}

		new_ref = MEM_malloc(sizeof(GlobalVariableRef));
		new_ref->variable = variable;
		new_ref->next = scope->u.scope_chain.global_ref;
		scope->u.scope_chain.global_ref = new_ref;
		inter->heap.current_heap_size += sizeof(GlobalVariableRef);
	}

	return CRB_TRUE;

}


Variable* crb_search_assoc_variable(CRB_Interpreter *inter,
										CRB_Object *assoc,
										char *identifier,
										CRB_Boolean can_create)
{
	Variable *member;
	for (member = assoc->u.assoc.member; member != NULL;
			member = member->next) {
		if (strcmp(member->name, identifier)==0)
			return member;
	}

	if (can_create) {
		member = MEM_malloc(sizeof(Variable));
		member->name = identifier;
		member->value.type = CRB_NULL_VALUE;
		member->next = assoc->u.assoc.member;
		assoc->u.assoc.member = member;
		assoc->u.assoc.member_count++;
		inter->heap.current_heap_size += sizeof(Variable);
		return member;
	}

	return NULL;
}

void crb_set_assoc_variable(CRB_Interpreter *inter,
							CRB_Object *assoc,
							char *identifier, CRB_Value value)
{
	Variable *variable = crb_search_assoc_variable(inter, assoc,
											identifier, CRB_TRUE);

	variable->value = value;
}


Variable* crb_search_scope_variable(CRB_Interpreter *inter,
										CRB_Object *scope,
										char *identifier,
										CRB_Boolean can_create)
{
	GlobalVariableRef *ref_pos;
	//printf("crb_search_scope_variable(%s)\n", identifier);

	for (ref_pos = scope->u.scope_chain.global_ref;
			ref_pos != NULL; ref_pos = ref_pos->next) {
		if (strcmp(ref_pos->variable->name, identifier)==0) {
			return ref_pos->variable;
		}
	}

	return crb_search_assoc_variable(inter, 
				scope->u.scope_chain.assoc_namespace, identifier,
				can_create);

}

void crb_remove_assoc_variable(CRB_Interpreter *inter,
							CRB_Object *assoc,
							char *identifier)
{
	Variable *prev_var, *var;
	prev_var = NULL;

	for (var = assoc->u.assoc.member; var != NULL; 
						prev_var = var, var = var->next) {
		if (strcmp(var->name, identifier)==0)
			break;
	}
	if (var != NULL) {
		if (prev_var == NULL)
			assoc->u.assoc.member = var->next;
		else
			prev_var->next = var->next;

		assoc->u.assoc.member_count--;
		MEM_free(var);
		inter->heap.current_heap_size -= sizeof(Variable);
	}
}


void crb_remove_scope_variable(CRB_Interpreter *inter,
							CRB_Object *scope,
							char *identifier)
{
	CRB_Object *assoc = scope->u.scope_chain.assoc_namespace;
	if (assoc)
		crb_remove_assoc_variable(inter, assoc, identifier);

}
