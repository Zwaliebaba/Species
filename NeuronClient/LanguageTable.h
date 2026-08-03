#pragma once

#include "InputTypes.h"

#include <map>
#include <string.h> // stricmp, for the comparator below
#include <string>
#include <strstream>
#include <vector>


class LangPhrase
{
  public:
    char* m_key;    // malloced. The name used as an ID
    char* m_string; // malloced. This bit is different for each language

    LangPhrase();
    ~LangPhrase();
};


// Phrase keys are matched without regard to case, and always have been: BTree
// ordered and compared with stricmp, and HashTable masked the case bit off in
// its hash. A language file that spells a key "Dialog_Ok" answers a lookup for
// "dialog_ok" today, so the ordering has to keep ignoring case.
//
// Third copy of this comparator in the tree — Resource.h and Profiler.h have
// their own. A fourth should be the point where it moves into NeuronCore.
struct LangKeyLess
{
    bool operator()(const std::string& _a, const std::string& _b) const { return stricmp(_a.c_str(), _b.c_str()) < 0; }
};

// Key -> offset into m_chunk. Was a HashTable<int>.
typedef std::map<std::string, int, LangKeyLess> PhraseOffsets;


class LangTable
{
  private:
    LangPhrase m_notFound;
    // Owning: the destructor deletes every phrase. Ordered rather than hashed,
    // and the order is observable — see GetPhraseList and the note in
    // tasks/containers-replaced.yaml T24.
    std::map<std::string, LangPhrase*, LangKeyLess> m_phrasesRaw;
    PhraseOffsets* m_phrasesKbd;
    PhraseOffsets* m_phrasesXin;
    char* m_chunk;

    bool specific_key_exists(const char* _key, InputMode _mood);
    bool RawDoesPhraseExist(char const* _key);
    PhraseOffsets* GetCurrentTable();
    PhraseOffsets* GetCurrentTable(InputMode _mood);

    void RebuildTable(PhraseOffsets* _phrases, std::ostrstream& stream, InputMode _mood);

  public:
    LangTable(char const* _filename);
    ~LangTable();

    void ParseLanguageFile(char const* _filename);
    void RebuildTables();

    bool DoesPhraseExist(char const* _key);
    char* LookupPhrase(char const* _key);

    char* RawLookupPhrase(char const* _key);

    bool RawDoesPhraseExist(char const* _key, InputMode _mood);
    char* RawLookupPhrase(char const* _key, InputMode _mood);

    // Caller owns the vector but NOT the phrases in it. No caller in this
    // repository — see tasks/containers-replaced.yaml T24.
    std::vector<LangPhrase*>* GetPhraseList();

    void TestAgainstEnglish();
};


// Returns a vector the caller owns, holding pointers INTO A SINGLE BUFFER that
// element 0 points at the start of. The elements are not separate allocations:
// freeing them individually corrupts the heap, and freeing only element 0 —
// with delete[], it is a `new char[]` — releases all of them. Nothing but
// element 0 may outlive that delete.
std::vector<char*>* WordWrapText(const char* _string, float _linewidth, float _fontWidth, bool _wrapToWindow = true);

#define LANGUAGEPHRASE(x) g_langTable->LookupPhrase(x)
#define ISLANGUAGEPHRASE(x) g_langTable->DoesPhraseExist(x)
#define ISLANGUAGEPHRASE_ANY(x) g_langTable->DoesPhraseExist(x)

#define RAWLANGUAGEPHRASE(x) g_langTable->RawLookupPhrase(x)
#define MOODYLANGUAGEPHRASE(x, y) g_langTable->RawLookupPhrase((x), (y))
#define MOODYISLANGUAGEPHRASE(x, y) g_langTable->RawDoesPhraseExist((x), (y))

// Owned by App, which assigns this during startup. Declared here so the layers
// below Species can reach the subsystem without including App.h — see
// tasks/layering-inversion.yaml T8.
extern LangTable* g_langTable;
