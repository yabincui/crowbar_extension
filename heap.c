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
	return object;
}


CRB_Object* crb_literal_to_crb_string(CRB_Interpreter *inter, char *str)
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


CRB_Object* crb_create_crowbar_string(CRB_Interpreter *inter, char *str)
{
	CRB_Object *object;

	object = alloc_object(inter, STRING_OBJECT);
	object->u.string.string = str;
	object->u.string.is_literal = CRB_FALSE;
	inter->heap.current_heap_size += strlen(str)+1;

	return object;
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


