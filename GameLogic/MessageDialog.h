#pragma once


#include "SpeciesWindow.h"


namespace Species
{
  class MessageDialog : public SpeciesWindow
  {
    protected:
      char* m_messageLines[20];
      int m_numLines;

    public:
      MessageDialog(char const* _name, char const* _message);
      ~MessageDialog();

      void Create();
      void Render(bool _hasFocus);
  };
} // namespace Species
