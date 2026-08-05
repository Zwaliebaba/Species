#pragma once

#include <string.h>


namespace Neuron
{
  void StrToLower(char* _string);

  inline char* NewStr(const char* src) { return strcpy(new char[strlen(src) + 1], src); }
} // namespace Neuron
