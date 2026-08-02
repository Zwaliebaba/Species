#include "pch.h"

#include "LookupTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace NeuronCoreTests
{
  namespace
  {
    // Forces every key into one bucket, so the collision paths run in every
    // test that uses it.
    struct CollidingHash
    {
        size_t operator()(int) const { return 0; }
    };
  } // namespace

  // LookupTable is the sanctioned way to use a hashed container in
  // simulation code (CODING_STANDARDS.md, Determinism): it exposes no
  // iteration, so nothing order-dependent can be built on it. These tests
  // cover the operations it does expose; that it exposes nothing else is
  // enforced by its API, not a test.
  TEST_CLASS(LookupTableTests)
  {
    public:
      TEST_METHOD(FindReturnsWhatInsertStored)
      {
        Neuron::LookupTable<int, int> table;
        table.Insert(7, 700);
        table.Insert(8, 800);

        const int* found = table.Find(7);
        Assert::IsNotNull(found);
        Assert::AreEqual(700, *found);
        Assert::AreEqual(size_t{2}, table.Size());
      }

      TEST_METHOD(FindReturnsNullForAnAbsentKey)
      {
        Neuron::LookupTable<int, int> table;
        table.Insert(7, 700);

        Assert::IsNull(table.Find(99));
        Assert::IsFalse(table.Contains(99));
        Assert::IsTrue(table.Contains(7));
      }

      TEST_METHOD(InsertOverwritesAnExistingKey)
      {
        Neuron::LookupTable<int, int> table;
        table.Insert(7, 700);
        table.Insert(7, 701);

        Assert::AreEqual(size_t{1}, table.Size());
        Assert::AreEqual(701, *table.Find(7));
      }

      TEST_METHOD(EraseRemovesExactlyTheGivenKey)
      {
        Neuron::LookupTable<int, int> table;
        table.Insert(7, 700);
        table.Insert(8, 800);

        Assert::IsTrue(table.Erase(7));
        Assert::IsFalse(table.Erase(7));
        Assert::IsNull(table.Find(7));
        Assert::AreEqual(800, *table.Find(8));
      }

      TEST_METHOD(ClearEmptiesTheTable)
      {
        Neuron::LookupTable<int, int> table;
        table.Insert(7, 700);
        table.Clear();

        Assert::IsTrue(table.IsEmpty());
        Assert::AreEqual(size_t{0}, table.Size());
      }

      TEST_METHOD(CollidingKeysStayIndependent)
      {
        Neuron::LookupTable<int, int, CollidingHash> table;
        for (int key = 0; key < 50; ++key)
          table.Insert(key, key * 10);

        Assert::AreEqual(size_t{50}, table.Size());
        for (int key = 0; key < 50; ++key)
          Assert::AreEqual(key * 10, *table.Find(key));

        Assert::IsTrue(table.Erase(25));
        Assert::IsNull(table.Find(25));
        Assert::AreEqual(240, *table.Find(24));
        Assert::AreEqual(260, *table.Find(26));
      }

      TEST_METHOD(FindThroughAConstTableObservesWithoutMutating)
      {
        Neuron::LookupTable<int, int> table;
        table.Insert(7, 700);

        const auto& view = table;
        const int* found = view.Find(7);
        Assert::IsNotNull(found);
        Assert::AreEqual(700, *found);
        Assert::IsNull(view.Find(8));
      }
  };
} // namespace NeuronCoreTests
