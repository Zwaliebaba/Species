#pragma once

#include <string.h>

#include <string_view>


namespace Neuron
{
  void StrToLower(char* _string);

  inline char* NewStr(const char* src) { return strcpy(new char[strlen(src) + 1], src); }

  // CASE-INSENSITIVE EQUALITY, and it exists because std::string's own == is
  // not. strings-modernised T11 converted the Eclipse widget names and captions
  // from char arrays to std::string, and a dozen call sites in the UI compared
  // them with stricmp rather than strcmp — a button matched on its caption
  // whatever case the language file spelled it in. Replacing stricmp with ==
  // during a sweep would have made each of those stop matching, silently and
  // only for the spellings that differ in case.
  //
  // Eclipse's OWN lookup is case-SENSITIVE and stays that way: EclGetWindow and
  // EclWindow::GetButton used strcmp and now use ==. This is for the callers
  // that deliberately did not.
  bool StrEqualsIgnoreCase(std::string_view _a, std::string_view _b);
} // namespace Neuron
