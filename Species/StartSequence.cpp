#include "pch.h"
#include "Input.h"
#include "Win32EventHandler.h"
#include "Profiler.h"
#include "TextRenderer.h"
#include "WindowManager.h"
#include "HiResTime.h"
#include "LanguageTable.h"

#include "Eclipse.h"

#include "MainMenus.h"

#include "StartSequence.h"
#include "Camera.h"
#include "UserInput.h"
#include "Renderer.h"
#include "GlobalWorld.h"

#include "SoundSystem.h"
#include "WorldPointers.h"
#include "AppState.h"


namespace Species
{
  StartSequence::StartSequence()
  {
    m_startTime = GetHighResTime();

    float screenRatio = (float)g_renderer->ScreenH() / (float)g_renderer->ScreenW();
    int screenH = 800 * screenRatio;

    float x = 150;
    float y = screenH * 4 / 5.0f;
    float size = 10.0f;

    RegisterCaption(LANGUAGEPHRASE("intro_1"), x, y, size, 3, 15);
    RegisterCaption(LANGUAGEPHRASE("intro_2"), x, y + 15, 20, 8, 15);
    RegisterCaption(LANGUAGEPHRASE("intro_3"), x, y + 40, size, 30, 45);
    RegisterCaption(LANGUAGEPHRASE("intro_4"), x, y + 50, size, 42, 45);
    RegisterCaption(LANGUAGEPHRASE("intro_5"), x, y, size, 54, 65);
    RegisterCaption(LANGUAGEPHRASE("intro_6"), x, y, size, 66, 74);
    RegisterCaption(LANGUAGEPHRASE("intro_7"), x, y + 15, size, 72, 74);
    RegisterCaption(LANGUAGEPHRASE("intro_8"), x, y, size, 74, 90);
    RegisterCaption(LANGUAGEPHRASE("intro_9"), x, y + 15, 15, 82, 90);
    RegisterCaption(LANGUAGEPHRASE("intro_10"), x, y + 30, 15, 86, 90);
  }


  void StartSequence::RegisterCaption(char* _caption, float _x, float _y, float _size, float _startTime, float _endTime)
  {
    StartSequenceCaption* caption = new StartSequenceCaption();


    caption->m_caption = strdup(_caption);
    caption->m_x = _x;
    caption->m_y = _y;
    caption->m_size = _size;
    caption->m_startTime = _startTime;
    caption->m_endTime = _endTime;

    m_captions.push_back(caption);
  }


  bool StartSequence::Advance()
  {
    static bool started = false;
    if (GetHighResTime() > m_startTime && !started)
    {
      started = true;
      g_soundSystem->TriggerOtherEvent("StartSequence", SoundSourceBlueprint::TypeMusic);
      TheCamera()->SetDebugMode(Camera::DebugModeAuto);
      TheCamera()->RequestMode(Camera::Mode::ModeSphereWorldIntro);
    }

    g_inputManager->PollForEvents();

    if (!g_eventHandler->WindowHasFocus())
    {
      Sleep(1);
      TheUserInput()->Advance();
      return false;
    }

    if (g_inputManager->controlEvent(ControlType::ControlSkipMessage) || g_requestQuit || (GetHighResTime() - m_startTime) > 90)
    {
      g_soundSystem->StopAllSounds(WorldObjectId(), "Music StartSequence");
      return true;
    }

    TheUserInput()->Advance();
    TheCamera()->Advance();
    g_soundSystem->Advance();
#ifdef PROFILER_ENABLED
  g_profiler->Advance();
#endif // PROFILER_ENABLED

  TheRenderer()->Render();

  return false;
}


void StartSequence::Render()
{
  float screenRatio = (float)g_renderer->ScreenH() / (float)g_renderer->ScreenW();
  int screenH = 800 * screenRatio;

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, 800, screenH, 0);
  glMatrixMode(GL_MODELVIEW);

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  float timeNow = GetHighResTime() - m_startTime;

  if (timeNow < 3.0f)
  {
    float alpha = 1.0f - timeNow / 3.0f;
    glColor4f(0, 0, 0, alpha);
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(800, 0);
    glVertex2i(800, screenH);
    glVertex2i(0, screenH);
    glEnd();
  }

