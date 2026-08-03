#include "pch.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

#include "Debug.h"
#include "Preferences.h"

PrefsManager *g_prefsManager = nullptr;

static bool s_overwrite = false;

PrefsManager::DefaultsProvider PrefsManager::sm_defaultsProvider = nullptr;

void PrefsManager::SetDefaultsProvider(DefaultsProvider _provider) { sm_defaultsProvider = _provider; }

// ***************
// Class PrefsItem
// ***************

PrefsItem::PrefsItem()
:   m_key(nullptr),
    m_str(nullptr),
    m_int(0)
{
}


PrefsItem::PrefsItem(char *_line)
:	m_str(nullptr)
{
	// Get key
	char *key = _line;
	while (!isalnum(*key) && *key != '\0')		// Skip leading whitespace
	{
		key++;
	}
	char *c = key;
	while (isalnum(*c))							// Find the end of the key word
	{
		c++;
	}
	*c = '\0';
	m_key = strdup(key);

	// Get value
	char *value = c + 1;
	while (isspace(*value) || *value == '=')
	{
		if (*value == '\0') break;
		value++;
	}

	// Is value a number?
	if (value[0] == '-' || isdigit(value[0]))
	{
		// Guess that number is an int
		m_type = TypeInt;

		// Verify that guess
		c = value;
        int numDots = 0;
		while (*c != '\0')
		{
			if (*c == '.')
			{
                ++numDots;
			}
			++c;
		}
        if( numDots == 1 ) m_type = TypeFloat;
        else if(  numDots > 1 ) m_type = TypeString;


		// Convert string into a real number
		if      (m_type == TypeFloat)	    m_float = atof(value);
        else if (m_type == TypeString)      m_str = strdup(value);
		else						        m_int = atoi(value);
	}
	else
	{
		m_type = TypeString;
		m_str = strdup(value);
	}
}


PrefsItem::PrefsItem(char const *_key, char const *_str)
:	m_type(TypeString)
{
	m_key = strdup(_key);
	m_str = strdup(_str);
}


// m_str is nulled even though neither of these is a string item: the destructor
// free()s it unconditionally, so leaving it indeterminate meant SetFloat or
// SetInt on a key the file did not already contain built an item that crashed
// when the manager was destroyed. That is the upgrade path for every new
// setting — old prefs file, new key — so it was reachable in normal use.
PrefsItem::PrefsItem(char const* _key, float _float)
  : m_type(TypeFloat),
    m_str(nullptr),
    m_float(_float)
{
	m_key = strdup(_key);
}


PrefsItem::PrefsItem(char const* _key, int _int)
  : m_type(TypeInt),
    m_str(nullptr),
    m_int(_int)
{
	m_key = strdup(_key);
}


PrefsItem::~PrefsItem()
{
	free(m_key);
	m_key = nullptr;
	free(m_str);
	m_str = nullptr;
}


// ******************
// Class PrefsManager
// ******************

PrefsManager::PrefsManager(char const *_filename)
{
    m_filename = strdup(_filename);

	Load();

    if( GetInt("ControlMethod") == -1 ) AddLine( "ControlMethod = 1" );

}

PrefsManager::PrefsManager(std::string const &_filename)
{
    m_filename = strdup(_filename.c_str());

	Load();

    if( GetInt("ControlMethod") == -1 ) AddLine( "ControlMethod = 1" );
}


PrefsManager::~PrefsManager()
{
	free(m_filename);
  m_items.clear();
  m_fileText.clear();
}


bool PrefsManager::IsLineEmpty(char const *_line)
{
	while (_line[0] != '\0')
	{
		if (_line[0] == '#') return true;
		if (isalnum(_line[0])) return false;
		++_line;
	}

	return true;
}

int GetDefaultHelpEnabled()
{
	return 1;
}

const char *GetDefaultSoundLibrary()
{
#ifdef HAVE_DSOUND
    return "dsound";
#else
	return "software";
#endif
}

