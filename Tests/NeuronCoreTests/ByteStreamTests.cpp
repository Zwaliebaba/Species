#include "pch.h"

#include "ByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  // ByteReader and ByteWriter are the wire format. They move native
  // representations in and out of a packet buffer and advance a cursor by
  // exactly the size of what they moved, so both the value and the byte layout
  // are part of the contract between two machines — including two machines of
  // different architectures, which nobody has yet verified agree (see
  // AGENTS.md).
  //
  // What is new here is the bound. The macros these replaced could not fail:
  // they cast the cursor and dereferenced it, so a short datagram was read past
  // its end and the caller never learned. The tests below pin both halves —
  // that the bytes are unchanged, and that running out of them is now an
  // answerable question rather than undefined behaviour.
  TEST_CLASS(ByteStreamTests)
  {
    public:
      TEST_METHOD(ScalarsReadBackInWriteOrder)
      {
        char buffer[32] = {};

        ByteWriter writer(buffer, sizeof(buffer));
        writer.Write<int>(-12345);
        writer.Write<float>(0.5f);
        writer.Write<unsigned char>(200);
        writer.Write<unsigned short>(40000);
        writer.Write<signed char>(-42);

        ByteReader reader(buffer, writer.BytesWritten());
        const int asInt = reader.Read<int>();
        const float asFloat = reader.Read<float>();
        const unsigned char asUnsignedChar = reader.Read<unsigned char>();
        const unsigned short asUnsignedShort = reader.Read<unsigned short>();
        const signed char asSignedChar = reader.Read<signed char>();

        Assert::AreEqual(-12345, asInt);
        Assert::AreEqual(0.5f, asFloat);
        Assert::AreEqual(static_cast<unsigned char>(200), asUnsignedChar);
        Assert::AreEqual(static_cast<unsigned short>(40000), asUnsignedShort);
        Assert::AreEqual(static_cast<signed char>(-42), asSignedChar);
        Assert::IsTrue(reader.Ok());
      }

      TEST_METHOD(BothCursorsAdvanceByTheSizeOfWhatTheyMoved)
      {
        char buffer[32] = {};

        ByteWriter writer(buffer, sizeof(buffer));
        writer.Write<int>(1);
        writer.Write<float>(1.0f);
        writer.Write<unsigned char>(1);
        writer.Write<unsigned short>(1);

        ByteReader reader(buffer, sizeof(buffer));
        [[maybe_unused]] const int asInt = reader.Read<int>();
        [[maybe_unused]] const float asFloat = reader.Read<float>();
        [[maybe_unused]] const unsigned char asUnsignedChar = reader.Read<unsigned char>();
        [[maybe_unused]] const unsigned short asUnsignedShort = reader.Read<unsigned short>();

        Assert::AreEqual(static_cast<size_t>(11), writer.BytesWritten());
        Assert::AreEqual(writer.BytesWritten(), reader.BytesRead());
      }

      TEST_METHOD(IntegersAreWrittenLittleEndian)
      {
        // Pinned explicitly. A round-trip test passes on either byte order,
        // because the same machine does both halves of it.
        char buffer[8] = {};

        ByteWriter writer(buffer, sizeof(buffer));
        writer.Write<int>(0x01020304);

        Assert::AreEqual(static_cast<unsigned char>(0x04), static_cast<unsigned char>(buffer[0]));
        Assert::AreEqual(static_cast<unsigned char>(0x03), static_cast<unsigned char>(buffer[1]));
        Assert::AreEqual(static_cast<unsigned char>(0x02), static_cast<unsigned char>(buffer[2]));
        Assert::AreEqual(static_cast<unsigned char>(0x01), static_cast<unsigned char>(buffer[3]));
      }

      TEST_METHOD(AReadThatDoesNotFitYieldsZeroAndLeavesTheCursorAlone)
      {
        // The whole point of the class. Three bytes cannot answer a
        // four-byte read, and what came back before was whatever the fourth
        // byte of the receive buffer happened to hold.
        char buffer[3] = {1, 2, 3};

        ByteReader reader(buffer, sizeof(buffer));
        const int value = reader.Read<int>();

        Assert::AreEqual(0, value);
        Assert::IsFalse(reader.Ok());
        Assert::AreEqual(static_cast<size_t>(0), reader.BytesRead());
      }

      TEST_METHOD(OverrunIsStickyEvenIfALaterReadWouldFit)
      {
        // A parser reads its fields and asks once at the end whether any of it
        // was real, so a single short field has to poison the whole parse — not
        // just its own read. Here the second read fits and must still leave the
        // reader not-Ok.
        char buffer[8] = {};

        ByteReader reader(buffer, sizeof(buffer));
        [[maybe_unused]] const int first = reader.Read<int>();
        [[maybe_unused]] const int second = reader.Read<int>();
        [[maybe_unused]] const int third = reader.Read<int>(); // past the end

        Assert::IsFalse(reader.Ok());

        ByteReader rewound(buffer, sizeof(buffer));
        [[maybe_unused]] const unsigned char oneByte = rewound.Read<unsigned char>();
        Assert::IsTrue(rewound.Ok(), L"a reader that has not overrun stays Ok");
      }

      TEST_METHOD(AZeroLengthOrNullBufferReadsNothing)
      {
        // Both are reachable: a zero-byte datagram is a legal thing to receive,
        // and a length parameter arriving as zero or negative is what a caller
        // that has not checked its own read looks like.
        ByteReader empty(nullptr, 64);
        [[maybe_unused]] const int fromNull = empty.Read<int>();
        Assert::IsFalse(empty.Ok());

        char buffer[8] = {};
        ByteReader zeroLength(buffer, 0);
        [[maybe_unused]] const int fromZero = zeroLength.Read<int>();
        Assert::IsFalse(zeroLength.Ok());
      }

      TEST_METHOD(AWriteThatDoesNotFitIsDroppedRatherThanOverrunning)
      {
        // A guard byte after the buffer, checked afterwards: the failure this
        // prevents is a write off the end, and the only way to test for one is
        // to look at what is on the other side of the bound.
        struct
        {
            char buffer[6];
            char guard;
        } storage = {};
        storage.guard = 0x7E;

        ByteWriter writer(storage.buffer, sizeof(storage.buffer));
        writer.Write<int>(0x01020304);
        Assert::IsTrue(writer.Ok());

        writer.Write<int>(0x05060708); // only two bytes left
        Assert::IsFalse(writer.Ok());
        Assert::AreEqual(static_cast<size_t>(4), writer.BytesWritten());
        Assert::AreEqual(static_cast<char>(0x7E), storage.guard);
      }

      TEST_METHOD(WriteBytesRefusesABlockThatDoesNotFit)
      {
        // The letter path copies each update's serialised bytes in as a block,
        // so the block form needs the same bound as the scalar one.
        char source[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        char buffer[6] = {};

        ByteWriter writer(buffer, sizeof(buffer));
        writer.WriteBytes(source, sizeof(source));

        Assert::IsFalse(writer.Ok());
        Assert::AreEqual(static_cast<size_t>(0), writer.BytesWritten());
        Assert::AreEqual(static_cast<char>(0), buffer[0]);
      }
  };
} // namespace NeuronCoreTests
