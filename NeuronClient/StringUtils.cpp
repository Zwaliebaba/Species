#include "pch.h"

#include <ctype.h>

#include "StringUtils.h"


void StrToLower(char *_string)
{
	while (*_string != '\0')
	{
		*_string = tolower(*_string);
		_string++;
	}
}

