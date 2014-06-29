#include <string.h>
#include "crowbar.h"

MessageFormat crb_compile_error_message_format[] = {
    {"dummy"},
    {"syntex parse error near ($(token))"},
    {"unexpected character ($(bad_char))"},
    {"function name conflict ($(name))"},
    {"dummy"},
};

MessageFormat crb_runtime_error_message_format[] = {
    {"dummy"},
    {"variable name not found ($(name))。"},
    {"function name not found ($(name))。"},
    {"argument num more than parameter num"},
    {"argument num less than parameter num"},
    {"the type of if statement conditional expression must be boolean"},
    {"the arithmetic operation must have int/double value type"},
    {"the operands type for operand $(operator) is unexpected"},
    {"operator $(operator) can not be used for boolean type"},
    {"set the filename and open mode for fopen(), both are string type"},
    {"set the file pointer for fclose()"},
    {"set the file pointer for fgets()"},
    {"set the file pointer and string for fputs()"},
    {"null can only be used for operator == and !=(not by $(operator))"},
    {"can not be divided by zero"},
    {"global variable $(name) does not exist"},
    {"can not use global statement outside function"},
    {"operator $(operator) can not be used to string type"},
	{"operand can not be lvalue."},
	{"no such method $(method_name) in object."},
	{"not valid integer value for array.resize()."},
	{"left side of index operation is not array type."},
	{"right side in [] is not int type."},
	{"array size is $(size), access index is [$(index)]."},
	{"argument type for new_array() should be nonnegative integer"},
	{"increment/decrement operand type must be lvalue integer"},
    {"dummy"},
};
