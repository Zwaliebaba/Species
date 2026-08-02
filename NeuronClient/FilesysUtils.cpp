#include "pch.h"
#include <io.h>
#include <direct.h>
#include "FilesysUtils.h"

// Finds all the filenames in the specified directory that match the specified
// filter. Directory should be like "textures" or "Textures/".
// Filter can be NULL or "" or "*.bmp" or "Map*" or "Map*.txt"
// Set FullFilename to true if you want results like "Textures/blah.bmp"
// or false for "blah.bmp"
LList<char*>* ListDirectory(const char* _dir, const char* _filter, bool _fullFilename)
{
  if (_filter == nullptr || _filter[0] == '\0')
    _filter = "*";

  // Create a DArray for our results
  auto result = new LList<char*>();

  // Now add on all files found locally
  char searchstring[256];
  DEBUG_ASSERT(strlen(_dir) + strlen(_filter) < sizeof(searchstring) - 1);
  sprintf(searchstring, "%s%s", _dir, _filter);

  _finddata_t thisfile;
  long fileindex = _findfirst(searchstring, &thisfile);

  int exitmeplease = 0;

  while (fileindex != -1 && !exitmeplease)
  {
    if (strcmp(thisfile.name, ".") != 0 && strcmp(thisfile.name, "..") != 0 && !(thisfile.attrib & _A_SUBDIR))
    {
      char* newname = nullptr;
      if (_fullFilename)
      {
        int len = strlen(_dir) + strlen(thisfile.name);
        newname = new char [len + 1];
        sprintf(newname, "%s%s", _dir, thisfile.name);
      }
      else
      {
        int len = strlen(thisfile.name);
        newname = new char [len + 1];
        sprintf(newname, "%s", thisfile.name);
      }

      result->PutData(newname);
    }

    exitmeplease = _findnext(fileindex, &thisfile);
  }
  return result;
}

LList<char*>* ListSubDirectoryNames(const char* _dir)
{
  auto result = new LList<char*>();

  _finddata_t thisfile;
  long fileindex = _findfirst(_dir, &thisfile);

  int exitmeplease = 0;

  while (fileindex != -1 && !exitmeplease)
  {
    if (strcmp(thisfile.name, ".") != 0 && strcmp(thisfile.name, "..") != 0 && (thisfile.attrib & _A_SUBDIR))
    {
      char* newname = strdup(thisfile.name);
      result->PutData(newname);
    }

    exitmeplease = _findnext(fileindex, &thisfile);
  }
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

#define FILE_PATH_BUFFER_SIZE 256
static char s_filePathBuffer[FILE_PATH_BUFFER_SIZE + 1];

const char* GetDirectoryPart(const char* _fullFilePath)
{
  strncpy(s_filePathBuffer, _fullFilePath, FILE_PATH_BUFFER_SIZE);

  char* finalSlash = strrchr(s_filePathBuffer, '/');
  if (finalSlash)
  {
    *(finalSlash + 1) = '\x0';
    return s_filePathBuffer;
  }

  return nullptr;
}

const char* GetFilenamePart(const char* _fullFilePath)
{
  const char* filePart = strrchr(_fullFilePath, '/') + 1;
  if (filePart)
  {
    strncpy(s_filePathBuffer, filePart, FILE_PATH_BUFFER_SIZE);
    return s_filePathBuffer;
  }
  return nullptr;
}

const char* GetExtensionPart(const char* _fullFilePath) { return strrchr(_fullFilePath, '.') + 1; }

const char* RemoveExtension(const char* _fullFileName)
{
  strcpy(s_filePathBuffer, _fullFileName);

  char* dot = strrchr(s_filePathBuffer, '.');
  if (dot)
    *dot = '\x0';

  return s_filePathBuffer;
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
