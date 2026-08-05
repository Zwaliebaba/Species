#pragma once

#include "MessageDialog.h"


namespace Species
{
  class UpdateAvailableWindow : public MessageDialog
  {
    public:
      UpdateAvailableWindow(const char* newVersion, const char* changeLog);
      void Create();
  };
} // namespace Species
