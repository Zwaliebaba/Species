#pragma once

#include <unordered_map>
#include <utility>

namespace Neuron
{
  // A hashed lookup whose element order can never be observed.
  //
  // CODING_STANDARDS.md (Determinism) forbids unordered containers holding
  // simulation state because their traversal order is unspecified and can
  // differ between builds of the same source. What desyncs lockstep is
  // observing that order — find, insert and erase results do not depend on
  // it. LookupTable therefore wraps std::unordered_map and deliberately
  // exposes no begin()/end(): "never iterated" is a property the compiler
  // enforces rather than a review hope. If a call site needs traversal it
  // needs a different structure — see the sanctioned patterns in
  // CODING_STANDARDS.md.
  template <typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>> class LookupTable
  {
    public:
      void Insert(const Key& _key, Value _value) { m_table.insert_or_assign(_key, std::move(_value)); }

      // Returns whether anything was removed.
      bool Erase(const Key& _key) { return m_table.erase(_key) != 0; }

      void Clear() { m_table.clear(); }

      // nullptr when the key is absent. The pointer observes; the table owns.
      [[nodiscard]] Value* Find(const Key& _key)
      {
        const auto found = m_table.find(_key);
        return found == m_table.end() ? nullptr : &found->second;
      }

      [[nodiscard]] const Value* Find(const Key& _key) const
      {
        const auto found = m_table.find(_key);
        return found == m_table.end() ? nullptr : &found->second;
      }

      [[nodiscard]] bool Contains(const Key& _key) const { return m_table.contains(_key); }

      [[nodiscard]] size_t Size() const { return m_table.size(); }

      [[nodiscard]] bool IsEmpty() const { return m_table.empty(); }

    private:
      std::unordered_map<Key, Value, Hash, KeyEqual> m_table;
  };
} // namespace Neuron
