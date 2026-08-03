#include "pch.h"
#include <io.h>
#include <direct.h>
#include "FilesysUtils.h"

// Finds all the filenames in the specified directory that match the specified
// filter. Directory should be like "textures" or "Textures/".
// Filter can be nullptr or "" or "*.bmp" or "Map*" or "Map*.txt"
// Set FullFilename to true if you want results like "Textures/blah.bmp"
// or false for "blah.bmp"
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

// All four of these return a pointer into this one buffer, so only one result
// is valid at a time — holding two across a call is broken, and always was.
// The storage is a std::string now rather than a char[257], which removes the
// truncation at 256 characters and the copies that produced it, but NOT the
// sharing. Fixing that means returning by value, which is a signature change
// reaching five callers and the tests that assert a null return; it is noted
// on tasks/strings-modernised.yaml T4 rather than smuggled in here.
static std::string s_filePath;

const char* GetDirectoryPart(const char* _fullFilePath)
{
  const std::string_view path(_fullFilePath);
  const size_t finalSlash = path.find_last_of('/');
  if (finalSlash == std::string_view::npos)
    return nullptr;

  // Inclusive of the slash — callers concatenate a filename straight onto it.
  s_filePath = path.substr(0, finalSlash + 1);
  return s_filePath.c_str();
}

const char* GetFilenamePart(const char* _fullFilePath)
{
  // BEHAVIOUR DEFINED HERE, deliberately. This read `strrchr(path, '/') + 1`
  // and then tested the result for null — but the +1 happens first, so a path
  // with no slash produced 0x1, which is not null, and the copy that followed
  // read from address 1. A bare filename now returns itself, which is what the
  // name of the function promises. See tasks/strings-modernised.yaml T4.
  const std::string_view path(_fullFilePath);
  const size_t finalSlash = path.find_last_of('/');
  s_filePath = finalSlash == std::string_view::npos ? path : path.substr(finalSlash + 1);
  return s_filePath.c_str();
}

const char* GetExtensionPart(const char* _fullFilePath)
{
  // Same defect and the same fix: `strrchr(path, '.') + 1` handed back 0x1 for
  // a name with no dot, and this one did not even test it. Returns empty now,
  // NOT null — every caller passes the result straight to stricmp or a
  // BitmapRGBA constructor without checking, so null would crash them where
  // empty does not.
  const std::string_view path(_fullFilePath);
  const size_t finalDot = path.find_last_of('.');
  s_filePath = finalDot == std::string_view::npos ? std::string_view() : path.substr(finalDot + 1);
  return s_filePath.c_str();
}

const char* RemoveExtension(const char* _fullFileName)
{
  const std::string_view name(_fullFileName);
  const size_t finalDot = name.find_last_of('.');
  s_filePath = finalDot == std::string_view::npos ? name : name.substr(0, finalDot);
  return s_filePath.c_str();
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

void DeleteThisFile(const char* _filename)
{
  bool result = DeleteFile(_filename);
}

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
  : m_offsetIndex(0) { m_out = fopen(FileSys::GetFullPathA(_name).c_str(), "w"); }

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
