#include <stdio.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "crowbar.h"


void crb_check_gc(CRB_Interpreter *inter)
{
	if (inter->heap.gc_enabled > 0) {
		
		if (inter->heap.current_heap_size >= 
				inter->heap.current_threshold) {

			crb_garbage_collect(inter);

			inter->heap.current_threshold = inter->heap.current_heap_size
												+ HEAP_THRESHOLD_SIZE;
		}

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

static void gc_mark(CRB_Object *object)
{
	// attention for recursion mark
	if (!object->marked) {
		object->marked = CRB_TRUE;

		if (object->type == ARRAY_OBJECT) {
			int i;
			for (i=0; i<object->u.array.length; i++) {
				if (dkc_is_object_value(object->u.array.array[i].type))
					gc_mark(object->u.array.array[i].u.object_value);
			}
		}

	}
}

static void gc_mark_objects(CRB_Interpreter *inter)
{
	CRB_Object *p;
	CRB_LocalEnvironment *env;
	Variable *variable;
	int i;

	for (p=inter->heap.header; p!=NULL; p=p->next)
		gc_reset_mark(p);

	for (env=inter->top_env; env!=NULL; env=env->parent_env) {
		for (variable=env->variable; variable!=NULL; 
				variable=variable->next) {
			if (dkc_is_object_value(variable->value.type)) {
				gc_mark(variable->value.u.object_value);
			}
		}
	}

	for (variable=inter->variable; variable!=NULL;
				variable=variable->next) {
		if (dkc_is_object_value(variable->value.type)) {
			gc_mark(variable->value.u.object_value);
		}
	}

	for (i=0; i<inter->stack.stack_pointer; i++) {
		if (dkc_is_object_value(inter->stack.stack[i].type)) {
			gc_mark(inter->stack.stack[i].u.object_value);	
		}
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
