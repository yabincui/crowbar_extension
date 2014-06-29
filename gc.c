#include <stdio.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "crowbar.h"


void crb_check_gc(CRB_Interpreter *inter)
{
	if (inter->heap.gc_enabled > 0) {

#if 1
		crb_garbage_collect(inter);
#else

		if (inter->heap.current_heap_size >= 
				inter->heap.current_threshold) {

			crb_garbage_collect(inter);

			inter->heap.current_threshold = inter->heap.current_heap_size
												+ HEAP_THRESHOLD_SIZE;
		}
#endif

	}

}

void crb_gc_enable(CRB_Interpreter *inter)
{
	inter->heap.gc_enabled++;
	if (inter->heap.gc_enabled==1)
		crb_check_gc(inter);
}

void crb_gc_disable(CRB_Interpreter *inter)
{
	inter->heap.gc_enabled--;
}

static void gc_reset_mark(CRB_Object *object)
{
	//printf("gc_reset_mark(0x%x)\n", object);
	object->marked = CRB_FALSE;
}

static void gc_mark_object(CRB_Object *object);
static void gc_mark_value(CRB_Value *value);

static void gc_mark_value(CRB_Value *value)
{
	switch (value->type) {
	case CRB_BOOLEAN_VALUE:		// fall through
	case CRB_INT_VALUE:			// fall through
	case CRB_DOUBLE_VALUE:		// fall through
		break;
	case CRB_STRING_VALUE:
		gc_mark_object(value->u.object_value);
		break;
	case CRB_NATIVE_POINTER_VALUE:	// fall through
	case CRB_NULL_VALUE:
		break;
	case CRB_ARRAY_VALUE:
		gc_mark_object(value->u.object_value);
		break;
	case CRB_ASSOC_VALUE:
		gc_mark_object(value->u.object_value);
		break;
	case CRB_SCOPE_CHAIN_VALUE:
		gc_mark_object(value->u.object_value);
		break;
	case CRB_CLOSURE_VALUE:
		gc_mark_object(value->u.closure_value.scope_obj);
		break;
	case CRB_FAKE_METHOD_VALUE:
		gc_mark_object(value->u.fake_method.object);
		break;
	default:
		*(char*)0 = 1;
		DBG_panic(("unexpected value type : %d\n", value->type));
	}
}

static void gc_mark_object(CRB_Object *object)
{
	// attention for recursion mark

	if (object == NULL) return;

	if (!object->marked) {
		object->marked = CRB_TRUE;

		switch (object->type) {
		case STRING_OBJECT:
			break;
		case ARRAY_OBJECT:
			{
				int i;
				for (i=0; i<object->u.array.length; i++) {
					gc_mark_value(&(object->u.array.array[i]));
				}
				break;
			}
		case ASSOC_OBJECT:
			{
				Variable *member = object->u.assoc.member;
				while (member != NULL) {
					gc_mark_value(&(member->value));
					member = member->next;
				}
				break;
			}
		case SCOPE_CHAIN_OBJECT:
			{
				gc_mark_object(object->u.scope_chain.assoc_namespace);
				gc_mark_object(object->u.scope_chain.prev_scope);
				break;
			}
		case OBJECT_TYPE_COUNT_PLUS_1:
		default:
			DBG_panic(("unexpected object type: %d\n", object->type));

		}

	}
}

static void gc_mark_objects(CRB_Interpreter *inter)
{
	CRB_Object *p;
	CRB_LocalEnvironment *env;
	int i;

	for (p=inter->heap.header; p!=NULL; p=p->next)
		gc_reset_mark(p);

	//printf("gc_mark_objects:, inter->top_env=0x%x\n", inter->top_env);
	for (env=inter->top_env; env!=NULL; env=env->parent_env) {
		gc_mark_object(env->environ_scope);
	}


	for (i=0; i<inter->stack.stack_pointer; i++) {
		gc_mark_value(&(inter->stack.stack[i]));	
	}

	gc_mark_object(inter->throwed_exception);

}

static void dispose_assoc_member(CRB_Interpreter *inter, 
									Variable *member)
{
	while (member) {
		Variable *tmp_member = member;
		member = member->next;

		inter->heap.current_heap_size -= sizeof(Variable);
		MEM_free(tmp_member);
		
	}
}


static void dispose_scope_chain(CRB_Interpreter *inter,
								CRB_Object *object)
{
	//printf("dispose_scope_chain(0x%x)\n", object);
	GlobalVariableRef *ref = object->u.scope_chain.global_ref;
	while (ref) {
		GlobalVariableRef *tmp_ref = ref;
		ref = ref->next;
		MEM_free(tmp_ref);
		inter->heap.current_heap_size -= sizeof(GlobalVariableRef);
	}
}


static void gc_dispose_object(CRB_Interpreter *inter, CRB_Object *object)
{
	switch (object->type) {
	case STRING_OBJECT:
		if (!object->u.string.is_literal) {
			inter->heap.current_heap_size -= 
				(CRB_wcslen(object->u.string.string)+1) * sizeof(CRB_CHAR);
			MEM_free(object->u.string.string);
		}
		break;
	case ARRAY_OBJECT:
		if (object->u.array.array!=NULL) {
			inter->heap.current_heap_size -=
				object->u.array.alloc_size * sizeof(CRB_Value);
			MEM_free(object->u.array.array);
		}
		break;
	case ASSOC_OBJECT:
		dispose_assoc_member(inter, object->u.assoc.member);
		break;
	case SCOPE_CHAIN_OBJECT:
		dispose_scope_chain(inter, object);
		break;
	case OBJECT_TYPE_COUNT_PLUS_1:
	default:
		DBG_assert(0, ("bad type..%d\n", object->type));
	}
	inter->heap.current_heap_size -= sizeof(CRB_Object);
	MEM_free(object);
}


static void gc_sweep_objects(CRB_Interpreter *inter)
{
	CRB_Object *object, *next_obj;
	for (object = inter->heap.header; object!=NULL; object = next_obj) {
		next_obj = object->next;
		if (!object->marked) {
			if (object->prev)
				object->prev->next = object->next;
			else
				inter->heap.header = object->next;

			if (object->next)
				object->next->prev = object->prev;

			gc_dispose_object(inter, object);
		}
	}
}


void crb_garbage_collect(CRB_Interpreter *inter)
{
	//printf("before gc\n");
	gc_mark_objects(inter);
	gc_sweep_objects(inter);
	//printf("after gc\n");

	//printf("crb_garbage_collect after, heap_size..%d\n",
	//		inter->heap.current_heap_size);
}
