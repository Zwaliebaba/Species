#include "pch.h"
#include "UpdateAvailableWindow.h"


namespace Species
{
  class GetItNowButton : public SpeciesButton
  {
    public:
      void MouseUp() override { EclRemoveWindow(m_parent->m_name); }
  };

  namespace
  {
    // The base takes a char const*, so the message has to outlive the
    // constructor call — which is why this used to be a file-scope char* that
    // a comma expression sprintf'd into and NOBODY EVER FREED, one leak per
    // window. A function-local static is the same lifetime with none of that;
    // the window is a singleton dialogue and is never built twice at once.
    char const* UpdateMessage(char const* _newVersion, char const* _changeLog)
    {
      static std::string message;
      message = std::format("There is a new version of Darwinia available ({}) at\n"
                            "http://www.ambrosiasw.com/games/darwinia/\n"
                            "\n"
                            "{}",
                            _newVersion, _changeLog);
      return message.c_str();
    }
  } // namespace

  UpdateAvailableWindow::UpdateAvailableWindow(const char* newVersion, const char* changeLog)
    : MessageDialog("New version available", UpdateMessage(newVersion, changeLog))
  {
    m_h += 30;
  }

  void UpdateAvailableWindow::Create()
  {
    MessageDialog::Create();

    constexpr int buttonWidth = 80;
    constexpr int buttonHeight = 18;
    SpeciesButton* button = new GetItNowButton();
    button->SetShortProperties("Get It Now!", (m_w - buttonWidth) / 2, m_h - 60, buttonWidth, buttonHeight);
    RegisterButton(button);
  }
} // namespace Species
