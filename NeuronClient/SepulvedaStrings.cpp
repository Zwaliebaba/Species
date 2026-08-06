#include "pch.h"

#include "StringUtils.h"

// Caption template parser for the Sepulveda help/tutorial strings.
//
// Language strings may contain markers that are expanded at lookup time:
//
//   [key controlname]  - the name of the input currently bound to a control
//   [phrase_key]       - the (recursively expanded) contents of another phrase
//
// Ported from the original Darwinia sepulveda_strings.cpp. It also had
// [ifmode keyboard], [else] and [endif]; input-native-events T14 removed them
// with the input mode they asked about, having first measured that no file
// under GameData used any of the three.

#include "Input.h"
#include "LanguageTable.h"


#ifndef SEPULVEDA_MAX_PHRASE_LENGTH
#define SEPULVEDA_MAX_PHRASE_LENGTH 1024
#endif
#define MAX_READ_KEY_LENGTH 64
#define MAX_FINAL_KEY_LENGTH 96


namespace Neuron
{
  struct CaptionParserMode
  {
      // THE CONDITIONAL STATE IS GONE WITH [IFMODE]. `writing`, `ifdepth` and
      // `writingStopDepth` existed for one directive whose only question was
      // the input mode; see consumeIfMarker's removal below. Nothing can
      // suppress output now, so the parser always writes.
      int inOffset, outOffset;
      CaptionParserMode()
        : inOffset(0),
          outOffset(0)
      {
      }
  };

  bool buildCaption(char const* _baseString, char* _dest, CaptionParserMode& _mode);
  bool buildPhrase(char const* _key, char* _dest, CaptionParserMode& _mode);
  bool consumeMarker(char const* _baseString, char* _dest, CaptionParserMode& _mode);
  bool consumeKeyMarker(char const* _baseString, char* _dest, CaptionParserMode& _mode);
  bool consumeOtherMarker(char const* _baseString, char* _dest, CaptionParserMode& _mode);


  bool buildCaption(char const* _baseString, char* _dest, CaptionParserMode& _mode)
  {
    char ch;
    while ((ch = _baseString[_mode.inOffset]) != '\0')
    {
      if (ch == '[')
      {
        if (!consumeMarker(_baseString, _dest, _mode))
        {
          return false;
        }
      }
      else
      {
        if (_mode.outOffset < SEPULVEDA_MAX_PHRASE_LENGTH - 1)
          _dest[_mode.outOffset++] = ch;
        else
        {
          _dest[_mode.outOffset] = '\0';
          return false;
        }
      }
      ++_mode.inOffset;
    }
    _dest[_mode.outOffset] = '\0';
    return true;
  }


  bool buildPhrase(char const* _key, char* _dest, CaptionParserMode& _mode)
  {
    bool done = false;

    std::string key(_key);
    if (key.size() > MAX_FINAL_KEY_LENGTH - 1)
      key.resize(MAX_FINAL_KEY_LENGTH - 1);
    StrToLower(key.data());

    // The MOODY* macros took an input mode and are gone with it; these are the
    // same two lookups without the mode argument.
    if (RAWISLANGUAGEPHRASE(key.c_str()))
    {
      const char* definition = RAWLANGUAGEPHRASE(key.c_str());
      if (definition)
      {
        CaptionParserMode mode;
        mode.outOffset = _mode.outOffset;
        done = buildCaption(definition, _dest, mode);
        if (done)
          _mode.outOffset = mode.outOffset;
      }
    }

    return done;
  }


  // Return 0, or the length of the marker at the beginning of _in,
  // including the brackets.
  int markerLength(char const* _in)
  {
    int len = 0;
    if (_in[0] == '[')
    {
      while (_in[++len] != ']')
      {
        if (_in[len] == '\0')
        {
          len = -1;
          break;
        }
      }
      ++len;
    }
    return len;
  }


  bool consumeMarker(char const* _baseString, char* _dest, CaptionParserMode& _mode)
  {
    if (consumeKeyMarker(_baseString, _dest, _mode) || consumeOtherMarker(_baseString, _dest, _mode)) // Other must be last. It always succeeds.
    {
      return true;
    }
    // _dest is the caller's buffer and stays one: every function in this file
    // writes into it at _mode.outOffset, so it cannot become a std::string
    // without changing all of them together.
    const std::string_view source(_baseString);
    size_t length = source.size();
    if (length > SEPULVEDA_MAX_PHRASE_LENGTH - 1)
    {
      length = SEPULVEDA_MAX_PHRASE_LENGTH - 1;
    }
    std::memcpy(_dest, source.data(), length);
    _dest[length] = '\0';
    return false;
  }


  // consumeIfMarker IS GONE, and with it [IFMODE ...], [ELSE] and [ENDIF].
  //
  // [IFMODE was the conditional's only opener and the input mode was its only
  // question — the code could ask about exactly one value, KEYBOARD — so with
  // INPUT_MODE_GAMEPAD deleted the directive became a tautology and [ELSE] a
  // branch that could never be taken. Keeping ELSE and ENDIF without an opener
  // would be worse than either.
  //
  // MEASURED BEFORE REMOVING: `[IFMODE`, `[ELSE]` and `[ENDIF]` appear ZERO
  // times across everything under GameData, so no shipped caption used any of
  // it. The other three markers — [KEY...], the phrase lookup and the plain
  // text run — are untouched.


  bool consumeKeyMarker(char const* _baseString, char* _dest, CaptionParserMode& _mode)
  {
    char const* in = _baseString + _mode.inOffset;
    bool done = false;
    if (strnicmp(in, "[KEY", 4) == 0)
    {
      int len = markerLength(in) - 5;
      if (0 < len && len < MAX_READ_KEY_LENGTH)
      {
        const std::string buf(in + 4, len);

        std::optional<ControlType> const eventId = g_inputManager->getControlID(buf);
        if (eventId.has_value())
        {
          // Lookup the key name for the binding
          InputDescription desc;
          if (g_inputManager->getBoundInputDescription(*eventId, desc))
          {
            char const* keyName = desc.noun.c_str();

            // Is the caption long enough
            const size_t keyNameLength = strlen(keyName);
            if (_mode.outOffset + keyNameLength < SEPULVEDA_MAX_PHRASE_LENGTH - 1)
            {
              // Write the key name into the caption
              std::memcpy(_dest + _mode.outOffset, keyName, keyNameLength + 1);
              _mode.outOffset += static_cast<int>(keyNameLength);
              done = true;
            }
          }
        }
      }
      if (done)
        _mode.inOffset += len + 4;
    }

    return done;
  }


  bool consumeOtherMarker(char const* _baseString, char* _dest, CaptionParserMode& _mode)
  {
    // Look in hash table
    const char* in = _baseString + _mode.inOffset;
    bool done = false;

    int len = markerLength(in) - 2;
    if (0 < len && len < MAX_READ_KEY_LENGTH)
    {
      std::string key;
      CaptionParserMode mode(_mode);

      if (in[1] == ' ')
      {
        if (mode.outOffset < SEPULVEDA_MAX_PHRASE_LENGTH - 1)
          _dest[mode.outOffset++] = ' ';
        key.assign(in + 2, len - 1);
      }
      else
      {
        key.assign(in + 1, len);
      }

      done = buildPhrase(key.c_str(), _dest, mode);
      if (done)
        _mode = mode;
    }

    _mode.inOffset += len + 1; // Always eat the input

    return true; // Always succeed
  }


  bool buildCaption(char const* _baseString, char* _dest)
  {
    CaptionParserMode mode;
    return buildCaption(_baseString, _dest, mode);
  }


} // namespace Neuron
