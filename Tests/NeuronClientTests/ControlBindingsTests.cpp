#include "pch.h"

#include <optional>
#include <string>

#include "ControlBindings.h"
#include "ControlTypes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronClientTests
{
  // ControlBindings' name lookup, characterised by language-hygiene T11 when
  // getControlID stopped returning -1 and started returning an optional.
  //
  // ALL OF THIS IS PRE-EXISTING BEHAVIOUR. The point of writing it down is that
  // the lookup walks a table generated from ControlTypes.inc, matches
  // case-insensitively, and has one edge that reads like a bug and is not one
  // — see EmptyNameResolvesToControlNull. None of that was covered before, and
  // a conversion that quietly changed any of it would have broken the control
  // preferences file format without failing a build.
  TEST_CLASS(ControlBindingsTests)
  {
    public:
      TEST_METHOD(FindsTheFirstGeneratedControlByName)
      {
        // "MenuLeft" is DEF_CONTROL_TYPE's first line, so this also pins that
        // the table and the enum start at the same place.
        std::optional<ControlType> const id = ControlBindings::getControlID("MenuLeft");
        Assert::IsTrue(id.has_value());
        Assert::IsTrue(ControlType::ControlMenuLeft == *id);
      }

      TEST_METHOD(FindsAControlFromTheEndOfTheTable)
      {
        // The last DEF_CONTROL_TYPE line. Together with the test above, a
        // table that had drifted by one row would fail here.
        std::optional<ControlType> const id = ControlBindings::getControlID("ControllerPlugged");
        Assert::IsTrue(id.has_value());
        Assert::IsTrue(ControlType::ControlControllerPlugged == *id);
      }

      TEST_METHOD(NameMatchingIsCaseInsensitive)
      {
        // stricmp, not strcmp. The shipped preferences files are not
        // consistently cased and this is what lets them load.
        std::optional<ControlType> const lower = ControlBindings::getControlID("cameraleft");
        std::optional<ControlType> const upper = ControlBindings::getControlID("CAMERALEFT");
        Assert::IsTrue(lower.has_value());
        Assert::IsTrue(upper.has_value());
        Assert::IsTrue(ControlType::ControlCameraLeft == *lower);
        Assert::IsTrue(ControlType::ControlCameraLeft == *upper);
      }

      TEST_METHOD(UnknownNameYieldsNothing)
      {
        // This is the -1 that became std::nullopt. The two callers both tested
        // `>= 0` and now test has_value(); nothing ever used the -1 itself.
        Assert::IsFalse(ControlBindings::getControlID("NoSuchControlName").has_value());
      }

      TEST_METHOD(EmptyNameResolvesToControlNull)
      {
        // NEGATIVE CONTROL, and the one result here that surprises. s_actions
        // holds an empty-name row at ControlNull's index, before the null
        // terminator, so the empty string MATCHES rather than failing — the
        // lookup returns a real, bindable control type for it.
        //
        // It is pre-existing and harmless (ControlNull is documented as never
        // triggered), and language-hygiene T11 deliberately did not change it:
        // the task was scoping the enum, and "" is not a name any preferences
        // file writes. Recorded so that a later reader does not mistake the
        // row for the terminator and "fix" the bound.
        std::optional<ControlType> const id = ControlBindings::getControlID("");
        Assert::IsTrue(id.has_value());
        Assert::IsTrue(ControlType::ControlNull == *id);
      }

      TEST_METHOD(RoundTripsThroughGetControlString)
      {
        // getControlString is getControlID's inverse and reads the same table
        // from the other end.
        std::string name;
        ControlBindings::getControlString(ControlType::ControlCameraForwards, name);
        Assert::AreEqual(std::string("CameraForwards"), name);

        std::optional<ControlType> const back = ControlBindings::getControlID(name);
        Assert::IsTrue(back.has_value());
        Assert::IsTrue(ControlType::ControlCameraForwards == *back);
      }

      TEST_METHOD(TheRangeCoversEveryBindableControlType)
      {
        // printNumBindings and ControlBindings::Clear both walk
        // RangeControlType now instead of an int loop. It must cover exactly
        // the array bound, or Clear stops clearing the last binding.
        size_t count = 0;
        for (ControlType const type : RangeControlType())
        {
          (void)type;
          ++count;
        }
        Assert::AreEqual(Neuron::I(ControlType::NumControlTypes), count);
      }
  };
} // namespace NeuronClientTests
