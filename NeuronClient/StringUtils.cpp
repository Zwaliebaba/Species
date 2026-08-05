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

  bool StrEqualsIgnoreCase(std::string_view _a, std::string_view _b)
  {
    if (_a.size() != _b.size())
      return false;
    for (size_t i = 0; i < _a.size(); ++i)
      if (tolower(static_cast<unsigned char>(_a[i])) != tolower(static_cast<unsigned char>(_b[i])))
        return false;
    return true;
  }
} // namespace Neuron
