#include "pch.h"
#include "BinaryStreamReaders.h"
#include "Bitmap.h"
#include "Debug.h"
#include "FilesysUtils.h"
#include "FileWriter.h"
#include "Resource.h"
#include "Shape.h"
#include "StringUtils.h"
#include "TextRenderer.h"
#include "TextStreamReaders.h"
#include "Preferences.h"
#include "SoundStreamDecoder.h"
#include "WorldPointers.h"

Resource* g_resource = nullptr;

namespace
{
  // HashTable::GetData took the value to return for a missing key. std::map has
  // no such overload, and several call sites here lean on the miss value rather
  // than testing membership — -1 for a texture or display list, nullptr for a
  // bitmap or shape — so the shape of those calls is worth keeping.
  template <typename Map> typename Map::mapped_type Lookup(const Map& _map, const char* _key, typename Map::mapped_type _default)
  {
    const auto entry = _map.find(_key);
    return entry == _map.end() ? _default : entry->second;
  }

  // The same shape for the two maps that own their values. It cannot be Lookup:
  // that returns mapped_type by value, and a unique_ptr will not copy. The miss
  // value is always nullptr here, so it is not a parameter.
  template <typename Map> auto LookupOwned(const Map& _map, const char* _key) -> typename Map::mapped_type::pointer
  {
    const auto entry = _map.find(_key);
    return entry == _map.end() ? nullptr : entry->second.get();
  }
} // namespace

Resource::Resource()
  : m_nameSeed(1),
    m_modName(nullptr)
{
}

Resource::~Resource()
{
  FlushOpenGlState();

  // Both maps own their values; clear() is kept so they still empty here,
  // after FlushOpenGlState, rather than at the member destructors.
  m_bitmaps.clear();
  m_shapes.clear();
}

void Resource::AddBitmap(const char* _name, const BitmapRGBA& _bmp, bool _mipMapping)
{
  // Only insert if a bitmap with no other bitmap is already using that name
  if (!m_bitmaps.contains(_name))
  {
    m_bitmaps[_name] = std::make_unique<BitmapRGBA>(_bmp);
  }
}

void Resource::DeleteBitmap(const char* _name)
{
  const auto entry = m_bitmaps.find(_name);
  if (entry != m_bitmaps.end())
  {
    m_bitmaps.erase(entry);
  }
}

const BitmapRGBA* Resource::GetBitmap(const char* _name) { return LookupOwned(m_bitmaps, _name); }

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
  // First lookup this name in the table of existing textures
  int theTexture = Lookup(m_textures, _name, -1);

  // If the texture wasn't there, then look in our bitmap store
  if (theTexture == -1)
  {
    BitmapRGBA* bmp = LookupOwned(m_bitmaps, _name);
    if (bmp)
    {
      if (_masked)
        bmp->ConvertPinkToTransparent();
      theTexture = bmp->ConvertToTexture(_mipMapping);
      m_textures[_name] = theTexture;
    }
  }

  // If we still didn't find it, try to load it from a file on the disk
  if (theTexture == -1)
  {
    std::string fullPath(_name);
    StrToLower(fullPath.data());
    BinaryReader* reader = GetBinaryReader(fullPath.c_str());

    if (reader)
    {
      const std::string extension = GetExtensionPart(fullPath.c_str());
      BitmapRGBA bmp(reader, extension.c_str());
      delete reader;

      if (_masked)
        bmp.ConvertPinkToTransparent();
      theTexture = bmp.ConvertToTexture(_mipMapping);
      m_textures[_name] = theTexture;
    }
  }

  if (theTexture == -1)
  {
    ASSERT_TEXT(false, "Failed to load texture %s", _name);
  }

  return theTexture;
}

bool Resource::DoesTextureExist(const char* _name)
{
  // First lookup this name in the table of existing textures
  int theTexture = Lookup(m_textures, _name, -1);
  if (theTexture != -1)
    return true;

  // If the texture wasn't there, then look in our bitmap store
  BitmapRGBA* bmp = LookupOwned(m_bitmaps, _name);
  if (bmp)
    return true;

  // If we still didn't find it, try to load it from a file on the disk
  std::string fullPath(_name);
  StrToLower(fullPath.data());
  BinaryReader* reader = GetBinaryReader(fullPath.c_str());
  if (reader)
    return true;

  return false;
}

void Resource::DeleteTexture(const char* _name)
{
  const auto entry = m_textures.find(_name);
  if (entry != m_textures.end() && entry->second > 0)
  {
    unsigned int id = entry->second;
    glDeleteTextures(1, &id);
    m_textures.erase(entry);
  }
}

