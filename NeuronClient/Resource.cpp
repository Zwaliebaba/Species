#include "pch.h"
#include "BinaryStreamReaders.h"
#include "Bitmap.h"
#include "Debug.h"
#include "FilesysUtils.h"
#include "FileWriter.h"
#include "Resource.h"
#include "Shape.h"
#include "TextRenderer.h"
#include "TextStreamReaders.h"
#include "Preferences.h"
#include "SoundStreamDecoder.h"
#include "Location.h"
#include "WorldPointers.h"

Resource* g_resource = nullptr;

Resource::Resource()
  : m_nameSeed(1),
    m_modName(nullptr) {}

Resource::~Resource()
{
  FlushOpenGlState();
  m_bitmaps.EmptyAndDelete();
  m_shapes.EmptyAndDelete();
}

void Resource::AddBitmap(const char* _name, const BitmapRGBA& _bmp, bool _mipMapping)
{
  // Only insert if a bitmap with no other bitmap is already using that name
  if (m_bitmaps.GetIndex(_name) == -1)
  {
    auto bmpCopy = new BitmapRGBA(_bmp);
    m_bitmaps.PutData(_name, bmpCopy);
  }
}

void Resource::DeleteBitmap(const char* _name)
{
  int index = m_bitmaps.GetIndex(_name);
  if (index >= 0)
  {
    BitmapRGBA* bmp = m_bitmaps.GetData(index);
    delete bmp;
    m_bitmaps.RemoveData(index);
  }
}

const BitmapRGBA* Resource::GetBitmap(const char* _name) { return m_bitmaps.GetData(_name); }

TextReader* Resource::GetTextReader(const std::string& _filename) { return GetTextReader(_filename.c_str()); }

TextReader* Resource::GetTextReader(const char* _filename)
{
  TextReader* reader = nullptr;
  if (DoesFileExist(_filename))
    reader = new TextFileReader(_filename);

  return reader;
}

BinaryReader* Resource::GetBinaryReader(const char* _filename)
{
  BinaryReader* reader = nullptr;
  if (DoesFileExist(_filename))
    reader = new BinaryFileReader(_filename);

  return reader;
}

int Resource::GetTexture(const char* _name, bool _mipMapping, bool _masked)
{
  // First lookup this name in the BTree of existing textures
  int theTexture = m_textures.GetData(_name, -1);

  // If the texture wasn't there, then look in our bitmap store
  if (theTexture == -1)
  {
    BitmapRGBA* bmp = m_bitmaps.GetData(_name);
    if (bmp)
    {
      if (_masked)
        bmp->ConvertPinkToTransparent();
      theTexture = bmp->ConvertToTexture(_mipMapping);
      m_textures.PutData(_name, theTexture);
    }
  }

  // If we still didn't find it, try to load it from a file on the disk
  if (theTexture == -1)
  {
    char fullPath[512];
    sprintf(fullPath, "%s", _name);
    strlwr(fullPath);
    BinaryReader* reader = GetBinaryReader(fullPath);

    if (reader)
    {
      const char* extension = GetExtensionPart(fullPath);
      BitmapRGBA bmp(reader, extension);
      delete reader;

      if (_masked)
        bmp.ConvertPinkToTransparent();
      theTexture = bmp.ConvertToTexture(_mipMapping);
      m_textures.PutData(_name, theTexture);
    }
  }

  if (theTexture == -1)
  {
    char errorString[512];
    sprintf(errorString, "Failed to load texture %s", _name);
    ASSERT_TEXT(false, errorString);
  }

  return theTexture;
}

bool Resource::DoesTextureExist(const char* _name)
{
  // First lookup this name in the BTree of existing textures
  int theTexture = m_textures.GetData(_name, -1);
  if (theTexture != -1)
    return true;

  // If the texture wasn't there, then look in our bitmap store
  BitmapRGBA* bmp = m_bitmaps.GetData(_name);
  if (bmp)
    return true;

  // If we still didn't find it, try to load it from a file on the disk
  char fullPath[512];
  sprintf(fullPath, "%s", _name);
  strlwr(fullPath);
  BinaryReader* reader = GetBinaryReader(fullPath);
  if (reader)
    return true;

  return false;
}

void Resource::DeleteTexture(const char* _name)
{
  int id = m_textures.GetData(_name);
  if (id > 0)
  {
    unsigned int id2 = id;
    glDeleteTextures(1, &id2);
    m_textures.RemoveData(_name);
  }
}

Shape* Resource::GetShape(const char* _name)
{
  Shape* theShape = m_shapes.GetData(_name);

  // If we haven't loaded the shape before, or _makeNew is true, then
  // try to load it from the disk
  if (!theShape)
  {
    theShape = GetShapeCopy(_name, false);
    m_shapes.PutData(_name, theShape);
  }

  return theShape;
}

Shape* Resource::GetShapeCopy(const char* _name, bool _animating)
{
  char fullPath[512];
  Shape* theShape = nullptr;

  sprintf(fullPath, "Shapes/%s", _name);
  strlwr(fullPath);
  if (DoesFileExist(fullPath))
    theShape = new Shape(fullPath, _animating);

  ASSERT_TEXT(theShape, "Couldn't create shape file %s", _name);
  return theShape;
}

