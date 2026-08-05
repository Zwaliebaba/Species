#pragma once

#include <stdio.h>


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

      int printf(char const* fmt, ...);
  };
} // namespace Neuron