int GetDefaultSoundDSP()
{
	return 1;
}

int GetDefaultSoundChannels()
{
	return 32;
}

int GetDefaultPixelShader()
{
	return 1;
}

int GetDefaultGraphicsDetail()
{
	return 1;
}

void PrefsManager::CreateDefaultValues()
{

    AddLine( "ServerAddress = 127.0.0.1" );
    AddLine( "IAmAServer = 1" );

    AddLine( "\n" );

    AddLine( "TextLanguage = unknown" );

    AddLine( "TextSpeed = 15" );

    AddLine( "\n" );

    AddLine(std::format("SoundLibrary = {}", GetDefaultSoundLibrary()).c_str());

    AddLine( "SoundMixFreq = 22050" );
    AddLine( "SoundMasterVolume = 255" );
    AddLine(std::format("SoundChannels = {}", GetDefaultSoundChannels()).c_str());
    AddLine( "SoundHW3D = 0" );
    AddLine( "SoundSwapStereo = 0" );
    AddLine( "SoundMemoryUsage = 1" );
    AddLine( "SoundBufferSize = 512" ); // Must be a power of 2 for Linux
    AddLine(std::format("SoundDSP = {}", GetDefaultSoundDSP()).c_str());

    AddLine( "\n" );

    AddLine( "ScreenWindowed = 1" );
    AddLine( "ScreenZDepth = 24" );

    AddLine( "\n" );

    AddLine(std::format("RenderLandscapeDetail = {}", GetDefaultGraphicsDetail()).c_str());
    AddLine(std::format("RenderWaterDetail = {}", GetDefaultGraphicsDetail()).c_str());
    AddLine(std::format("RenderBuildingDetail = {}", GetDefaultGraphicsDetail()).c_str());
    AddLine(std::format("RenderEntityDetail = {}", GetDefaultGraphicsDetail()).c_str());
    AddLine(std::format("RenderCloudDetail = {}", GetDefaultGraphicsDetail()).c_str());

    AddLine(std::format("RenderPixelShader = {}", GetDefaultPixelShader()).c_str());

    AddLine( "\n" );

    AddLine( "ControlMouseButtons = 3" );
    AddLine( "ControlMethod = 1" );

    AddLine( "BootLoader = firsttime" );
    AddLine( "UserProfile = NewUser" );
    AddLine( "RenderSpecialLighting = 0" );
	AddLine( OTHER_DIFFICULTY " = 1" );

  // Override the defaults above with stuff from a default preferences file.
  // The host installs the reader — see SetDefaultsProvider. This used to read
  // GameData/DefaultPreferences.txt through g_app->m_resource directly, which
  // is what kept a settings store dependent on the application object and the
  // renderer's resource loader.
  if (sm_defaultsProvider)
  {
    sm_defaultsProvider(*this);
  }
}


void PrefsManager::Load(char const *_filename)
{
	if (!_filename) _filename = m_filename;

  m_items.clear();

  // Try to read preferences if they exist
  FILE* in = fopen(FileSys::GetFullPathA(_filename).c_str(), "r");

  if (!in)
  {
    // Probably first time running the game
    CreateDefaultValues();
  }
    else
    {
	    char line[256];
	    while (fgets(line, 256, in) != nullptr)
	    {
            AddLine( line );
        }
    	fclose(in);
    }
}


void PrefsManager::SaveItem(FILE *out, PrefsItem *_item)
{
	switch (_item->m_type)
	{
		case PrefsItem::TypeFloat:
			fprintf(out, "%s = %.2f\n", _item->m_key, _item->m_float);
			break;
		case PrefsItem::TypeInt:
			fprintf(out, "%s = %d\n", _item->m_key, _item->m_int);
			break;
		case PrefsItem::TypeString:
			fprintf(out, "%s = %s\n", _item->m_key, _item->m_str);
			break;
	}
	_item->m_hasBeenWritten = true;
}


