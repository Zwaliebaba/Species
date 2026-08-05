#include "pch.h"

#include <string>
#include <string_view>

#include "BinaryStreamReaders.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// BinaryReader::m_filename was a char[256] that an strncpy bounded to 255 wrote
// into — so a name of exactly 255 characters or more was never terminated — and
// GetFileType returned a char* INTO that buffer, pointing at the terminator when
// there was no extension. strings-modernised T9 made the member a std::string
// and the return a std::string_view, and these pin what the extension parse
// answers, including the two edges the pointer form expressed by accident.
//
// BinaryDataReader is the constructible half of the pair: it reads from memory
// the caller owns and touches no global, where BinaryFileReader opens a file.
// The parse under test lives on the shared member and is identical in both.
namespace NeuronClientTests
{
  TEST_CLASS(BinaryReaderTests)
  {
    public:
      TEST_METHOD(TheFileTypeIsWhateverFollowsTheLastDot)
      {
        const unsigned char data[] = {'x'};
        Neuron::BinaryDataReader reader(data, sizeof data, "laser.wav");

        Assert::AreEqual(std::string("wav"), std::string(reader.GetFileType()));
      }

      TEST_METHOD(OnlyTheLastDotCounts)
      {
        const unsigned char data[] = {'x'};
        Neuron::BinaryDataReader reader(data, sizeof data, "Sounds/music.loop.wav");

        Assert::AreEqual(std::string("wav"), std::string(reader.GetFileType()));
      }

      TEST_METHOD(ANameWithNoDotHasAnEmptyFileType)
      {
        const unsigned char data[] = {'x'};
        Neuron::BinaryDataReader reader(data, sizeof data, "laser");

        // The char* form returned &m_filename[strlen(m_filename)] here — a
        // pointer to the terminator, which is to say the empty string. Empty is
        // the same answer without the pointer arithmetic, and NOT nullptr: the
        // only caller hands this straight to a case-insensitive compare.
        Assert::IsTrue(reader.GetFileType().empty());
      }

      TEST_METHOD(ATrailingDotHasAnEmptyFileType)
      {
        const unsigned char data[] = {'x'};
        Neuron::BinaryDataReader reader(data, sizeof data, "laser.");

        Assert::IsTrue(reader.GetFileType().empty());
      }

      TEST_METHOD(AFilenameLongerThanTheOldBufferSurvivesWhole)
      {
        const std::string longName = std::string(400, 'a') + ".wav";
        const unsigned char data[] = {'x'};
        Neuron::BinaryDataReader reader(data, sizeof data, longName.c_str());

        // 400 characters is past the char[256] this used to be copied into, and
        // past the 255 the strncpy was bounded to — which wrote no terminator at
        // all at that length.
        Assert::AreEqual(longName, reader.m_filename);
        Assert::AreEqual(std::string("wav"), std::string(reader.GetFileType()));
      }
  };
} // namespace NeuronClientTests