Shape* Resource::GetShape(const char* _name)
{
  Shape* theShape = LookupOwned(m_shapes, _name);

  // If we haven't loaded the shape before, or _makeNew is true, then
  // try to load it from the disk
  if (!theShape)
  {
    // GetShapeCopy hands back a shape it does not keep; the map takes it and
    // theShape stays as the observer this function returns.
    std::unique_ptr<Shape> owned(GetShapeCopy(_name, false));
    theShape = owned.get();
    m_shapes[_name] = std::move(owned);
  }

  return theShape;
}

Shape* Resource::GetShapeCopy(const char* _name, bool _animating)
{
  Shape* theShape = nullptr;

  std::string fullPath = std::format("Shapes/{}", _name);
  StrToLower(fullPath.data());
  if (DoesFileExist(fullPath.c_str()))
    theShape = new Shape(fullPath.c_str(), _animating);

  ASSERT_TEXT(theShape, "Couldn't create shape file %s", _name);
  return theShape;
}

SoundStreamDecoder* Resource::GetSoundStreamDecoder(const char* _filename)
{
  const std::string wavName = std::format("{}.wav", _filename);
  BinaryReader* binReader = GetBinaryReader(wavName.c_str());

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

  while (*wild == '*')
  {
    wild++;
  }
  return !*wild;
}

int Resource::CreateDisplayList(const char* _name)
{
  // Make sure name isn't nullptr and isn't too long
  DEBUG_ASSERT(_name && strlen(_name) < 20);

  unsigned int id = glGenLists(1);
  m_displayLists[_name] = id;

  return id;
}

int Resource::GetDisplayList(const char* _name)
{
  // Make sure name isn't nullptr and isn't too long
  DEBUG_ASSERT(_name && strlen(_name) < 20);

  return Lookup(m_displayLists, _name, -1);
}

void Resource::DeleteDisplayList(const char* _name)
{
  if (!_name)
    return;

  // Make sure name isn't too long
  DEBUG_ASSERT(strlen(_name) < 20);

  const auto entry = m_displayLists.find(_name);
  if (entry != m_displayLists.end() && entry->second >= 0)
  {
    glDeleteLists(entry->second, 1);
    m_displayLists.erase(entry);
  }
}

void Resource::FlushOpenGlState()
{
#if 1 // Try to catch crash on shutdown bug
  // Tell OpenGL to delete the display lists
  for (const auto& [name, displayList] : m_displayLists)
    glDeleteLists(displayList, 1);

  // Tell OpenGL to delete the textures. This used to pass the loop counter —
  // the hash table's slot index — to glDeleteTextures rather than the texture
  // name stored in that slot, so it deleted whichever textures happened to have
  // low numeric names and left the real ones alive. There is no index to get
  // wrong now. The "crash on shutdown bug" the #if above is hunting is a
  // plausible consequence, but that is a guess, not a diagnosis.
  for (const auto& [name, texture] : m_textures)
  {
    unsigned int id = texture;
    glDeleteTextures(1, &id);
  }
#endif

  // Forget all the display lists
  m_displayLists.clear();

  // Forget all the texture handles
  m_textures.clear();

  if (g_locationAccess)
    g_locationAccess->FlushOpenGlState();
}

void Resource::RegenerateOpenGlState()
{
  // Tell the text renderers to reload their font
  g_editorFont.BuildOpenGlState();
  g_gameFont.BuildOpenGlState();

  // Tell all the shapes to generate a new display list
  for (const auto& [name, shape] : m_shapes)
    shape->BuildDisplayList();

  // Tell the renderer (for the pixel effect texture)
  g_renderer->BuildOpenGlState();

  // Tell the location
  if (g_locationAccess)
    g_locationAccess->RegenerateOpenGlState();
}

char* Resource::GenerateName()
{
  int digits = log10f(m_nameSeed) + 1;
  auto name = new char[digits + 1];
  itoa(m_nameSeed, name, 10);
  m_nameSeed++;

  return name;
}

FileWriter* Resource::GetFileWriter(const char* _filename, bool _encrypt) { return new FileWriter(_filename, _encrypt); }


// Finds all the filenames in the specified directory that match the specified
// filter. Directory should be like "textures" or "Textures/".
// Filter can be nullptr or "" or "*.bmp" or "map_*" or "map_*.txt"
// Set _longResults to true if you want results like "Textures/blah.bmp"
// or false for "blah.bmp"
std::vector<char*>* Resource::ListResources(const char* _dir, const char* _filter, bool _longResults /* = true */)
{
  std::vector<char*>* results = nullptr;

  //
  // List the base data directory

  results = ListDirectory(_dir, _filter, _longResults);

  return results;
}