void PrefsManager::Save()
{
	// We've got a copy of the plain text from the prefs file that we initially
	// loaded in the variable m_fileText. We use that file as a template with
	// which to create the new save file. Updated prefs items values are looked up
	// as it their turn to be output comes around. When we've finished we then
	// write out all the new prefs items because they didn't exist in m_fileText.

	// First clear the "has been written" flags on all the items
  for (auto& entry : m_items)
  {
    entry.second->m_hasBeenWritten = false;
  }

  // Now use m_fileText as a template to write most of the items
  FILE* out = fopen(FileSys::GetFullPathA(m_filename).c_str(), "w");

  // If we couldn't open the prefs file for writing then just silently fail -
  // it's better than crashing.
  if (!out)
  {
    return;
  }

  for (const std::string& text : m_fileText)
  {
    char const* line = text.c_str();
    if (IsLineEmpty(line))
    {
      // "%s", not line: comments are user-written, and one containing a
      // percent sign made this read arguments that were never passed.
      fprintf(out, "%s", line);
    }
    else
    {
      char const* c = line;
      char const* keyStart = nullptr;
      char const* keyEnd = nullptr;
      while (*c != '=' && *c != '\0')
      {
        if (keyStart)
        {
          // First non-alphanumeric only. Tracking the last one meant
          // "Key    = 1" produced the key "Key   " — trailing spaces
          // included — which then failed to look up.
          if (!keyEnd && !isalnum(c[0]))
          {
            keyEnd = c;
          }
        }
        else
        {
          if (isalnum(c[0]))
          {
            keyStart = c;
          }
        }
        ++c;
      }

      // A line with no key at all ("= 5"). Nothing to look up, so it goes
      // back out as the user wrote it rather than being dropped.
      if (!keyStart)
      {
        fprintf(out, "%s", line);
        continue;
      }

      // A key running straight into '=' with no space between. keyEnd was
      // previously left indeterminate here, so keyLen was garbage.
      if (!keyEnd)
        keyEnd = c;

      // std::string, so no 128-byte buffer and no truncation clamp: a key
      // longer than the old limit now looks itself up instead of being cut
      // short and missing.
      const std::string key(keyStart, keyEnd - keyStart);

      // A template line naming a key the manager no longer holds is not a
      // reason to dereference slot -1. Echo the line instead of losing it.
      auto entry = m_items.find(key);
      if (entry == m_items.end())
      {
        fprintf(out, "%s", line);
        continue;
      }

      SaveItem(out, entry->second.get());
    }
  }

  // Finally output any items that haven't already been written. In key order,
  // where it used to be hash-slot order — see the note on m_items.
  for (auto& entry : m_items)
  {
    if (!entry.second->m_hasBeenWritten)
    {
      SaveItem(out, entry.second.get());
    }
  }

  fclose(out);
}


void PrefsManager::Clear()
{
  m_items.clear();
  m_fileText.clear();
}


float PrefsManager::GetFloat(char const *_key, float _default) const
{
  auto entry = m_items.find(_key);
  if (entry == m_items.end())
    return _default;
  PrefsItem* item = entry->second.get();
  if (item->m_type != PrefsItem::TypeFloat)
    return _default;
  return item->m_float;
}


int PrefsManager::GetInt(char const *_key, int _default) const
{
  auto entry = m_items.find(_key);
  if (entry == m_items.end())
    return _default;
  PrefsItem* item = entry->second.get();
  if (item->m_type != PrefsItem::TypeInt)
    return _default;
  return item->m_int;
}


char const *PrefsManager::GetString(char const *_key, char const *_default) const
{
  auto entry = m_items.find(_key);
  if (entry == m_items.end())
    return _default;
  PrefsItem* item = entry->second.get();
  if (item->m_type != PrefsItem::TypeString)
    return _default;
  return item->m_str;
}


