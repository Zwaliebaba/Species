#include "pch.h"

#include <ctype.h>

#include "StringUtils.h"


namespace Neuron
{
  void StrToLower(char* _string)
  {
    while (*_string != '\0')
    {
      *_string = tolower(*_string);
      _string++;
    }
  }
} // namespace Neuron
