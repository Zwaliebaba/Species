#pragma once

#include <stdio.h>

#include <format>
#include <string>
#include <string_view>
#include <utility>


namespace Neuron
{
  class FileWriter
  {
    protected:
      int m_offsetIndex;
      FILE* m_file;
      bool m_encrypt;

    public:
      FileWriter(char const* _filename, bool _encrypt);
      ~FileWriter();

      // Two overloads, the same split TextRenderer took in strings-modernised
      // T12 and for the same reasons:
      //
      //   printf("Landscape_StartDefinition\n")   the plain overload. The text
      //                                           is TEXT — a percent sign in
      //                                           it is a percent sign.
      //   printf("\tRoute {}\n", id)              the FORMATTING template. The
      //                                           format string is checked
      //                                           against the argument types at
      //                                           COMPILE time.
      //
      // THE TEMPLATE REQUIRES AT LEAST ONE ARGUMENT, which is what keeps the
      // two unambiguous: with only the text the template is not viable and the
      // plain overload takes the call.
      //
      // What went with the variadic entry point this replaces: a char[10240] on
      // the stack that vsprintf wrote into with no bound, and which the
      // encryption loop below then followed for `len` bytes. Nothing on the
      // save path bounds a level name, a shape name or a script filename, so
      // that buffer was one long name away from corrupting the stack and having
      // the encryption walk after it.
      int printf(std::string_view _text);

      template <typename T, typename... Args> int printf(std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args)
      {
        std::string const text = std::format(_fmt, std::forward<T>(_arg), std::forward<Args>(_args)...);
        return printf(std::string_view(text));
      }
  };
} // namespace Neuron
