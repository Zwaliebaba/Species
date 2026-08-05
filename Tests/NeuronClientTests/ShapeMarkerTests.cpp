#include "pch.h"

#include <string>

#include <string.h>

#include "Shape.h"
#include "TextStreamReaders.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace
{
  // A marker block exactly as the shipped .shp files spell one — note the
  // LOWERCASE sceneroot, which is what all 605 ParentName lines under
  // GameData/Shapes use. The code compares it against the literal "SceneRoot".
  char const* const MARKER_BLOCK = "\tParentName: sceneroot\n"
                                   "\tDepth: 1\n"
                                   "\tUp:     0.00  1.00  0.00\n"
                                   "\tFront:  0.00  0.00 -1.00\n"
                                   "\tPos:   95.06  0.00 -25.39\n"
                                   "\tMarkerEnd\n";

  // Depth is deliberately present in every block below. ShapeMarker's
  // TextReader constructor does not initialise m_depth before the parse loop
  // and then calls m_parents.assign(m_depth, nullptr), so a block without a
  // Depth line sizes a vector from an indeterminate value. That is a
  // pre-existing defect rather than anything this test is pinning, and it is
  // recorded in tasks/strings-modernised.yaml T20.
  ShapeMarker ParseMarker(char const* _block, char const* _name)
  {
    TextDataReader in(_block, static_cast<unsigned int>(strlen(_block)), "test.shp");
    return ShapeMarker(&in, _name);
  }
} // namespace

namespace NeuronClientTests
{
  TEST_CLASS(ShapeMarkerTests)
  {
    public:
      TEST_METHOD(ParsesNameAndParentName)
      {
        ShapeMarker const marker = ParseMarker(MARKER_BLOCK, "MarkerSpike01");

        Assert::AreEqual(std::string("MarkerSpike01"), marker.m_name);
        Assert::AreEqual(std::string("sceneroot"), marker.m_parentName);
        Assert::AreEqual(1, marker.m_depth);
      }

      // The char* pair this member replaced started as nullptr and was tested
      // for null at the end of the constructor. Empty stands in for that,
      // because TextReader::GetNextToken returns nullptr for an empty token and
      // never an empty string.
      TEST_METHOD(AMissingParentNameBecomesUnknown)
      {
        ShapeMarker const marker = ParseMarker("\tDepth: 1\n\tMarkerEnd\n", "MarkerNoParent");

        Assert::AreEqual(std::string("MarkerNoParent"), marker.m_name);
        Assert::AreEqual(std::string("unknown"), marker.m_parentName);
      }

      // This one pins NEW behaviour rather than preserved behaviour, and the
      // distinction is worth stating. The only caller of this constructor
      // (Shape::Load) passes GetNextToken()'s result, which is either nullptr
      // or a non-empty token, so "" is unreachable in production. What the old
      // code did with the reachable nullptr was strdup(nullptr) — undefined —
      // and then test the result for null, which is plainly what the author
      // intended to happen. std::string makes that intent the defined
      // behaviour instead of an accident of the CRT.
      TEST_METHOD(AnAbsentNameBecomesUnknown)
      {
        ShapeMarker const marker = ParseMarker(MARKER_BLOCK, "");

        Assert::AreEqual(std::string("unknown"), marker.m_name);
      }

      // The exporter constructor stripped spaces from both names with a pair of
      // hand-written in-place compaction loops. std::erase does the same thing.
      TEST_METHOD(TheExporterConstructorStripsEverySpaceFromBothNames)
      {
        char parentName[] = "Upper Mount 3";
        DirectX::XMFLOAT4X4 const identity(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

        ShapeMarker const marker("Marker Spike 01", parentName, 0, identity);

        Assert::AreEqual(std::string("MarkerSpike01"), marker.m_name);
        Assert::AreEqual(std::string("UpperMount3"), marker.m_parentName);
      }

      // NEGATIVE CONTROL, and the reason this file exists. Converting these
      // members to std::string invites operator==, which matches strcmp. Every
      // comparison of a marker or fragment name in the tree is stricmp, and the
      // shipped data relies on it: 605 of 605 ParentName lines say "sceneroot"
      // while the code looks for "SceneRoot". If this test ever starts failing
      // on the first assert, someone has narrowed a comparison and the fragment
      // tree will not parent.
      TEST_METHOD(NameComparisonMustStayCaseInsensitive)
      {
        ShapeMarker const marker = ParseMarker(MARKER_BLOCK, "MarkerSpike01");

        Assert::IsFalse(marker.m_parentName == "SceneRoot", L"operator== is strcmp, and the shipped spelling is lowercase");
        Assert::AreEqual(0, stricmp(marker.m_parentName.c_str(), "SceneRoot"));
      }
  };
} // namespace NeuronClientTests
