#include "pch.h"

#include "Debug.h"
#include "FileWriter.h"


namespace Neuron
{
  static unsigned int s_offsets[] = {31, 7, 9, 1, 11, 2, 5, 5, 3, 17, 40, 12, 35, 22, 27, 2};


  FileWriter::FileWriter(char const* _filename, bool _encrypt)
    : m_offsetIndex(0),
      m_encrypt(_encrypt)
  {
    m_file = fopen(FileSys::GetFullPathA(_filename).c_str(), "w");

    ASSERT_TEXT(m_file, "Couldn't create file {}", _filename);

    if (_encrypt)
    {
      fprintf(m_file, "redshirt2");
    }
  }


  FileWriter::~FileWriter() { fclose(m_file); }


  int FileWriter::printf(std::string_view _text)
  {
    // The encryption is a walk over the bytes about to be written, so it needs
    // storage it may modify. The char[10240] this replaces was the only reason
    // the old entry point had a length limit at all.
    std::string text(_text);

    if (m_encrypt)
    {
      for (char& c : text)
      {
        // Signed char on purpose: a byte above 127 is NEGATIVE here and so is
        // left alone, exactly as it was when this read buf[i] > 32.
        if (c > 32)
        {
          m_offsetIndex++;
          m_offsetIndex %= 16;
          int j = c + s_offsets[m_offsetIndex];
          if (j >= 128)
            j -= 95;
          c = static_cast<char>(j);
        }
      }
    }

    return fprintf(m_file, "%s", text.c_str());
  }
} // namespace Neuron
