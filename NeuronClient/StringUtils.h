#pragma once

#include <string.h>

void StrToLower(char *_string);

inline char *NewStr(const char *src)
{
	return strcpy(new char[strlen(src)+1], src);
}

