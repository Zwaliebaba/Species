#include "pch.h"
#include "UpdateAvailableWindow.h"


namespace Species
{
  class GetItNowButton : public SpeciesButton
  {
    public:
      void MouseUp() override { EclRemoveWindow(m_parent->m_name); }
  };

  static char* buf = nullptr;

  UpdateAvailableWindow::UpdateAvailableWindow(const char* newVersion, const char* changeLog)
    : MessageDialog("New version available", (sprintf(buf = new char[512 + strlen(newVersion) + strlen(changeLog)],
                                                      "There is a new version of Darwinia available (%s) at\n"
                                                      "http://www.ambrosiasw.com/games/darwinia/\n"
                                                      "\n"
                                                      "%s",
                                                      newVersion, changeLog),
                                              buf))
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