SoundStreamDecoder* Resource::GetSoundStreamDecoder(const char* _filename)
{
  char buf[256];
  sprintf(buf, "%s.wav", _filename);
  BinaryReader* binReader = GetBinaryReader(buf);

  if (!binReader || !binReader->IsOpen())
    return nullptr;

  auto ssd = new SoundStreamDecoder(binReader);
  if (!ssd)
    return nullptr;

  return ssd;
}

int Resource::WildCmp(const char* wild, const char* string)
{
  const char* cp;
  const char* mp;

  while ((*string) && (*wild != '*'))
  {
    if ((*wild != *string) && (*wild != '?'))
      return 0;
    wild++;
    string++;
  }

  while (*string)
  {
    if (*wild == '*')
    {
      if (!*++wild)
        return 1;
      mp = wild;
      cp = string + 1;
    }
    else if ((*wild == *string) || (*wild == '?'))
    {
      wild++;
      string++;
    }
    else
    {
      wild = mp;
      string = cp++;
    }
  }

  while (*wild == '*') { wild++; }
  return !*wild;
}

int Resource::CreateDisplayList(const char* _name)
{
  // Make sure name isn't nullptr and isn't too long
  DEBUG_ASSERT(_name && strlen(_name) < 20);

  unsigned int id = glGenLists(1);
  m_displayLists.PutData(_name, id);

  return id;
}

int Resource::GetDisplayList(const char* _name)
{
  // Make sure name isn't nullptr and isn't too long
  DEBUG_ASSERT(_name && strlen(_name) < 20);

  return m_displayLists.GetData(_name, -1);
}

void Resource::DeleteDisplayList(const char* _name)
{
  if (!_name)
    return;

  // Make sure name isn't too long
  DEBUG_ASSERT(strlen(_name) < 20);

  int id = m_displayLists.GetData(_name, -1);
  if (id >= 0)
  {
    glDeleteLists(id, 1);
    m_displayLists.RemoveData(_name);
  }
}

void Resource::FlushOpenGlState()
{
#if 1 // Try to catch crash on shutdown bug
  // Tell OpenGL to delete the display lists
  for (int i = 0; i < m_displayLists.Size(); ++i)
  {
    if (m_displayLists.ValidIndex(i))
      glDeleteLists(m_displayLists[i], 1);
  }

  // Tell OpenGL to delete the textures
  for (int i = 0; i < m_textures.Size(); ++i)
  {
    if (m_textures.ValidIndex(i))
    {
      unsigned int id = i;
      glDeleteTextures(1, &id);
    }
  }
#endif

  // Forget all the display lists
  m_displayLists.Empty();

  // Forget all the texture handles
  m_textures.Empty();

  if (g_location)
    g_location->FlushOpenGlState();
}

void Resource::RegenerateOpenGlState()
{
  // Tell the text renderers to reload their font
  g_editorFont.BuildOpenGlState();
  g_gameFont.BuildOpenGlState();

  // Tell all the shapes to generate a new display list
  for (int i = 0; i < m_shapes.Size(); ++i)
  {
    if (!m_shapes.ValidIndex(i))
      continue;

    m_shapes[i]->BuildDisplayList();
  }

  // Tell the renderer (for the pixel effect texture)
  g_renderer->BuildOpenGlState();

  // Tell the location
  if (g_location)
    g_location->RegenerateOpenGlState();
}

char* Resource::GenerateName()
{
  int digits = log10f(m_nameSeed) + 1;
  auto name = new char [digits + 1];
  itoa(m_nameSeed, name, 10);
  m_nameSeed++;

  return name;
}

FileWriter* Resource::GetFileWriter(const char* _filename, bool _encrypt)
{
    return new FileWriter(_filename, _encrypt);
  }


// The string is copied and entered into llist
// in correct alphabetical order.
// The string wont be added if it is already present.
void OrderedInsert(LList<char*>* _llist, char* _newString)
{
  bool added = false;

  for (int i = 0; i < _llist->Size() - 1; ++i)
  {
    char* thisString = _llist->GetData(i);
    if (strcmp(_newString, thisString) == 0)
    {
      // String duplicate
      return;
    }
    if (strcmp(_newString, thisString) < 0)
    {
      _llist->PutDataAtIndex(strdup(_newString), i);
      added = true;
      break;
    }
  }

  if (!added)
    _llist->PutDataAtEnd(strdup(_newString));
}

// Finds all the filenames in the specified directory that match the specified
// filter. Directory should be like "textures" or "Textures/".
// Filter can be nullptr or "" or "*.bmp" or "map_*" or "map_*.txt"
// Set _longResults to true if you want results like "Textures/blah.bmp"
// or false for "blah.bmp"
LList<char*>* Resource::ListResources(const char* _dir, const char* _filter, bool _longResults /* = true */)
{
  LList<char*>* results = nullptr;

  //
  // List the base data directory

  results = ListDirectory(_dir, _filter, _longResults);

  return results;
}
