#include "pch.h"

#include "Generic.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  // ConvertIPToInt packs a dotted quad with the *first* octet in the low byte,
  // matching the layout Winsock's in_addr already uses. The packed values are
  // pinned rather than only round-tripped: a reversed byte order round-trips
  // perfectly and still sends every client to the wrong address.
  TEST_CLASS(GenericTests)
  {
    public:
      TEST_METHOD(ConvertIpToIntPutsTheFirstOctetInTheLowByte) { Assert::AreEqual(0x04030201, ConvertIPToInt("1.2.3.4")); }

      TEST_METHOD(ConvertIpToIntHandlesLoopback) { Assert::AreEqual(0x0100007F, ConvertIPToInt("127.0.0.1")); }

      TEST_METHOD(ConvertIpToIntHandlesTheAllOnesBroadcast)
      {
        // Every octet 255 fills the sign bit; the result is deliberately an int,
        // so this is -1 rather than a value that fails to compare equal.
        Assert::AreEqual(-1, ConvertIPToInt("255.255.255.255"));
      }

      TEST_METHOD(ConvertIntToIpReversesConvertIpToInt) { Assert::AreEqual("192.168.0.1", ConvertIntToIP(ConvertIPToInt("192.168.0.1"))); }
  };
} // namespace NeuronCoreTests
