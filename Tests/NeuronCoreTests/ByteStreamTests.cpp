#include "pch.h"

#include "ByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  // The ByteStream macros are the wire format. They write native representations
  // straight into a packet buffer and advance the cursor by exactly the size of
  // what they wrote, so both the value and the byte layout are part of the
  // contract between two machines — including two machines of different
  // architectures, which nobody has yet verified agree (see AGENTS.md).
  TEST_CLASS(ByteStreamTests)
  {
    public:
      TEST_METHOD(ScalarsReadBackInWriteOrder)
      {
        char buffer[32] = {};

        char* write = buffer;
        WRITE_INT(write, -12345);
        WRITE_FLOAT(write, 0.5f);
        WRITE_UNSIGNED_CHAR(write, 200);
        WRITE_UNSIGNED_SHORT(write, 40000);
        WRITE_SIGNED_CHAR(write, -42);

        char* read = buffer;
        const int asInt = READ_INT(read);
        const float asFloat = READ_FLOAT(read);
        const unsigned char asUnsignedChar = READ_UNSIGNED_CHAR(read);
        const unsigned short asUnsignedShort = READ_UNSIGNED_SHORT(read);
        const signed char asSignedChar = READ_SIGNED_CHAR(read);

        Assert::AreEqual(-12345, asInt);
        Assert::AreEqual(0.5f, asFloat);
        Assert::AreEqual(static_cast<unsigned char>(200), asUnsignedChar);
        Assert::AreEqual(static_cast<unsigned short>(40000), asUnsignedShort);
        Assert::AreEqual(static_cast<signed char>(-42), asSignedChar);
      }

      TEST_METHOD(BothCursorsAdvanceByTheSizeOfWhatTheyMoved)
      {
        char buffer[32] = {};

        char* write = buffer;
        WRITE_INT(write, 1);
        WRITE_FLOAT(write, 1.0f);
        WRITE_UNSIGNED_CHAR(write, 1);
        WRITE_UNSIGNED_SHORT(write, 1);

        char* read = buffer;
        [[maybe_unused]] const int asInt = READ_INT(read);
        [[maybe_unused]] const float asFloat = READ_FLOAT(read);
        [[maybe_unused]] const unsigned char asUnsignedChar = READ_UNSIGNED_CHAR(read);
        [[maybe_unused]] const unsigned short asUnsignedShort = READ_UNSIGNED_SHORT(read);

        Assert::AreEqual(static_cast<size_t>(11), static_cast<size_t>(write - buffer));
        Assert::AreEqual(write - buffer, read - buffer);
      }

      TEST_METHOD(IntegersAreWrittenLittleEndian)
      {
        // Pinned explicitly. A round-trip test passes on either byte order,
        // because the same machine does both halves of it.
        char buffer[8] = {};

        char* write = buffer;
        WRITE_INT(write, 0x01020304);

        Assert::AreEqual(static_cast<unsigned char>(0x04), static_cast<unsigned char>(buffer[0]));
        Assert::AreEqual(static_cast<unsigned char>(0x03), static_cast<unsigned char>(buffer[1]));
        Assert::AreEqual(static_cast<unsigned char>(0x02), static_cast<unsigned char>(buffer[2]));
        Assert::AreEqual(static_cast<unsigned char>(0x01), static_cast<unsigned char>(buffer[3]));
      }
  };
} // namespace NeuronCoreTests
