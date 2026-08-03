#pragma once

#include <string>
#include <vector>

#include <stdio.h>


//*****************************************************************************
// Misc directory and filename functions
//*****************************************************************************

// Both return a vector the caller owns, holding strings the caller owns. The
// two do NOT allocate their strings the same way and never have: ListDirectory
// uses `new char[]`, so its results need delete[], while ListSubDirectoryNames
// uses strdup, so its results need free(). Callers today disagree about which
// (see tasks/containers-replaced.yaml T6) — read this line before writing a
// clean-up loop.
std::vector<char*>* ListDirectory(char const* _dir, char const* _filter, bool fullFilename = true);
std::vector<char*>* ListSubDirectoryNames(char const* _dir);

bool DoesFileExist(char const *_fullPath);

// By value: these used to return a pointer into one shared static, so two
// results could not be held at once. An absent directory or extension is the
// empty string, never null. See tasks/strings-modernised.yaml T16.
std::string GetDirectoryPart(char const* _fullFilePath);
std::string GetFilenamePart(char const* _fullFilePath);
std::string GetExtensionPart(char const* _fullFilePath);
std::string RemoveExtension(char const* _fullFileName);

bool AreFilesIdentical          (char const *_name1, char const *_name2);

bool CreateDirectory            (char const *_directory);
void DeleteThisFile             (char const *_filename);


class EncryptedFileWriter
{
protected:
	int m_offsetIndex;
	FILE *m_out;

public:
	EncryptedFileWriter(char const *_name);
	~EncryptedFileWriter();

	void WriteLine(char *_line);	// _line gets modified
};

