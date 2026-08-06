#include "pch.h"

#include <strstream>
#include <iostream>
#include <fstream>

#include "Debug.h"
#include "LanguageTable.h"
#include "Resource.h"
#include "TextRenderer.h"
#include "TextStreamReaders.h"


#include "Input.h"


namespace Neuron
{
  LangTable* g_langTable = nullptr;

#define DEBUG_PRINT_LANGTABLE 0

// ****************************************************************************
// Class LangPhrase
// ****************************************************************************

LangPhrase::LangPhrase()
  : m_key(nullptr),
    m_string(nullptr)
{
}


LangPhrase::~LangPhrase()
{
  free(m_key);
  free(m_string);
}


// ****************************************************************************
// Class LangTable
// ****************************************************************************

LangTable::LangTable(char const* _filename)
{
  // Create the "not found" Phrase that will be returned when
  // the LookupPhrase is called with an unknown key
  m_notFound.m_key = nullptr;
  m_notFound.m_string = (char*)malloc(NOT_FOUND_CAPACITY);

  // The owning members start empty on their own.

  // Open the required language file
  ParseLanguageFile(_filename);
}


LangTable::~LangTable()
{
  // All four members own what they hold, so the destructor has nothing left to
  // do. The commented-out Empty()-then-delete block that used to sit here went
  // with them: it described the legacy HashTable, not std::map.
}


void LangTable::ParseLanguageFile(char const* _filename)
{
  TextReader* in = g_resource->GetTextReader(_filename);
  ASSERT_TEXT(in && in->IsOpen(), "Couldn't open language file {}", _filename);

  // Read all the phrases from the language file
  while (in->ReadLine())
  {
    if (!in->TokenAvailable())
      continue;

    char* key = in->GetNextToken();

    auto phrase = std::make_unique<LangPhrase>();
    phrase->m_key = strdup(key);

    // Make sure the this key isn't already used. BTree::RemoveData dropped the
    // node without deleting the phrase it held, so a language file that
    // repeated a key leaked one LangPhrase per repeat; erasing here deletes it.
    if (const auto existing = m_phrasesRaw.find(key); existing != m_phrasesRaw.end())
    {
      m_phrasesRaw.erase(existing);
    }

    char* aString = in->GetRestOfLine();

    // Make sure a language key always has a some text with it
    DEBUG_ASSERT(aString && aString[0] != '\0');

    for (unsigned int i = 0; i < strlen(aString) - 1; ++i)
    {
      if (aString[i] == '\\' && aString[i + 1] == 'n')
      {
        aString[i] = ' ';
        aString[i + 1] = '\n';
      }
    }

    phrase->m_string = strdup(aString);

    int stringLength = strlen(phrase->m_string);
    if (phrase->m_string[stringLength - 1] == '\n')
    {
      phrase->m_string[stringLength - 1] = '\x0';
    }

    if (phrase->m_string[stringLength - 2] == '\r')
    {
      phrase->m_string[stringLength - 2] = '\x0';
    }

    m_phrasesRaw[phrase->m_key] = std::move(phrase);
  }

  delete in;
}


bool is_suffix(const char* str, const char* suf)
{
  int suf_len = strlen(suf);
  int off = strlen(str) - suf_len;
  if (off >= 0)
  {
    return strncmp(str + off, suf, suf_len) == 0;
  }
  else
    return false;
}


// wrong_suffix IS GONE. It asked "does this key belong to a mode that is not
// the current one", which had exactly one possible yes -- a `_xin` key, the
// gamepad's -- and there is no gamepad mode and no `_xin` key left in any
// language file. The RebuildTable branch it selected, the one that emplaced an
// empty string so a foreign-mode key rendered as nothing, went with it.


// The not-found phrase is a malloc'd buffer the lookups write into and then
// hand out as a char*, so it cannot become a std::string without changing what
// LookupPhrase returns — ownership/T3's job. Bounded here, which the old
// formatted writes into it were not.
void LangTable::SetNotFoundText(std::string_view _text)
{
  size_t length = _text.size();
  if (length > NOT_FOUND_CAPACITY - 1)
  {
    length = NOT_FOUND_CAPACITY - 1;
  }
  std::memcpy(m_notFound.m_string, _text.data(), length);
  m_notFound.m_string[length] = '\0';
}


// `_kbd` only, now that `_xin` is not a suffix this file knows about.
bool chomp_mode_suffix(char* key)
{
  if (is_suffix(key, "_kbd"))
  {
    key[strlen(key) - 4] = '\0';
    return true;
  }
  return false;
}


bool LangTable::specific_key_exists(const char* _key)
{
  if (_key)
  {
    // This was a char[128] filled by a 123-byte bounded copy, after which the
    // suffix was written at key + strlen(_key) — the UNtruncated length. A key
    // of 130 characters therefore wrote its suffix past the end of the buffer,
    // and the truncation that was supposed to prevent it made the overflow
    // worse rather than better by leaving len pointing outside.
    std::string key(_key);
    key += "_kbd";
    return RawDoesPhraseExist(key.c_str());
  }
  else
    return false;
}


bool buildCaption(char const* _baseString, char* _dest); // see sepulveda_strings.cpp


// Only used for debugging
bool printTable(PhraseOffsets const* keys, const char* table, std::ostream& out)
{
  if (keys && table && out.good())
  {
    for (const auto& [key, offset] : *keys)
    {
      out << key << '\t' << (table + offset) << std::endl;
    }
    return true;
  }
  return false;
}


void LangTable::RebuildTables()
{
  std::ostrstream stream;


  // Assigning frees the previous table at the same point the deletes did.
  m_phrases = std::make_unique<PhraseOffsets>();

  RebuildTable(m_phrases.get(), stream);
  m_chunk.reset(stream.str());

  if (DEBUG_PRINT_LANGTABLE)
  {
    std::ofstream dbg_out("langtable_debug.txt", std::ios::app);
    dbg_out << "********** ALL STRINGS **********" << std::endl;
    if (!printTable(m_phrases.get(), m_chunk.get(), dbg_out))
    {
      dbg_out << "STRINGS NOT AVAILABLE." << std::endl;
    }
    dbg_out << std::endl << "********** FINISHED ALL STRINGS **********" << std::endl << std::endl << std::endl;
  }
}

void LangTable::RebuildTable(PhraseOffsets* _phrases, std::ostrstream& stream)
{
  char theString[1024];

  stream << '\x0'; // This is our empty string

  // try_emplace rather than operator[]: HashTable::PutData kept the FIRST value
  // written for a key and a lookup never reached a later duplicate. The
  // suffix-chomping below can produce the same key twice, and the guards that
  // are supposed to prevent it are the thing most likely to have a hole in it.
  for (const auto& [rawKey, phrase] : m_phrasesRaw)
  {
    if (strncmp(phrase->m_key, "part_", 5) != 0)
    {
      // chomp_mode_suffix writes into this, so it is a mutable copy on
      // purpose. The char[1024] it used to be was one unbounded copy away from
      // an overflow on any key longer than that.
      std::string key(phrase->m_key);

      // Make sure this is the most specific string, or ignore it
      if (chomp_mode_suffix(key.data()) || !specific_key_exists(key.c_str()))
      {
        int currPos = stream.tellp();
        buildCaption(phrase->m_string, theString);
        stream << theString << '\x0';

        _phrases->try_emplace(key, currPos);
      }
    }
  }
}


// It used to pick a table by the current input mode, and to answer nullptr
// before the input manager existed -- which meant every phrase lookup made
// during startup silently returned the empty string.
PhraseOffsets* LangTable::GetCurrentTable()
{
  if (!m_phrases)
    RebuildTables();

  return m_phrases.get();
}


bool LangTable::DoesPhraseExist(char const* _key)
{
  PhraseOffsets* phrases = GetCurrentTable();
  if (!_key || !phrases)
  {
    return false;
  }
  else
  {
    return phrases->contains(_key);
  }
}


// The mode-taking overload is gone; what it added was a second lookup under
// the current mode's suffix, and one of the two suffixes is all that is left.
bool LangTable::RawDoesPhraseExist(char const* _key)
{
  if (!_key)
    return false;

  if (m_phrasesRaw.contains(_key))
    return true;

  return m_phrasesRaw.contains(std::string(_key) + "_kbd");
}


char* LangTable::LookupPhrase(char const* _key)
{
  PhraseOffsets* phrases = GetCurrentTable();
  char* phrase = nullptr;

  if (!_key || !phrases || !m_chunk)
  {
    SetNotFoundText("ERROR (null)");
    phrase = m_notFound.m_string;
  }
  else
  {
    // A miss reads offset 0, which is the empty string RebuildTable writes
    // first, NOT the not-found phrase. HashTable::GetData returned a
    // value-initialised int for an absent key and the `offset >= 0` test below
    // never rejected it, so an unknown key has always rendered as "". Keeping
    // that: the menus are full of keys that only exist in some languages.
    const auto entry = phrases->find(_key);
    phrase = m_chunk.get() + (entry == phrases->end() ? 0 : entry->second);
  }

  return phrase;
}


char* LangTable::RawLookupPhrase(char const* _key)
{
  LangPhrase* phrase = nullptr;

  if (!_key)
  {
    SetNotFoundText("ERROR (null)");
    phrase = &m_notFound;
  }
  else
  {
    // The suffixed key first, the plain one second: that ordering is what makes
    // `_kbd` mean `the more specific spelling of this phrase`.
    {
      const auto suffixed = m_phrasesRaw.find(std::string(_key) + "_kbd");
      if (suffixed != m_phrasesRaw.end())
        phrase = suffixed->second.get();
    }

    if (!phrase)
    {
      const auto plain = m_phrasesRaw.find(_key);
      if (plain != m_phrasesRaw.end())
        phrase = plain->second.get();
    }

    if (!phrase)
    {
      // Historically this reported ERROR (<key>); it reports nothing now, which
      // is why a missing phrase renders as empty rather than as a visible
      // marker. Left as found — changing it is a UI decision, not a string one.
      *(m_notFound.m_string) = '\0';
      phrase = &m_notFound;
    }
  }

  return phrase->m_string;
}


void LangTable::TestAgainstEnglish()
{
  //
  // Open the errors file

  FILE* output = fopen(FileSys::GetFullPathA("language_errors.txt").c_str(), "wt");


  //
  // Open the English language file as a base

  LangTable* english = new LangTable("Language/English.txt");


  //
  // Look for strings in English that are not in this language

  for (const auto& [key, phrase] : english->m_phrasesRaw)
  {
    if (!DoesPhraseExist(phrase->m_key))
    {
      fprintf(output, "ERROR : Failed to find translation for string ID '%s'\n", phrase->m_key);
    }
    else
    {
      char const* translatedPhrase = LookupPhrase(phrase->m_key);
      if (strcmp(phrase->m_string, translatedPhrase) == 0)
      {
        fprintf(output, "ERROR : String ID appears not to be translated : '%s'\n", phrase->m_key);
      }
    }
  }


  //
  // Look for phrases in this language that are not in English

  for (const auto& [key, phrase] : m_phrasesRaw)
  {
    if (!english->DoesPhraseExist(phrase->m_key))
    {
      fprintf(output, "ERROR : Found new string ID not present in original English : '%s'\n", phrase->m_key);
    }
  }


  //
  // Clean up

  delete english;
  fclose(output);
}


std::vector<LangPhrase*>* LangTable::GetPhraseList()
{
  auto phrases = new std::vector<LangPhrase*>();
  phrases->reserve(m_phrasesRaw.size());
  for (const auto& [key, phrase] : m_phrasesRaw)
    phrases->push_back(phrase.get());
  return phrases;
}


// Goes through the string and divides it up into several
// smaller strings, taking into account newline characters,
// and the width of the text area.
std::vector<char*>* WordWrapText(const char* _string, float _lineWidth, float _fontWidth, bool _wrapToWindow)
{
  if (!_string)
    return nullptr;
  if (_lineWidth < 0 && _wrapToWindow)
    return nullptr;

  // Calculate the maximum width in characters for 1 line
  int linewidth = int(_lineWidth / _fontWidth);


  // Create a copy of the string which we will use as our output strings
  // (All connected together but seperated by 0)
  // And add a newline on at the end (to make sure a newline will be found)

  // The trailing newline guarantees the scan below finds a final line break.
  // Still `new char[]` because the pointers handed out below point into it and
  // the caller frees it; ownership/T3 is what changes that.
  const std::string terminated = std::format("{}\n", _string);
  char* newstring = new char[terminated.size() + 1];
  std::memcpy(newstring, terminated.c_str(), terminated.size() + 1);

  // Build a linked list of pointers into this new string
  // Each pointer representing another line

  auto lines = new std::vector<char*>();

  char* currentpos = newstring;
  lines->push_back(currentpos);

  while (true)
  {
    char* nextnewline = strchr(currentpos, '\n');
    if (!nextnewline)
      break;

    if (_wrapToWindow && nextnewline - currentpos > linewidth)
    {
      // This line is too long and needs trimming down
      // Ignore the newline char we found - it will be dealt with
      // as part of the next line
      // Place a terminater in the space between the last 2 words
      currentpos += linewidth;
      char oldchar = *(currentpos - 1);
      *(currentpos - 1) = 0;
      char* space = strrchr(currentpos - linewidth, ' ');

      if (space)
      {
        *(currentpos - 1) = oldchar;
        currentpos = space + 1;
        *space = 0;
        lines->push_back(currentpos);
      }
      else
      {
        // We cannot wrap this line - the word is longer than the max line width
      }
    }
    else
    {
      // Found a newline char - replace with a terminator
      // then add this position in as the next line
      // then continue from this position
      currentpos = nextnewline + 1;
      *nextnewline = 0;
      lines->push_back(currentpos);
    }
  }

  //
  // The very last line is always empty.
  // Remove it.
  lines->pop_back();

  return lines;
}
} // namespace Neuron