//void PrefsManager::GetData(char const *_key, void *_data, int _length) const
//{
//    unsigned char *data = (unsigned char *)_data;
//    char *stringData = GetString(_key);
//
//    if( stringData )
//    {
//        DEBUG_ASSERT (_length * 2 == strlen(stringData));
//
//        for( int i = 0; i < _length; ++i )
//        {
//            data[i] = (stringData[i*2] - 'A') << 4;
//            data[i] |= (stringData[i*2+1] - 'A');
//        }
//    }
//}


void PrefsManager::SetString(char const *_key, char const *_string)
{
  auto entry = m_items.find(_key);

  if (entry == m_items.end())
  {
    auto item = std::make_unique<PrefsItem>(_key, _string);
    m_items[item->m_key] = std::move(item);
  }
  else
  {
    PrefsItem* item = entry->second.get();
    DEBUG_ASSERT(item->m_type == PrefsItem::TypeString);
    char* newString = strdup(_string);
    free(item->m_str);
    // Note by Chris:
    // The incoming value of _string might also be item->m_str
    // So it is essential to copy _string before freeing item->m_str
    item->m_str = newString;
  }
}


void PrefsManager::SetFloat(char const *_key, float _float)
{
  auto entry = m_items.find(_key);

  if (entry == m_items.end())
  {
    auto item = std::make_unique<PrefsItem>(_key, _float);
    m_items[item->m_key] = std::move(item);
  }
  else
  {
    PrefsItem* item = entry->second.get();
    DEBUG_ASSERT(item->m_type == PrefsItem::TypeFloat);
    item->m_float = _float;
  }
}


void PrefsManager::SetInt(char const *_key, int _int)
{
  auto entry = m_items.find(_key);

  if (entry == m_items.end())
  {
    auto item = std::make_unique<PrefsItem>(_key, _int);
    m_items[item->m_key] = std::move(item);
  }
  else
  {
    PrefsItem* item = entry->second.get();
    DEBUG_ASSERT(item->m_type == PrefsItem::TypeInt);
    item->m_int = _int;
  }
}


void PrefsManager::AddLine(char const*_line, bool _overwrite)
{
	if ( !_line )
		return;

	bool saveLine = true;

	if (!IsLineEmpty(_line))				// Skip comment lines and blank lines
	{
		char *localCopy = strdup( _line );
		char *c = strchr(localCopy, '\n');
		if (c)
			*c = '\0';

    auto item = std::make_unique<PrefsItem>(localCopy);

    auto existing = m_items.find(item->m_key);
    if (_overwrite && existing != m_items.end())
    {
      m_items.erase(existing);
      saveLine = false;
    }

    // The hand-rolled table this replaced asserted in GetInsertPos that a
    // key was never inserted twice, so a duplicate was a precondition
    // violation rather than supported behaviour: debug tripped the
    // assert, release grew a second slot for the same key. Replacing is
    // the honest reading of that contract; move-assigning over the node
    // frees whatever it displaces, which is what closes the leak the
    // assert used to stand in for.
    const std::string key = item->m_key;
    m_items[key] = std::move(item);

    free(localCopy);
  }

  if (saveLine)
  {
    // std::string, so no strdup. The legacy array's EmptyAndDelete used
    // `delete` on these malloc'd pointers, which was undefined behaviour
    // every time the manager was destroyed.
    m_fileText.emplace_back(_line);
  }
}


//void PrefsManager::AddData(char const *_key, void *_data, int _length)
//{
//    char *newString = new char[_length*2 + 1];
//    unsigned char *data = (unsigned char *)_data;
//	int i;
//    for( i = 0; i < _length; ++i )
//    {
//        newString[i*2]      = 'A' + ((data[i] & 0xf0) >> 4);
//        newString[i*2+1]    = 'A' + (data[i] & 0xf);
//    }
//
//    newString[i*2] = '\0';
//
//    PrefsItem *item = new PrefsItem();
//    item->m_key = strdup(_key);
//    item->m_str = newString;
//    m_items.PutData(item->m_key, item);
//}


bool PrefsManager::DoesKeyExist(char const* _key) { return m_items.contains(_key); }
