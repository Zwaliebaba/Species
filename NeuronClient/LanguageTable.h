#pragma once

#include <memory>

#include <map>
#include <string.h> // stricmp, for the comparator below
#include <string>
#include <strstream>
#include <vector>


namespace Neuron
{
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
      static constexpr size_t NOT_FOUND_CAPACITY = 256;
      LangPhrase m_notFound;

      void SetNotFoundText(std::string_view _text);
      // Owning: the destructor deletes every phrase. Ordered rather than hashed,
      // and the order is observable — see GetPhraseList and the note in
      // tasks/containers-replaced.yaml T24.
      std::map<std::string, std::unique_ptr<LangPhrase>, LangKeyLess> m_phrasesRaw;

      // ONE TABLE, WHERE THERE WERE TWO. m_phrasesXin was the gamepad's, built
      // from the `_xin` half of the language files, and both went with
      // INPUT_MODE_GAMEPAD. The `_kbd` mechanism this keeps is the live one:
      // a key spelled `help_camera_movement_kbd` answers a lookup for
      // `help_camera_movement`, which is how a phrase that names keys is kept
      // separate from one that does not.
      std::unique_ptr<PhraseOffsets> m_phrases;
      // ostrstream::str() hands back a frozen buffer the caller must delete[],
      // which is what the array deleter does.
      std::unique_ptr<char[]> m_chunk;

      bool specific_key_exists(const char* _key);
      PhraseOffsets* GetCurrentTable();

      void RebuildTable(PhraseOffsets* _phrases, std::ostrstream& stream);

    public:
      LangTable(char const* _filename);
      ~LangTable();

      void ParseLanguageFile(char const* _filename);
      void RebuildTables();

      bool DoesPhraseExist(char const* _key);
      char* LookupPhrase(char const* _key);

      // Public since the mode-taking overloads went: the caption builder asks
      // these two directly, and used to reach the MOODY* pair instead.
      bool RawDoesPhraseExist(char const* _key);
      char* RawLookupPhrase(char const* _key);

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
#define RAWISLANGUAGEPHRASE(x) g_langTable->RawDoesPhraseExist(x)

  // Owned by App, which assigns this during startup. Declared here so the layers
  // below Species can reach the subsystem without including App.h — see
  // tasks/layering-inversion.yaml T8.
  extern LangTable* g_langTable;
} // namespace Neuron
