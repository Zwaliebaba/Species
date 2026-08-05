#include "pch.h"

#include "InputFilter.h"


namespace Neuron
{
  void InputFilter::setName(std::string const& name) { m_name = name; }


  const std::string& InputFilter::getName() const { return m_name; }


  std::ostream& operator<<(std::ostream& stream, InputFilter const& filter) { return stream << "InputFilter (" << filter.getName() << ")"; }
} // namespace Neuron
