#ifndef PUBLIC_CRB_DEV_H_INCLUDED
#define PUBLIC_CRB_DEV_H_INCLUDED
#include "CRB.h"

typedef wchar_t CRB_CHAR;

typedef enum {
    CRB_FALSE = 0,
    CRB_TRUE = 1
} CRB_Boolean;

typedef struct CRB_String_tag CRB_String;
typedef struct CRB_LocalEnvironment_tag CRB_LocalEnvironment;
typedef struct Variable_tag Variable;
typedef struct CRB_Object_tag CRB_Object;

typedef struct {
    char        *name;
} CRB_NativePointerInfo;

typedef enum {
    CRB_BOOLEAN_VALUE = 1,
    CRB_INT_VALUE,
    CRB_DOUBLE_VALUE,
    CRB_STRING_VALUE,
    CRB_NATIVE_POINTER_VALUE,
    CRB_NULL_VALUE,
	CRB_ARRAY_VALUE
} CRB_ValueType;

typedef struct {
    CRB_NativePointerInfo       *info;
    void                        *pointer;
} CRB_NativePointer;

typedef struct {
    CRB_ValueType       type;
    union {
        CRB_Boolean     boolean_value;
        int             int_value;
        double          double_value;
        CRB_Object      *object_value;
        CRB_NativePointer       native_pointer;
    } u;
} CRB_Value;

typedef void CRB_NativeFunctionProc(CRB_Interpreter *interpreter,
                                    CRB_LocalEnvironment *env,     
									int arg_count);

void CRB_add_native_function(CRB_Interpreter *interpreter,
                             char *name, CRB_NativeFunctionProc *proc);
Variable* CRB_add_global_variable(CRB_Interpreter *inter,
                             char *identifier, CRB_Value *value);

#endif /* PUBLIC_CRB_DEV_H_INCLUDED */
