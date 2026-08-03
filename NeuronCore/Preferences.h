#pragma once

#include <map>
#include <string>
#include <vector>


// ***************
// Class PrefsItem
// ***************

class PrefsItem
{
public:
	char		*m_key;

	enum
	{
		TypeString,
		TypeFloat,
		TypeInt
	};

	int			m_type;
	char		*m_str;
	union {
		int		m_int;
		float	m_float;
	};

	bool		m_hasBeenWritten;

    PrefsItem();
	PrefsItem(char *_line);
	PrefsItem(char const *_key, char const *_str);
	PrefsItem(char const *_key, float _float);
	PrefsItem(char const *_key, int _int);
	~PrefsItem();
};


// ******************
// Class PrefsManager
// ******************

class PrefsManager
{
  public:
    // Lets the host overlay its own shipped defaults, called from
    // CreateDefaultValues. See Preferences.cpp for why this is a seam.
    using DefaultsProvider = void (*)(PrefsManager& _prefs);
    static void SetDefaultsProvider(DefaultsProvider _provider);

  private:
    static DefaultsProvider sm_defaultsProvider;

    // Ordered, not hashed. The hand-rolled table's slot order decided where keys
    // absent from the loaded file landed in the saved one, and that order was a
    // function of the key text and the table's growth history — unreproducible
    // by any standard container, and pinned by nothing. Sorting by key makes
    // that block stable across runs and machines instead; template lines keep
    // their own positions either way, which is what
    // Tests/NeuronCoreTests/PreferencesTests.cpp actually pins.
    std::map<std::string, PrefsItem*> m_items;
    std::vector<std::string> m_fileText;
    char *m_filename;

	bool IsLineEmpty(char const *_line);
	void SaveItem(FILE *out, PrefsItem *_item);
  void DeleteItems();

  void CreateDefaultValues();

public:
	PrefsManager(char const *_filename);
	PrefsManager(std::string const &_filename);
	~PrefsManager();

	void Load		(char const *_filename=nullptr); // If filename is nullptr, then m_filename is used
	void Save		();

	void Clear		();

	char const *GetString (char const *_key, char const *_default=nullptr) const;
	float GetFloat  (char const *_key, float _default=-1.0f) const;
	int	  GetInt    (char const *_key, int _default=-1) const;

	// Set functions update existing PrefsItems or create new ones if key doesn't yet exist
	void SetString  (char const *_key, char const *_string);
	void SetFloat	(char const *_key, float _val);
	void SetInt		(char const *_key, int _val);

    void AddLine    (char const*_line, bool _overwrite = false);

//    void GetData    (char const *_key, void *_data, int _length) const;
//    void AddData    (char const *_key, void *_data, int _length);

    bool DoesKeyExist(char const *_key);
};


extern PrefsManager *g_prefsManager;


// Preference key names. They were declared in GameLogic/PrefsOtherWindow.h,
// beside the window that edits them, which meant naming a setting required a UI
// header. Every file that used them already includes this one.

#define OTHER_HELPENABLED "HelpEnabled"
#define OTHER_CONTROLHELPENABLED "ControlHelpEnabled"
#define OTHER_BOOTLOADER "BootLoader"
#define OTHER_CHRISTMASENABLED "ChristmasEnabled"
#define OTHER_LANGUAGE "TextLanguage"
#define OTHER_DIFFICULTY "Difficulty"
#define OTHER_LARGEMENUS "LargeMenus"
#define OTHER_AUTOMATICCAM "AutomaticCamera"


