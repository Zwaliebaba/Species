#pragma once


#include <string>
#include <vector>

#include "SpeciesWindow.h"


namespace Species
{
  class MessageDialog : public SpeciesWindow
  {
    protected:
      // Was char*[20] with a new char[] each, filled by an strncpy and counted
      // by m_numLines — which nothing bounded, so a message of more than twenty
      // lines wrote past the end of the array. strings-modernised T9.
      std::vector<std::string> m_messageLines;

    public:
      MessageDialog(char const* _name, char const* _message);

      void Create();
      void Render(bool _hasFocus);
  };
} // namespace Species
