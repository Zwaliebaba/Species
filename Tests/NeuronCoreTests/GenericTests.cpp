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

      TEST_METHOD(ConvertIntToIpReversesConvertIpToInt) { Assert::AreEqual(std::string("192.168.0.1"), ConvertIntToIP(ConvertIPToInt("192.168.0.1"))); }

      // The top octet is the one that could regress: 0xff000000 does not fit in
      // an int, so the mask is unsigned and the shift is logical. An arithmetic
      // shift here would sign-extend and print something other than 255.
      TEST_METHOD(ConvertIntToIpFormatsTheTopOctetOfANegativePackedAddress)
      {
        Assert::AreEqual(std::string("255.255.255.255"), ConvertIntToIP(-1));
      }

      TEST_METHOD(ConvertIntToIpReturnsAValueNotAStaticBuffer)
      {
        // The old implementation returned a pointer into a function-local
        // static char[16], so these two would have been the same address with
        // the same contents. Holding one result across a second call is now
        // safe, which is the point of the signature change.
        const std::string first = ConvertIntToIP(ConvertIPToInt("1.2.3.4"));
        const std::string second = ConvertIntToIP(ConvertIPToInt("5.6.7.8"));
        Assert::AreEqual(std::string("1.2.3.4"), first);
        Assert::AreEqual(std::string("5.6.7.8"), second);
      }
  };
} // namespace NeuronCoreTests
