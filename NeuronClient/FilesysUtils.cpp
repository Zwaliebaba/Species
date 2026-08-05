#include "pch.h"
#include <io.h>
#include <direct.h>
#include "FilesysUtils.h"

// Finds all the filenames in the specified directory that match the specified
// filter. Directory should be like "textures" or "Textures/".
// Filter can be nullptr or "" or "*.bmp" or "Map*" or "Map*.txt"
// Set FullFilename to true if you want results like "Textures/blah.bmp"
// or false for "blah.bmp"


namespace Neuron
{
  std::vector<char*>* ListDirectory(const char* _dir, const char* _filter, bool _fullFilename)
  {
    if (_filter == nullptr || _filter[0] == '\0')
      _filter = "*";

    // Create a DArray for our results
    auto result = new std::vector<char*>();

    // Now add on all files found locally
    const std::string searchstring = std::string(_dir) + _filter;

    _finddata_t thisfile;
    // intptr_t, not long. _findfirst returns a 64-bit handle on x64 and long is
    // 32 bits on Windows, so storing it here truncated the handle and the
    // _findnext below was reading through whatever the truncated value pointed
    // at. It survived in the game because the handles happened to fit; the tests
    // in Tests/NeuronClientTests/FilesysUtilsTests.cpp crashed on it immediately.
    intptr_t fileindex = _findfirst(searchstring.c_str(), &thisfile);

    int exitmeplease = 0;

    while (fileindex != -1 && !exitmeplease)
    {
      if (strcmp(thisfile.name, ".") != 0 && strcmp(thisfile.name, "..") != 0 && !(thisfile.attrib & _A_SUBDIR))
      {
        // Still `new char[]`, still the caller's to delete[] — the header says
        // so and ownership/T3 is what changes it. Only the formatting moves here.
        const std::string name = _fullFilename ? std::string(_dir) + thisfile.name : std::string(thisfile.name);
        char* newname = new char[name.size() + 1];
        std::memcpy(newname, name.c_str(), name.size() + 1);

        result->push_back(newname);
      }

      exitmeplease = _findnext(fileindex, &thisfile);
    }

    if (fileindex != -1)
      _findclose(fileindex);

    return result;
  }

  std::vector<char*>* ListSubDirectoryNames(const char* _dir)
  {
    auto result = new std::vector<char*>();

    _finddata_t thisfile;
    // See ListDirectory for why this is not a long.
    intptr_t fileindex = _findfirst(_dir, &thisfile);

    int exitmeplease = 0;

    while (fileindex != -1 && !exitmeplease)
    {
      if (strcmp(thisfile.name, ".") != 0 && strcmp(thisfile.name, "..") != 0 && (thisfile.attrib & _A_SUBDIR))
      {
        char* newname = strdup(thisfile.name);
        result->push_back(newname);
      }

      exitmeplease = _findnext(fileindex, &thisfile);
    }

    if (fileindex != -1)
      _findclose(fileindex);

    return result;
  }

  bool DoesFileExist(const char* _fullPath)
  {
    FILE* f = fopen(FileSys::GetFullPathA(_fullPath).c_str(), "r");
    if (f)
    {
      fclose(f);
      return true;
    }

    return false;
  }

  // All four return by value. They used to hand back a pointer into one shared
  // static, so only one result was valid at a time and holding two across a call
  // was broken — the same hazard tasks/strings-modernised.yaml T10 removed from
  // ConvertIntToIP. T4 converted the bodies and recorded the signatures as owed;
  // T16 is that debt.
  //
  // "No directory" and "no extension" are both the empty string rather than null.
  // GetExtensionPart already worked that way (T4 chose it, because every caller
  // feeds the result straight to stricmp or a BitmapRGBA constructor without
  // checking) and GetDirectoryPart now matches it.

  std::string GetDirectoryPart(const char* _fullFilePath)
  {
    const std::string_view path(_fullFilePath);
    const size_t finalSlash = path.find_last_of('/');
    if (finalSlash == std::string_view::npos)
      return {};

    // Inclusive of the slash — callers concatenate a filename straight onto it.
    return std::string(path.substr(0, finalSlash + 1));
  }

  std::string GetFilenamePart(const char* _fullFilePath)
  {
    // BEHAVIOUR DEFINED HERE, deliberately. This read `strrchr(path, '/') + 1`
    // and then tested the result for null — but the +1 happens first, so a path
    // with no slash produced 0x1, which is not null, and the copy that followed
    // read from address 1. A bare filename now returns itself, which is what the
    // name of the function promises. See tasks/strings-modernised.yaml T4.
    const std::string_view path(_fullFilePath);
    const size_t finalSlash = path.find_last_of('/');
    return std::string(finalSlash == std::string_view::npos ? path : path.substr(finalSlash + 1));
  }

  std::string GetExtensionPart(const char* _fullFilePath)
  {
    // Same defect and the same fix: `strrchr(path, '.') + 1` handed back 0x1 for
    // a name with no dot, and this one did not even test it. Returns empty, NOT
    // null, for the reason above the first function.
    const std::string_view path(_fullFilePath);
    const size_t finalDot = path.find_last_of('.');
    return std::string(finalDot == std::string_view::npos ? std::string_view() : path.substr(finalDot + 1));
  }

  std::string RemoveExtension(const char* _fullFileName)
  {
    const std::string_view name(_fullFileName);
    const size_t finalDot = name.find_last_of('.');
    return std::string(finalDot == std::string_view::npos ? name : name.substr(0, finalDot));
  }

  bool CreateDirectory(const char* _directory)
  {
    int result = _mkdir(_directory);
    if (result == 0)
      return true; // Directory was created
    if (result == -1 && errno == 17 /* EEXIST */)
      return true; // Directory already exists
    return false;
  }

  void DeleteThisFile(const char* _filename) { bool result = DeleteFile(_filename); }

  bool IsDirectory(const char* _fullPath)
  {
    // To do
    return false;
  }

  // ***************************************************************************
  //  Class EncryptedFileWriter
  // ***************************************************************************

  static unsigned int s_offsets[] = {31, 7, 9, 1, 11, 2, 5, 5, 3, 17, 40, 12, 35, 22, 27, 2};

  EncryptedFileWriter::EncryptedFileWriter(const char* _name)
    : m_offsetIndex(0)
  {
    m_out = fopen(FileSys::GetFullPathA(_name).c_str(), "w");
  }

  EncryptedFileWriter::~EncryptedFileWriter() { fclose(m_out); }

  void EncryptedFileWriter::WriteLine(char* _line)
  {
    int len = strlen(_line);

    for (int i = 0; i < len; ++i)
    {
      if (_line[i] > 32)
      {
        m_offsetIndex++;
        m_offsetIndex %= 16;
        int j = _line[i] + s_offsets[m_offsetIndex];
        if (j >= 128)
          j -= 95;
        _line[i] = j;
      }
    }

    fprintf(m_out, "%s", _line);
  }
} // namespace Neuron