  if (timeNow > 87)
  {
    float alpha = (timeNow - 87) / 2.0f;
    glColor4f(1, 1, 1, alpha);
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(800, 0);
    glVertex2i(800, screenH);
    glVertex2i(0, screenH);
    glEnd();
  }

  // Braced to zero: Vector2's default constructor did it, XMFLOAT2's does not,
  // and the sentinel test below reads it whether the loop assigned it or not.
  DirectX::XMFLOAT2 cursorPos{0.0f, 0.0f};
  bool cursorFlash = false;
  float cursorSize = 0.0f;

  for (int i = 0; i < static_cast<int>(m_captions.size()); ++i)
  {
    StartSequenceCaption* caption = m_captions[i];
    if (timeNow >= caption->m_startTime && timeNow <= caption->m_endTime)
    {
      // The caption was handed to sprintf AS ITS FORMAT STRING and copied into
      // a char[256] with no bound: a caption containing a percent sign read an
      // argument nobody passed, and one over 255 characters overran the buffer.
      // It is text now. strings-modernised T9.
      std::string theString(caption->m_caption);
      int stringLength = static_cast<int>(theString.size());
      int maxTimeLength = (timeNow - caption->m_startTime) * 20;
      if (maxTimeLength < stringLength)
      {
        theString.resize(maxTimeLength);
      }

      glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
      g_gameFont.DrawText2D(caption->m_x, caption->m_y, caption->m_size, theString);

      int finishedLen = static_cast<int>(theString.size());
      int texW = g_gameFont.GetTextWidth(finishedLen, caption->m_size);
      cursorPos = DirectX::XMFLOAT2(caption->m_x + texW, caption->m_y - 7.25f);
      cursorFlash = maxTimeLength > stringLength;
      cursorSize = caption->m_size;
    }
  }

  // Vector2::operator!= compared per component against FLT_EPSILON, so this
  // sentinel test was never an exact one. Kept as it was rather than tightened.
  if (fabsf(cursorPos.x) >= FLT_EPSILON || fabsf(cursorPos.y) >= FLT_EPSILON)
  {
    if (!cursorFlash || fmod(timeNow, 1) < 0.5f)
    {
      glBegin(GL_QUADS);
      glVertex2f(cursorPos.x, cursorPos.y);
      glVertex2f(cursorPos.x + cursorSize * 0.7f, cursorPos.y);
      glVertex2f(cursorPos.x + cursorSize * 0.7f, cursorPos.y + cursorSize * 0.88f);
      glVertex2f(cursorPos.x, cursorPos.y + cursorSize * 0.88f);
      glEnd();
    }
  }

  g_renderer->SetupMatricesFor3D();


  //
  // Render grid behind darwinia

  float scale = 1000.0f;

  float fog = 0.0f;
  float fogCol[] = {fog, fog, fog, fog};

  int fogVal = 5810000;

  float r = 2.0f;
  float height = -400.0f;
  float gridSize = 100.0f;

  float xStart = -4000.0f * r;
  float xEnd = 4000.0f + 4000.0f * r;
  float zStart = -4000.0f * r;
  float zEnd = 4000.0f + 4000.0f * r;

  float fogColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  if (timeNow > 50.0f)
  {
    glPushMatrix();
    glScalef(scale, scale, scale);

    glFogf(GL_FOG_DENSITY, 1.0f);
    glFogf(GL_FOG_START, 0.0f);
    glFogf(GL_FOG_END, (float)fogVal);
    glFogfv(GL_FOG_COLOR, fogCol);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glEnable(GL_FOG);

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    glColor4f(0.5, 0.5, 1.0, 0.5);

    float percentDrawn = 1.0f - (timeNow - 50.0f) / 10.0f;
    percentDrawn = std::max(percentDrawn, 0.0f);
    xEnd -= (8000 + 4000 * r * percentDrawn);
    zEnd -= (8000 + 4000 * r * percentDrawn);

    for (int x = xStart; x < xEnd; x += gridSize)
    {
      glBegin(GL_LINES);
      glVertex3f(x, height, zStart);
      glVertex3f(x, height, zEnd);
      glVertex3f(xStart, height, x);
      glVertex3f(xEnd, height, x);
      glEnd();
    }

    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(true);

    g_globalWorld->SetupFog();
    glDisable(GL_FOG);

    glPopMatrix();
  }
}
} // namespace Species
