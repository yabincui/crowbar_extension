#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <limits.h>
#include <locale.h>
#include "MEM.h"
#include "DBG.h"
#include "crowbar.h"



size_t CRB_wcslen(CRB_CHAR *str)
{
	if (str==NULL)
		return 0;
	return wcslen(str);
}


CRB_CHAR* CRB_wcscpy(CRB_CHAR *dest, CRB_CHAR *src)
{
	return wcscpy(dest, src);
}


CRB_CHAR* CRB_wcsncpy(CRB_CHAR *dest, CRB_CHAR *src, size_t n)
{
	return wcsncpy(dest, src, n);
}


int CRB_wcscmp(CRB_CHAR *s1, CRB_CHAR *s2)
{
	return wcscmp(s1, s2);
}


CRB_CHAR* CRB_wcscat(CRB_CHAR *s1, CRB_CHAR *s2)
{
	return wcscat(s1, s2);
}


int CRB_mbstowcs_len(const char *src)
{
	int src_idx, dest_idx;
	mbstate_t st;

	memset(&st, 0, sizeof(st));

	for (src_idx=dest_idx=0; src && src[src_idx]!='\0'; ) {
		int ret = mbrtowc(NULL, src+src_idx, MB_LEN_MAX, &st);
		if (ret<0) return ret;
		dest_idx++;
		src_idx += ret;

	}

	return dest_idx;
}



int CRB_mbstowcs(CRB_CHAR *dest, const char *src)
{
	int src_idx, dest_idx;
	int status;
	mbstate_t st;

	memset(&st, 0, sizeof(st));
	for (src_idx=dest_idx=0; src && src[src_idx]!='\0'; ) {
		status = mbrtowc(&dest[dest_idx], src+src_idx, MB_LEN_MAX, &st);
		if (status<0) return status;
		dest_idx++;
		src_idx += status;
	}
	dest[dest_idx] = L'\0';
	return dest_idx;
}



CRB_CHAR* CRB_mbstowcs_alloc(int line_number, const char *src)
{
	int len;
	CRB_CHAR *dest;

	len = CRB_mbstowcs_len(src);
	if (len < 0) {
		crb_runtime_error(line_number, BAD_MULTIBYTE_CHARACTER_ERR,
				MESSAGE_ARGUMENT_END);
		return NULL;
	}

	dest = (CRB_CHAR*)MEM_malloc(sizeof(CRB_CHAR)*(len+1));
	CRB_mbstowcs(dest, src);
	return dest;
}


int CRB_wcstombs_len(const CRB_CHAR *src)
{
	int src_idx, dest_idx;
	int status;
	char dummy[12];
	mbstate_t st;

	memset(&st, 0, sizeof(st));
	for (src_idx = dest_idx = 0; src && src[src_idx] != L'\0'; ) {
		status = wcrtomb(dummy, src[src_idx], &st);
		if (status < 0) return status;
		src_idx++;
		dest_idx += status;
	}
	return dest_idx;
}



int CRB_wcstombs(char *dest, const CRB_CHAR *src)
{
	int src_idx, dest_idx;
	int status;
	mbstate_t st;

	memset(&st, 0, sizeof(st));
	for (src_idx = dest_idx = 0; src && src[src_idx] != L'\0'; ) {
		status = wcrtomb(dest + dest_idx, src[src_idx], &st);
		if (status < 0) return status;
		src_idx++;
		dest_idx += status;
	}
	dest[dest_idx] = '\0';
	return dest_idx;

}


char* CRB_wcstombs_alloc(int line_number, const CRB_CHAR *src)
{
	int len;
	char *dest;

	len = CRB_wcstombs_len(src);
	if (len < 0) {
		crb_runtime_error(line_number, BAD_CRB_CHARACTER_ERR,
				MESSAGE_ARGUMENT_END);
		return NULL;
	}
	dest = (char*) MEM_malloc(sizeof(char)*(len+1));
	CRB_wcstombs(dest, src);
	return dest;
}


char CRB_wctochar(CRB_CHAR src)
{
	mbstate_t st;
	int status;
	char dest;

	memset(&st, 0, sizeof(st));
	status = wcrtomb(&dest, src, &st);
	DBG_assert(status==1, ("wcrtomb status..%d\n", status));

	return dest;

}



int CRB_print_wcs(FILE *fp, CRB_CHAR *str)
{
	char *tmp;
	int mb_len;
	int result;

	mb_len = CRB_wcstombs_len(str);
	if (mb_len < 0)
		return mb_len;
	tmp = (char*)MEM_malloc(sizeof(char)*(mb_len+1));
	CRB_wcstombs(tmp, str);

	result = fprintf(fp, "%s", tmp);
	MEM_free(tmp);

	return result;
}



int CRB_println_wcs(FILE *fp, CRB_CHAR *str)
{
	char *tmp;
	int mb_len;
	int result;

	mb_len = CRB_wcstombs_len(str);
	if (mb_len < 0)
		return mb_len;
	tmp = (char*)MEM_malloc(sizeof(char)*(mb_len+1));
	CRB_wcstombs(tmp, str);

	result = fprintf(fp, "%s\n", tmp);
	MEM_free(tmp);
	return result;
}


Encoding CRB_set_encoding(Encoding type)
{
	const char *type_str = NULL;

	switch (type) {
	case NO_ENCODING:
		type_str = "";
		break;
	case EN_ENCODING:
		type_str = "en_US.utf8";
		break;
	case UTF8_ENCODING:
		type_str = "zh_CN.utf8";
		break;
	case GBK_ENCODING:
		type_str = "zh_CN.gb18030";
		break;
	default:
		DBG_panic(("unexpected Encoding..%d", type));

	}

	char *ret = setlocale(LC_CTYPE, type_str);

	DBG_assert(ret!=NULL, ("unfulfilled setlocale request for %s", type_str));

	Encoding old_type = NO_ENCODING;
	if (strcmp(ret, "en_US.utf8")==0)
		old_type = EN_ENCODING;
	else if (strcmp(ret, "zh_CN.utf8")==0)
		old_type = UTF8_ENCODING;
	else if (strcmp(ret, "zh_CN.gb18030")==0)
		old_type = GBK_ENCODING;
	
	return old_type;

}
