#include "pch.h"
#include "Input.h"
#include "Debug.h"
#include "HiResTime.h"
#include "Win32EventHandler.h"
#include "MathUtils.h"
#include "OglExtensions.h"
#include "PersistingDebugRender.h"
#include "Preferences.h"
#include "Profiler.h"
#include "Resource.h"
#include "TextRenderer.h"
#include "WindowManager.h"
#include "LanguageTable.h"
#include "DebugRender.h"
#include "App.h"
#include "Camera.h"
#include "Explosion.h"
#include "GlobalWorld.h"
#include "LandscapeRenderer.h"
#include "Location.h"
#include "LocationEditor.h"
#include "Main.h"
#include "ParticleSystem.h"
#include "Renderer.h"
// Reads m_server->m_sequenceId for the latency readout. App.h only forward
// declares Server, and the complete type used to arrive through Location.h,
// which included Server.h without ever naming it.
#include "Server.h"
#include "TaskManager.h"
#include "TaskManagerInterface.h"
#include "Team.h"
#include "Unit.h"
#include "UserInput.h"
#include "GameCursor.h"
#include "StartSequence.h"
#include "ControlHelp.h"
#include "Eclipse.h"
#include "MessageDialog.h"
#include "InsertionSquad.h"
#include "WorldPointers.h"
#include "AppState.h"

#define USE_PIXEL_EFFECT_GRID_OPTIMISATION	1

Renderer::Renderer()
  : m_fps(60),
    m_displayFPS(false),
    m_renderDebug(false),
    m_displayInputMode(false),
    m_renderDarwinLogo(-1.0f),
    m_nearPlane(5.0f),
    m_farPlane(150000.0f),
    m_tileIndex(0),
    m_fadedness(0.0f),
    m_fadeRate(0.0f),
    m_fadeDelay(0.0f),
    m_pixelSize(256) {}

void Renderer::Initialise()
{
  m_screenW = g_prefsManager->GetInt("ScreenWidth", 0);
  m_screenH = g_prefsManager->GetInt("ScreenHeight", 0);
  bool windowed = g_prefsManager->GetInt("ScreenWindowed", 0) ? true : false;
  int colourDepth = g_prefsManager->GetInt("ScreenColourDepth", 32);
  int refreshRate = g_prefsManager->GetInt("ScreenRefresh", 75);
  int zDepth = g_prefsManager->GetInt("ScreenZDepth", 24);
  bool waitVRT = g_prefsManager->GetInt("WaitVerticalRetrace", 1);

  if (m_screenW == 0 || m_screenH == 0)
  {
    g_windowManager->SuggestDefaultRes(&m_screenW, &m_screenH, &refreshRate, &colourDepth);
    g_prefsManager->SetInt("ScreenWidth", m_screenW);
    g_prefsManager->SetInt("ScreenHeight", m_screenH);
    g_prefsManager->SetInt("ScreenRefresh", refreshRate);
    g_prefsManager->SetInt("ScreenColourDepth", colourDepth);
  }

  bool success = g_windowManager->CreateWin(m_screenW, m_screenH, windowed, colourDepth, refreshRate, zDepth, waitVRT);

  if (!success)
  {
    char caption[512];
    sprintf(caption, "Failed to set requested screen resolution of\n"
            "%d x %d, %d bit colour, %s\n\n" "Restored to safety settings of\n" "640 x 480, 16 bit colour, windowed", m_screenW, m_screenH,
            colourDepth, windowed ? "windowed" : "fullscreen");
    auto dialog = new MessageDialog("Error", caption);
    EclRegisterWindow(dialog);
    dialog->m_x = 100;
    dialog->m_y = 100;

    // Go for safety values
    m_screenW = 640;
    m_screenH = 480;
    windowed = true;
    colourDepth = 16;
    zDepth = 16;
    refreshRate = 60;

    success = g_windowManager->CreateWin(m_screenW, m_screenH, windowed, colourDepth, refreshRate, zDepth, waitVRT);
    if (!success)
    {
      // next try with 24bit z (colour depth is automatic in windowed mode)
      zDepth = 24;
      success = g_windowManager->CreateWin(m_screenW, m_screenH, windowed, colourDepth, refreshRate, zDepth, waitVRT);
    }
    ASSERT_TEXT(success, "Failed to set screen mode");

    g_prefsManager->SetInt("ScreenWidth", m_screenW);
    g_prefsManager->SetInt("ScreenHeight", m_screenH);
    g_prefsManager->SetInt("ScreenWindowed", 1);
    g_prefsManager->SetInt("ScreenColourDepth", colourDepth);
    g_prefsManager->SetInt("ScreenRefresh", 60);
    g_prefsManager->Save();
  }

  InitialiseOGLExtensions();

  BuildOpenGlState();
}

void Renderer::Restart() { BuildOpenGlState(); }

void Renderer::BuildOpenGlState() { glGenTextures(1, &m_pixelEffectTexId); }

void Renderer::RenderFlatTexture()
{
  glColor3ubv(g_colourWhite.GetData());
  glEnable(GL_TEXTURE_2D);
  int textureId = g_app->m_resource->GetTexture("Textures/privatedemo.bmp", true, true);
  if (textureId == -1)
    return;
  glBindTexture(GL_TEXTURE_2D, textureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

  float size = m_nearPlane * 0.3f;
  Vector3 up = TheCamera()->GetUp() * 1.0f * size;
  Vector3 right = TheCamera()->GetRight() * 1.0f * size;
  Vector3 pos = TheCamera()->GetPos() + TheCamera()->GetFront() * m_nearPlane * 1.01f;

  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_ALPHA_TEST);
  glAlphaFunc(GL_GREATER, 0.02f);

  glBegin(GL_QUADS);
  glTexCoord2f(1.0f, 1.0f);
  glVertex3fv((pos + up - right).GetData());
  glTexCoord2f(0.0f, 1.0f);
  glVertex3fv((pos + up + right).GetData());
  glTexCoord2f(0.0f, 0.0f);
  glVertex3fv((pos - up + right).GetData());
  glTexCoord2f(1.0f, 0.0f);
  glVertex3fv((pos - up - right).GetData());
  glEnd();

  glAlphaFunc(GL_GREATER, 0.01);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_BLEND);

  glDisable(GL_TEXTURE_2D);

  glLineWidth(1.0f);
  glBegin(GL_LINE_LOOP);
  glVertex3fv((pos + up - right).GetData());
  glVertex3fv((pos + up + right).GetData());
  glVertex3fv((pos - up + right).GetData());
  glVertex3fv((pos - up - right).GetData());
  glEnd();
}

void Renderer::RenderLogo()
{
  glColor3ubv(g_colourWhite.GetData());
  glEnable(GL_BLEND);
  glDepthMask(false);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glColor4ub(0, 0, 0, 200);
  float logoW = 200;
  float logoH = 35;
  glBegin(GL_QUADS);
  glVertex2f(m_screenW - logoW - 10, m_screenH - logoH - 10);
  glVertex2f(m_screenW - 10, m_screenH - logoH - 10);
  glVertex2f(m_screenW - 10, m_screenH - 10);
  glVertex2f(m_screenW - logoW - 10, m_screenH - 10);
  glEnd();

  glColor4ub(255, 255, 255, 255);
  glEnable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  int textureId = g_app->m_resource->GetTexture("Textures/privatedemo.bmp", true, false);
  if (textureId == -1)
    return;
  glBindTexture(GL_TEXTURE_2D, textureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(m_screenW - logoW - 10, m_screenH - logoH - 10);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(m_screenW - 10, m_screenH - logoH - 10);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(m_screenW - 10, m_screenH - 10);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(m_screenW - logoW - 10, m_screenH - 10);
  glEnd();

  glDepthMask(true);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glColor4f(1.0f, 0.75f, 0.75f, 1.0f);
  g_gameFont.DrawText2D(20, m_screenH - 70, 25, LANGUAGEPHRASE("privatedemo1"));
  g_gameFont.DrawText2D(20, m_screenH - 40, 25, LANGUAGEPHRASE("privatedemo2"));
  g_gameFont.DrawText2D(20, m_screenH - 10, 10, LANGUAGEPHRASE("privatedemo2"));
}

void Renderer::Render()
{
#ifdef PROFILER_ENABLED
  g_app->m_profiler->RenderStarted();
#endif

  RenderFrame();

#ifdef PROFILER_ENABLED
  g_app->m_profiler->RenderEnded();
#endif // PROFILER_ENABLED
}

bool Renderer::IsFadeComplete() const
{
  if (NearlyEquals(m_fadeRate, 0.0f))
    return true;

  return false;
}

void Renderer::StartFadeOut()
{
  m_fadeDelay = 0.0f;
  m_fadeRate = 1.0f;
}

void Renderer::StartFadeIn(float _delay)
{
  m_fadedness = 1.0f;
  m_fadeDelay = _delay;
  m_fadeRate = -1.0f;
}

void Renderer::RenderFadeOut()
{
  static double lastTime = GetHighResTime();
  double timeNow = GetHighResTime();
  double timeIncrement = timeNow - lastTime;
  if (timeIncrement > 0.05f)
    timeIncrement = 0.05f;
  lastTime = timeNow;

  if (m_fadeDelay > 0.0f)
    m_fadeDelay -= timeIncrement;
  else
  {
    m_fadedness += m_fadeRate * timeIncrement;
    if (m_fadedness < 0.0f)
    {
      m_fadedness = 0.0f;
      m_fadeRate = 0.0f;
    }
    else if (m_fadedness > 1.0f)
    {
      m_fadeRate = 0.0f;
      m_fadedness = 1.0f;
    }
  }

  if (m_fadedness > 0.0001f)
  {
    glEnable(GL_BLEND);
    glDepthMask(false);
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4ub(0, 0, 0, static_cast<int>(m_fadedness * 255.0f));
    glBegin(GL_QUADS);
    glVertex2i(-1, -1);
    glVertex2i(m_screenW, -1);
    glVertex2i(m_screenW, m_screenH);
    glVertex2i(-1, m_screenH);
    glEnd();

    glDisable(GL_BLEND);
    glDepthMask(true);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }
}

void Renderer::RenderPaused()
{
  auto msg = "PAUSED";
  int x = g_renderer->ScreenW() / 2;
  int y = g_renderer->ScreenH() / 2;
  TextRenderer& font = g_gameFont;

  font.BeginText2D();

  // Black Background
  g_gameFont.SetRenderShadow(true);
  glColor4f(0.3f, 0.3f, 0.3f, 0.0f);
  font.DrawText2DCentre(x, y, 80, msg);

  // White Foreground
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  font.SetRenderShadow(false);
  font.DrawText2DCentre(x, y, 80, msg);

  font.EndText2D();
}

void Renderer::RenderFrame(bool withFlip)
{
  int renderPixelShaderPref = g_prefsManager->GetInt("RenderPixelShader");

  SetOpenGLState();

  if (g_locationId == -1)
  {
    m_nearPlane = 50.0f;
    m_farPlane = 10000000.0f;
  }
  else
  {
    m_nearPlane = 5.0f;
    m_farPlane = 15000.0f;
  }

  FPSMeterAdvance();
  SetupMatricesFor3D();

  START_PROFILE(g_app->m_profiler, "Render Clear");
  RGBAColour* col = &g_app->m_backgroundColour;
  if (g_location)
    glClearColor(col->r / 255.0f, col->g / 255.0f, col->b / 255.0f, col->a / 255.0f);
  else
    glClearColor(0.05f, 0.0f, 0.05f, 0.1f);
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
  END_PROFILE(g_app->m_profiler, "Render Clear");

  bool deformStarted = false;

  if (g_editing)
  {
    if (g_locationId != -1)
    {
#ifdef LOCATION_EDITOR
      SetupMatricesFor3D();
      g_location->Render();
      TheLocationEditor()->Render();
#endif // LOCATION_EDITOR
    }
    else
      g_globalWorld->Render();
  }
  else
  {
    if (g_locationId != -1)
    {
      if (renderPixelShaderPref > 0)
      {
          PreRenderPixelEffect();

          START_PROFILE(g_app->m_profiler, "Render Clear");
          glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
          END_PROFILE(g_app->m_profiler, "Render Clear");
          g_location->Render();

          ApplyPixelEffect();

          SetupMatricesFor3D();
      }
      else
        g_location->Render();
    }
    else
      g_globalWorld->Render();
  }

  CHECK_OPENGL_STATE();
  TheControlHelp()->Render();
  g_explosionManager.Render();
  g_particleSystem->Render();

  TheUserInput()->Render();
  g_app->m_gameCursor->Render();
  TheTaskManagerInterface()->Render();
  TheCamera()->Render();

#ifdef DEBUG_RENDER_ENABLED
  g_debugRenderer.Render();
#endif
  CHECK_OPENGL_STATE();

  //	RenderFlatTexture();

  //if (m_renderingPoster == PosterMakerInactive)
  {}

  g_editorFont.BeginText2D();

  RenderFadeOut();

  if (m_displayFPS)
  {
    glColor4f(0, 0, 0, 0.6);
    glBegin(GL_QUADS);
    glVertex2f(8.0, 1.0f);
    glVertex2f(70.0, 1.0f);
    glVertex2f(70.0, 15.0f);
    glVertex2f(8.0, 15.0f);
    glEnd();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    g_editorFont.DrawText2D(12, 10, DEF_FONT_SIZE, "FPS: %d", m_fps);
    //		g_editorFont.DrawText2D( 150, 10, DEF_FONT_SIZE, "TFPS: %2.0f", g_targetFrameRate);
    //		Vector3 const camPos = TheCamera()->GetPos();
    //		g_editorFont.DrawText2D( 150, 10, DEF_FONT_SIZE, "cam: %.1f, %.1f, %.1f", camPos.x, camPos.y, camPos.z);
  }

  if (m_displayInputMode)
  {
    glColor4f(0, 0, 0, 0.6);
    glBegin(GL_QUADS);
    glVertex2f(80.0, 1.0f);
    glVertex2f(230.0, 1.0f);
    glVertex2f(230.0, 18.0f);
    glVertex2f(80.0, 18.0f);
    glEnd();

    std::string inmode;
    switch (g_inputManager->getInputMode())
    {
    case INPUT_MODE_KEYBOARD:
      inmode = "keyboard";
      break;
    case INPUT_MODE_GAMEPAD:
      inmode = "gamepad";
      break;
    default:
      inmode = "unknown";
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    g_editorFont.DrawText2D(84, 10, DEF_FONT_SIZE, "InputMode: %s", inmode.c_str());
  }

  if (g_editing)
  {
    g_gameFont.DrawText2DCentre(m_screenW / 2, 10, 17, "= EDITOR ENABLED =");

    if (g_locationId != -1)
    {
      g_editorFont.DrawText2D(m_screenW - 300, m_screenH - 40, DEF_FONT_SIZE, "Triangles : %d",
                              g_location->m_landscape.m_renderer->m_numTriangles);
      g_editorFont.DrawText2D(m_screenW - 300, m_screenH - 25, DEF_FONT_SIZE, "Mission   : %s", g_app->m_requestedMission);
      g_editorFont.DrawText2D(m_screenW - 300, m_screenH - 10, DEF_FONT_SIZE, "Map       : %s", g_app->m_requestedMap);
    }
  }

  if (g_server)
  {
    int latency = g_server->m_sequenceId - g_lastProcessedSequenceId;
    if (latency > 10)
    {
      glColor4f(0.0f, 0.0f, 0.0f, 0.8f);
      glBegin(GL_QUADS);
      glVertex2f(m_screenW / 2 - 200, 120);
      glVertex2f(m_screenW / 2 + 200, 120);
      glVertex2f(m_screenW / 2 + 200, 80);
      glVertex2f(m_screenW / 2 - 200, 80);
      glEnd();
      glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
      g_editorFont.DrawText2DCentre(m_screenW / 2, 100, 20, "Client LAG %dms behind Server ", latency * 100);
    }
  }

  if (g_app->m_startSequence)
    g_app->m_startSequence->Render();

  if (m_renderDarwinLogo >= 0.0f)
  {
    int textureId = g_app->m_resource->GetTexture("Icons/DarwinResearchAssociates.bmp");

    glBindTexture(GL_TEXTURE_2D, textureId);
    glEnable(GL_TEXTURE_2D);

    float y = m_screenH * 0.05f;
    float h = m_screenH * 0.7f;

    float w = h;
    float x = m_screenW / 2 - w / 2;

    float alpha = 0.0f;

    float timeNow = GetHighResTime();
    if (timeNow > m_renderDarwinLogo + 10)
      m_renderDarwinLogo = -1.0f;
    else if (timeNow < m_renderDarwinLogo + 3)
      alpha = (timeNow - m_renderDarwinLogo) / 3.0f;
    else if (timeNow > m_renderDarwinLogo + 8)
      alpha = 1.0f - (timeNow - m_renderDarwinLogo - 8) / 2.0f;
    else
      alpha = 1.0f;

    alpha = max(alpha, 0.0f);
    alpha = min(alpha, 1.0f);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);
    glEnable(GL_BLEND);
    glColor4f(alpha, alpha, alpha, 0.0f);

    for (float dx = -4; dx <= 4; dx += 4)
    {
      for (float dy = -4; dy <= 4; dy += 4)
      {
        glBegin(GL_QUADS);
        glTexCoord2i(0, 1);
        glVertex2f(x + dx, y + dy);
        glTexCoord2i(1, 1);
        glVertex2f(x + w + dx, y + dy);
        glTexCoord2i(1, 0);
        glVertex2f(x + w + dx, y + h + dy);
        glTexCoord2i(0, 0);
        glVertex2f(x + dx, y + h + dy);
        glEnd();
      }
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(1.0f, 1.0f, 1.0f, alpha);

    glBegin(GL_QUADS);
    glTexCoord2i(0, 1);
    glVertex2f(x, y);
    glTexCoord2i(1, 1);
    glVertex2f(x + w, y);
    glTexCoord2i(1, 0);
    glVertex2f(x + w, y + h);
    glTexCoord2i(0, 0);
    glVertex2f(x, y + h);
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);

    float textSize = m_screenH / 9.0f;
    g_gameFont.SetRenderOutline(true);
    glColor4f(alpha, alpha, alpha, 0.0f);
    g_gameFont.DrawText2DCentre(m_screenW / 2, m_screenH * 0.8f, textSize, "DARWINIA");
    g_gameFont.SetRenderOutline(false);
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    g_gameFont.DrawText2DCentre(m_screenW / 2, m_screenH * 0.8f, textSize, "DARWINIA");
  }

  g_editorFont.EndText2D();

  if (!g_eventHandler->WindowHasFocus() || g_app->m_paused)
    RenderPaused();

  START_PROFILE(g_app->m_profiler, "GL Flip");

  if (withFlip)
    g_windowManager->Flip();

  END_PROFILE(g_app->m_profiler, "GL Flip");

  CHECK_OPENGL_STATE();
}

int Renderer::ScreenW() const { return m_screenW; }

int Renderer::ScreenH() const { return m_screenH; }

void Renderer::SetupProjMatrixFor3D() const
{
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  gluPerspective(TheCamera()->GetFov(), static_cast<float>(m_screenW) / static_cast<float>(m_screenH), // Aspect ratio
                 m_nearPlane, m_farPlane);
}

void Renderer::SetupMatricesFor3D() const
{
  Camera* camera = TheCamera();

  SetupProjMatrixFor3D();
  camera->SetupModelviewMatrix();
}

void Renderer::SetupMatricesFor2D() const
{
  int v[4];
  glGetIntegerv(GL_VIEWPORT, &v[0]);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  float left = v[0];
  float right = v[0] + v[2];
  float bottom = v[1] + v[3];
  float top = v[1];
  //	glOrtho(left, right, bottom, top, -m_nearPlane, -m_farPlane);
  gluOrtho2D(left, right, bottom, top);
  glMatrixMode(GL_MODELVIEW);
}

void Renderer::FPSMeterAdvance()
{
  static int framesThisSecond = 0;
  static double endOfSecond = GetHighResTime() + 1.0;

  framesThisSecond++;

  double currentTime = GetHighResTime();
  if (currentTime > endOfSecond)
  {
    if (currentTime > endOfSecond + 2.0)
      endOfSecond = currentTime + 1.0;
    else
      endOfSecond += 1.0;
    m_fps = framesThisSecond;
    framesThisSecond = 0;
  }
}

float Renderer::GetNearPlane() const { return m_nearPlane; }

float Renderer::GetFarPlane() const { return m_farPlane; }

void Renderer::SetNearAndFar(float _nearPlane, float _farPlane)
{
  DEBUG_ASSERT(_nearPlane < _farPlane);
  DEBUG_ASSERT(_nearPlane > 0.0f);
  m_nearPlane = _nearPlane;
  m_farPlane = _farPlane;
}

int Renderer::GetGLStateInt(int pname) const
{
  int returnVal;
  glGetIntegerv(pname, &returnVal);
  return returnVal;
}

float Renderer::GetGLStateFloat(int pname) const
{
  float returnVal;
  glGetFloatv(pname, &returnVal);
  return returnVal;
}

void Renderer::CheckOpenGLState() const
{
  return;
  int results[10];
  float resultsf[10];

  DEBUG_ASSERT(glGetError() == GL_NO_ERROR);

  // Geometry
  //	DEBUG_ASSERT(glIsEnabled(GL_CULL_FACE));
  DEBUG_ASSERT(GetGLStateInt(GL_FRONT_FACE) == GL_CCW);
  glGetIntegerv(GL_POLYGON_MODE, results);
  DEBUG_ASSERT(results[0] == GL_FILL);
  DEBUG_ASSERT(results[1] == GL_FILL);
  DEBUG_ASSERT(GetGLStateInt(GL_SHADE_MODEL) == GL_FLAT);
  DEBUG_ASSERT(!glIsEnabled(GL_NORMALIZE));

  // Colour
  DEBUG_ASSERT(!glIsEnabled(GL_COLOR_MATERIAL));
  DEBUG_ASSERT(GetGLStateInt(GL_COLOR_MATERIAL_FACE) == GL_FRONT_AND_BACK);
  DEBUG_ASSERT(GetGLStateInt(GL_COLOR_MATERIAL_PARAMETER) == GL_AMBIENT_AND_DIFFUSE);

  // Lighting
  DEBUG_ASSERT(!glIsEnabled(GL_LIGHTING));

  DEBUG_ASSERT(!glIsEnabled(GL_LIGHT0));
  DEBUG_ASSERT(!glIsEnabled(GL_LIGHT1));
  DEBUG_ASSERT(!glIsEnabled(GL_LIGHT2));
  DEBUG_ASSERT(!glIsEnabled(GL_LIGHT3));
  DEBUG_ASSERT(!glIsEnabled(GL_LIGHT4));
  DEBUG_ASSERT(!glIsEnabled(GL_LIGHT5));
  DEBUG_ASSERT(!glIsEnabled(GL_LIGHT6));
  DEBUG_ASSERT(!glIsEnabled(GL_LIGHT7));

  glGetFloatv(GL_LIGHT_MODEL_AMBIENT, resultsf);
  DEBUG_ASSERT(resultsf[0] < 0.001f && resultsf[1] < 0.001f && resultsf[2] < 0.001f && resultsf[3] < 0.001f);

  if (g_location)
  {
    for (int i = 0; i < g_location->m_lights.Size(); i++)
    {
      Light* light = g_location->m_lights.GetData(i);

      float amb = 0.0f;
      GLfloat ambCol1[] = {amb, amb, amb, 1.0f};

      GLfloat pos1_actual[4];
      GLfloat ambient1_actual[4];
      GLfloat diffuse1_actual[4];
      GLfloat specular1_actual[4];

      glGetLightfv(GL_LIGHT0 + i, GL_POSITION, pos1_actual);
      glGetLightfv(GL_LIGHT0 + i, GL_DIFFUSE, diffuse1_actual);
      glGetLightfv(GL_LIGHT0 + i, GL_SPECULAR, specular1_actual);
      glGetLightfv(GL_LIGHT0 + i, GL_AMBIENT, ambient1_actual);

      for (int i = 0; i < 4; i++)
      {
        //			DEBUG_ASSERT(fabsf(lightPos1[i] - pos1_actual[i]) < 0.001f);
        //			DEBUG_ASSERT(fabsf(light->m_colour[i] - diffuse1_actual[i]) < 0.001f);
        //			DEBUG_ASSERT(fabsf(light->m_colour[i] - specular1_actual[i]) < 0.0001f);
        //			DEBUG_ASSERT(fabsf(ambCol1[i] - ambient1_actual[i]) < 0.001f);
      }
    }
  }

  // Blending, Anti-aliasing, Fog and Polygon Offset
  //	DEBUG_ASSERT(!glIsEnabled(GL_BLEND));
  DEBUG_ASSERT(GetGLStateInt(GL_BLEND_DST) == GL_ONE_MINUS_SRC_ALPHA);
  DEBUG_ASSERT(GetGLStateInt(GL_BLEND_SRC) == GL_SRC_ALPHA);
  DEBUG_ASSERT(!glIsEnabled(GL_ALPHA_TEST));
  DEBUG_ASSERT(GetGLStateInt(GL_ALPHA_TEST_FUNC) == GL_GREATER);
  DEBUG_ASSERT(GetGLStateFloat(GL_ALPHA_TEST_REF) == 0.01f);
  DEBUG_ASSERT(!glIsEnabled(GL_FOG));
  DEBUG_ASSERT(GetGLStateFloat(GL_FOG_DENSITY) == 1.0f);
  DEBUG_ASSERT(GetGLStateFloat(GL_FOG_END) >= 4000.0f);
  //DEBUG_ASSERT(GetGLStateFloat(GL_FOG_START) >= 1000.0f);
  glGetFloatv(GL_FOG_COLOR, resultsf);
  //	DEBUG_ASSERT(fabsf(resultsf[0] - g_location->m_backgroundColour.r/255.0f) < 0.001f);
  //	DEBUG_ASSERT(fabsf(resultsf[1] - g_location->m_backgroundColour.g/255.0f) < 0.001f);
  //	DEBUG_ASSERT(fabsf(resultsf[2] - g_location->m_backgroundColour.b/255.0f) < 0.001f);
  //	DEBUG_ASSERT(fabsf(resultsf[3] - g_location->m_backgroundColour.a/255.0f) < 0.001f);
  DEBUG_ASSERT(GetGLStateInt(GL_FOG_MODE) == GL_LINEAR);
  DEBUG_ASSERT(!glIsEnabled(GL_LINE_SMOOTH));
  DEBUG_ASSERT(!glIsEnabled(GL_POINT_SMOOTH));

  // Texture Mapping
  DEBUG_ASSERT(!glIsEnabled(GL_TEXTURE_2D));
  glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, results);
  DEBUG_ASSERT(results[0] == GL_CLAMP);
  glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, results);
  DEBUG_ASSERT(results[0] == GL_CLAMP);
  glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, results);
  DEBUG_ASSERT(results[0] == GL_MODULATE);
  glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, results);
  DEBUG_ASSERT(results[0] == 0);
  DEBUG_ASSERT(results[1] == 0);
  DEBUG_ASSERT(results[2] == 0);
  DEBUG_ASSERT(results[3] == 0);

  // Frame Buffer
  DEBUG_ASSERT(glIsEnabled(GL_DEPTH_TEST));
  DEBUG_ASSERT(GetGLStateInt(GL_DEPTH_WRITEMASK) != 0);
  DEBUG_ASSERT(GetGLStateInt(GL_DEPTH_FUNC) == GL_LEQUAL);
  DEBUG_ASSERT(glIsEnabled(GL_SCISSOR_TEST) == 0);

  // Hints
  DEBUG_ASSERT(GetGLStateInt(GL_FOG_HINT) == GL_DONT_CARE);
  DEBUG_ASSERT(GetGLStateInt(GL_POLYGON_SMOOTH_HINT) == GL_DONT_CARE);
}

void Renderer::SetOpenGLState() const
{
  // Geometry
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CCW);
  glPolygonMode(GL_FRONT, GL_FILL);
  glPolygonMode(GL_BACK, GL_FILL);
  glShadeModel(GL_FLAT);
  glDisable(GL_NORMALIZE);

  // Colour
  glDisable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

  // Lighting
  glDisable(GL_LIGHTING);
  glDisable(GL_LIGHT0);
  glDisable(GL_LIGHT1);
  glDisable(GL_LIGHT2);
  glDisable(GL_LIGHT3);
  glDisable(GL_LIGHT4);
  glDisable(GL_LIGHT5);
  glDisable(GL_LIGHT6);
  glDisable(GL_LIGHT7);
  if (g_location)
    g_location->SetupLights();
  else
    g_globalWorld->SetupLights();
  float ambient[] = {0, 0, 0, 0};
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

  // Blending, Anti-aliasing, Fog and Polygon Offset
  glDisable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_ALPHA_TEST);
  glAlphaFunc(GL_GREATER, 0.01);
  if (g_location)
    g_location->SetupFog();
  else
    g_globalWorld->SetupFog();
  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_POINT_SMOOTH);

  // Texture Mapping
  glDisable(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  int colour[4] = {0, 0, 0, 0};
  glTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, colour);

  // Frame Buffer
  glEnable(GL_DEPTH_TEST);
  glDepthMask(true);
  glDepthFunc(GL_LEQUAL);
  //	glStencilMask	(0x00);
  //	glScissor		(400, 100, 400, 400);
  glDisable(GL_SCISSOR_TEST);

  // Hints
  glHint(GL_FOG_HINT, GL_DONT_CARE);
  glHint(GL_POLYGON_SMOOTH_HINT, GL_DONT_CARE);
}

void Renderer::SetObjectLighting() const
{
  float spec = 0.0f;
  float diffuse = 1.0f;
  float amb = 0.0f;
  GLfloat materialShininess[] = {127.0f};
  GLfloat materialSpecular[] = {spec, spec, spec, 0.0f};
  GLfloat materialDiffuse[] = {diffuse, diffuse, diffuse, 1.0f};
  GLfloat ambCol[] = {amb, amb, amb, 1.0f};

  glMaterialfv(GL_FRONT, GL_SPECULAR, materialSpecular);
  glMaterialfv(GL_FRONT, GL_DIFFUSE, materialDiffuse);
  glMaterialfv(GL_FRONT, GL_SHININESS, materialShininess);
  glMaterialfv(GL_FRONT, GL_AMBIENT, ambCol);

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
}

void Renderer::UnsetObjectLighting() const
{
  glDisable(GL_LIGHTING);
  glDisable(GL_LIGHT0);
  glDisable(GL_LIGHT1);
}

void Renderer::PreRenderPixelEffect()
{
  START_PROFILE(g_app->m_profiler, "Pixel Pre-render");

  UpdateTotalMatrix();

  //
  // Reset pixel effect grid cell distances to infinity

  for (int y = 0; y < PIXEL_EFFECT_GRID_RES; ++y)
  {
    for (int x = 0; x < PIXEL_EFFECT_GRID_RES; ++x)
      m_pixelEffectGrid[y][x] = 1e9;
  }
  // memset(m_pixelEffectGrid, 0, sizeof(m_pixelEffectGrid));

  float timeSinceAdvance = g_predictionTime;

  //
  // Blend our old glow texture into place

  START_PROFILE(g_app->m_profiler, "blend old");
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_pixelEffectTexId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glEnable(GL_BLEND);
  glDepthMask(false);

  g_editorFont.BeginText2D();

  float upSpeed = 2.0f;
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  glDisable(GL_TEXTURE_2D); // *
  glEnable(GL_BLEND);
  //glDisable( GL_BLEND ); // *
  glDisable(GL_CULL_FACE); // *
  //glBegin( GL_QUADS );
  //    glTexCoord2f( 0.0f, 1.0f );     glVertex2f( 0, m_screenH-upSpeed );
  //    glTexCoord2f( 1.0f, 1.0f );     glVertex2f( m_pixelSize, m_screenH-upSpeed);
  //    glTexCoord2f( 1.0f, 0.0f );     glVertex2f( m_pixelSize, m_screenH - m_pixelSize -upSpeed );
  //    glTexCoord2f( 0.0f, 0.0f );     glVertex2f( 0, m_screenH - m_pixelSize - upSpeed);
  //glEnd();
  g_editorFont.EndText2D();
  glEnable(GL_TEXTURE_2D); // *
  END_PROFILE(g_app->m_profiler, "blend old");

  //glDisable           (GL_TEXTURE_2D);

  //
  // Draw all pixelated objects to the screen
  // Find the nearest pixelated object and update m_pixelSize at the end

  START_PROFILE(g_app->m_profiler, "Draw pixelated");
  glViewport(0, 0, m_pixelSize, m_pixelSize);
  float nearest = 99999.9f;

  float cutoff = 1000.0f;
  Vector3 camPos = TheCamera()->GetPos();

  for (int t = 0; t < NUM_TEAMS; ++t)
  {
    if (g_location->m_teams[t].m_teamType != Team::TeamTypeUnused)
    {
      for (int i = 0; i < g_location->m_teams[t].m_units.Size(); ++i)
      {
        if (g_location->m_teams[t].m_units.ValidIndex(i))
        {
          Unit* unit = g_location->m_teams[t].m_units[i];
          if (unit->m_troopType == Entity::TypeInsertionSquadie || unit->m_troopType == Entity::TypeCentipede)
          {
            if (unit->IsInView())
            {
              float distance = (unit->m_centrePos - camPos).Mag();
              if (distance < cutoff)
              {
                for (int j = 0; j < unit->m_entities.Size(); ++j)
                {
                  if (unit->m_entities.ValidIndex(j))
                  {
                    Entity* entity = unit->m_entities[j];
                    bool rendered = false;
                    if (j <= unit->m_entities.GetLastUpdated())
                      rendered = entity->RenderPixelEffect(g_predictionTime);
                    else
                      rendered = entity->RenderPixelEffect(g_predictionTime + SERVER_ADVANCE_PERIOD);
                    if (rendered)
                    {
                      float distance = (entity->m_pos - TheCamera()->GetPos()).Mag();
                      if (distance < nearest)
                        nearest = distance;
                    }
                  }
                }
              }
            }
          }
        }
      }

      for (int i = 0; i < g_location->m_teams[t].m_others.Size(); ++i)
      {
        if (g_location->m_teams[t].m_others.ValidIndex(i))
        {
          Entity* entity = g_location->m_teams[t].m_others[i];
          if (entity->IsInView())
          {
            float distance = (entity->m_pos - camPos).Mag();
            if (distance < cutoff)
            {
              bool rendered = false;
              if (i <= g_location->m_teams[t].m_others.GetLastUpdated())
                rendered = entity->RenderPixelEffect(g_predictionTime);
              else
                rendered = entity->RenderPixelEffect(g_predictionTime + SERVER_ADVANCE_PERIOD);
              if (rendered)
              {
                float distance = (entity->m_pos - TheCamera()->GetPos()).Mag();
                if (distance < nearest)
                  nearest = distance;
              }
            }
          }
        }
      }
    }
  }

  for (int i = 0; i < g_location->m_buildings.Size(); ++i)
  {
    if (g_location->m_buildings.ValidIndex(i))
    {
      Building* building = g_location->m_buildings[i];
      float distance = (building->m_centrePos - camPos).Mag();
      if (distance < cutoff)
      {
        bool rendered = building->RenderPixelEffect(g_predictionTime);
        if (rendered)
        {
          float distance = (building->m_pos - TheCamera()->GetPos()).Mag();
          if (distance < nearest)
            nearest = distance;
        }
      }
    }
  }

  END_PROFILE(g_app->m_profiler, "Draw pixelated");
  glViewport(0, 0, m_screenW, m_screenH);

  //
  // Copy the screen to a texture

  START_PROFILE(g_app->m_profiler, "Gen new texture");
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_pixelEffectTexId);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, m_pixelSize, m_pixelSize, 0);

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  END_PROFILE(g_app->m_profiler, "Gen new texture");

  glDepthMask(true);

  CHECK_OPENGL_STATE();

  //
  // Update pixel size

  //if      ( nearest < 30 )        m_pixelSize = 128;
  //else if ( nearest < 200 )       m_pixelSize = 256;
  //else                            m_pixelSize = 512;

  END_PROFILE(g_app->m_profiler, "Pixel Pre-render");
}

#define d3dOneMinus( _x ) _x

void Renderer::PaintPixels()
{
#if USE_PIXEL_EFFECT_GRID_OPTIMISATION
  const double aspectRatio = static_cast<double>(m_screenW) / static_cast<double>(m_screenH);
  double zoomCorrection = 0.000037 * static_cast<double>(TheCamera()->GetFov());
  double scale = (0.017 + zoomCorrection) * static_cast<double>(TheCamera()->GetFov());

  const double step = scale * aspectRatio / static_cast<double>(PIXEL_EFFECT_GRID_RES);
  const double xOffset = scale * (-0.5 * aspectRatio);
  const double yOffset = scale * -0.5;
  float gridToTexture = 1.0f / static_cast<float>(PIXEL_EFFECT_GRID_RES);
  float gridToTextureY = gridToTexture * (static_cast<float>(m_screenW) / static_cast<float>(m_screenH));
  float distance = 1.0f;
  const double cellsUsed = static_cast<double>(PIXEL_EFFECT_GRID_RES) / aspectRatio;
  const int cellsToSkip = PIXEL_EFFECT_GRID_RES - ceil(cellsUsed);

  const int yGridRes = PIXEL_EFFECT_GRID_RES - cellsToSkip;
  glBegin(GL_QUADS);
  for (int y = 0; y < yGridRes; ++y)
  {
    double y1 = static_cast<double>(y) * step + yOffset;
    double y2 = y1 + step;
    float ty = gridToTextureY * static_cast<float>(y);

    for (int x = 0; x < PIXEL_EFFECT_GRID_RES; ++x)
    {
      if (m_pixelEffectGrid[x][y] < 1e9)
      {
        distance = m_pixelEffectGrid[x][y];
        if (distance < m_nearPlane)
          distance = m_nearPlane + 0.1;
        double x1 = static_cast<double>(x) * step + xOffset;
        double x2 = x1 + step;
        x1 *= distance;
        x2 *= distance;
        double y1a = y1 * distance;
        double y2a = y2 * distance;
        float tx = static_cast<float>(x) * gridToTexture;

        // Direct3D renders the texture upside down for some reason, so we flip the
        // texture coordinates here.

        glTexCoord2f(tx, d3dOneMinus(ty));
        glVertex3d(x1, y1a, -distance);

        glTexCoord2f(tx + gridToTexture, d3dOneMinus(ty));
        glVertex3d(x2, y1a, -distance);

        glTexCoord2f(tx + gridToTexture, d3dOneMinus(ty + gridToTextureY));
        glVertex3d(x2, y2a, -distance);

        glTexCoord2f(tx, d3dOneMinus(ty + gridToTextureY));
        glVertex3d(x1, y2a, -distance);
      }
    }
  }
  glEnd();
#else
  glBegin(GL_QUADS); glTexCoord2i(0, 0); glVertex2i(0, 0); glTexCoord2i(1, 0); glVertex2i(m_screenW, 0); glTexCoord2i(1, 1);
  glVertex2i(m_screenW, m_screenH); glTexCoord2i(0, 1); glVertex2i(0, m_screenH); glEnd();
#endif
}

void Renderer::ApplyPixelEffect()
{
  //SetupMatricesFor2D	();
  //glDisable			(GL_DEPTH_TEST);
  //glDisable           (GL_CULL_FACE);
  //glEnable            (GL_BLEND);

  //glEnable            (GL_TEXTURE_2D);
  //glBindTexture       (GL_TEXTURE_2D, m_pixelEffectTexId );
  //glTexParameteri	    (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
  //glTexParameteri	    (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

  //glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

  //glBegin( GL_QUADS );
  //    glBegin( GL_QUADS );
  //    glTexCoord2i(0,0);          glVertex2i( 0, 0 );
  //    glTexCoord2i(1,0);          glVertex2i( m_screenW/4, 0 );
  //    glTexCoord2i(1,1);          glVertex2i( m_screenW/4, m_screenH/4 );
  //    glTexCoord2i(0,1);          glVertex2i( 0, m_screenH/4 );
  //    glEnd();
  //glEnd();

  //glDisable           (GL_TEXTURE_2D);
  //glEnable            (GL_DEPTH_TEST);

  //return;

  START_PROFILE(g_app->m_profiler, "Pixel Apply");

  CHECK_OPENGL_STATE();

  glEnable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDepthMask(false);

#if USE_PIXEL_EFFECT_GRID_OPTIMISATION
  glEnable(GL_DEPTH_TEST);
  SetupProjMatrixFor3D();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
#else
  SetupMatricesFor2D(); glDisable(GL_DEPTH_TEST);
#endif

  // Render debug information showing which cells are "dirty"
  if (false)
  {
    glColor4f(1.0f, 0.0f, 1.0f, 0.5f);
    PaintPixels();
  }

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_pixelEffectTexId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  // Additive blocky
  START_PROFILE(g_app->m_profiler, "pass 1");
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
  PaintPixels();
  END_PROFILE(g_app->m_profiler, "pass 1");

  // Subtractive smooth
  START_PROFILE(g_app->m_profiler, "pass 2");
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);
  glColor4f(1.0f, 1.0f, 1.0f, 0.0f);
  PaintPixels();
  END_PROFILE(g_app->m_profiler, "pass 2");

  // Subtractive smooth
  START_PROFILE(g_app->m_profiler, "pass 3");
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);
  glColor4f(1.0f, 1.0f, 1.0f, 0.2f);
  PaintPixels();
  END_PROFILE(g_app->m_profiler, "pass 3");

  // Additive smooth
  START_PROFILE(g_app->m_profiler, "pass 4");
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
  PaintPixels();
  END_PROFILE(g_app->m_profiler, "pass 4");

  glDisable(GL_TEXTURE_2D);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glEnable(GL_DEPTH_TEST); //FIXME
  glDepthMask(true);
  glEnable(GL_CULL_FACE);
  glDisable(GL_BLEND);

  if (false)
  {
    g_editorFont.BeginText2D();
    double aspectRatio = static_cast<double>(m_screenW) / static_cast<double>(m_screenH);
    const int yCellsUsed = static_cast<int>((double)PIXEL_EFFECT_GRID_RES / aspectRatio);
    const float scaleX = m_screenW / static_cast<float>(PIXEL_EFFECT_GRID_RES);
    const float scaleY = m_screenH / yCellsUsed;
    const float offsetX = scaleX / 2.0f;
    const float offsetY = scaleY / 2.0f;
    for (int y = 0; y < yCellsUsed; ++y)
    {
      for (int x = 0; x < PIXEL_EFFECT_GRID_RES; ++x)
      {
        int blah = yCellsUsed - y - 1;
        const float dist = m_pixelEffectGrid[x][blah];
        if (dist < 1e9)
        {
          g_editorFont.DrawText2DCentre(static_cast<float>(x) * scaleX + offsetX, static_cast<float>(y) * scaleY + offsetY, 12, "%.0f",
                                        dist);
        }
      }
    }
    g_editorFont.EndText2D();
  }

  CHECK_OPENGL_STATE();

  END_PROFILE(g_app->m_profiler, "Pixel Apply");
}

void Renderer::UpdateTotalMatrix()
{
  double m[16];
  double p[16];
  glGetDoublev(GL_MODELVIEW_MATRIX, m);
  glGetDoublev(GL_PROJECTION_MATRIX, p);

  DEBUG_ASSERT(m[3] == 0.0);
  DEBUG_ASSERT(m[7] == 0.0);
  DEBUG_ASSERT(m[11] == 0.0);
  DEBUG_ASSERT(NearlyEquals(m[15], 1.0));

  DEBUG_ASSERT(p[1] == 0.0);
  DEBUG_ASSERT(p[2] == 0.0);
  DEBUG_ASSERT(p[3] == 0.0);
  DEBUG_ASSERT(p[4] == 0.0);
  DEBUG_ASSERT(p[6] == 0.0);
  DEBUG_ASSERT(p[7] == 0.0);
  DEBUG_ASSERT(p[8] == 0.0);
  DEBUG_ASSERT(p[9] == 0.0);
  DEBUG_ASSERT(p[12] == 0.0);
  DEBUG_ASSERT(p[13] == 0.0);
  DEBUG_ASSERT(p[15] == 0.0);

  m_totalMatrix[0] = m[0] * p[0] + m[1] * p[4] + m[2] * p[8] + m[3] * p[12];
  m_totalMatrix[1] = m[0] * p[1] + m[1] * p[5] + m[2] * p[9] + m[3] * p[13];
  m_totalMatrix[2] = m[0] * p[2] + m[1] * p[6] + m[2] * p[10] + m[3] * p[14];
  m_totalMatrix[3] = m[0] * p[3] + m[1] * p[7] + m[2] * p[11] + m[3] * p[15];

  m_totalMatrix[4] = m[4] * p[0] + m[5] * p[4] + m[6] * p[8] + m[7] * p[12];
  m_totalMatrix[5] = m[4] * p[1] + m[5] * p[5] + m[6] * p[9] + m[7] * p[13];
  m_totalMatrix[6] = m[4] * p[2] + m[5] * p[6] + m[6] * p[10] + m[7] * p[14];
  m_totalMatrix[7] = m[4] * p[3] + m[5] * p[7] + m[6] * p[11] + m[7] * p[15];

  m_totalMatrix[8] = m[8] * p[0] + m[9] * p[4] + m[10] * p[8] + m[11] * p[12];
  m_totalMatrix[9] = m[8] * p[1] + m[9] * p[5] + m[10] * p[9] + m[11] * p[13];
  m_totalMatrix[10] = m[8] * p[2] + m[9] * p[6] + m[10] * p[10] + m[11] * p[14];
  m_totalMatrix[11] = m[8] * p[3] + m[9] * p[7] + m[10] * p[11] + m[11] * p[15];

  m_totalMatrix[12] = m[12] * p[0] + m[13] * p[4] + m[14] * p[8] + m[15] * p[12];
  m_totalMatrix[13] = m[12] * p[1] + m[13] * p[5] + m[14] * p[9] + m[15] * p[13];
  m_totalMatrix[14] = m[12] * p[2] + m[13] * p[6] + m[14] * p[10] + m[15] * p[14];
  m_totalMatrix[15] = m[12] * p[3] + m[13] * p[7] + m[14] * p[11] + m[15] * p[15];
}

void Renderer::Get2DScreenPos(const Vector3& v, Vector3* _out)
{
  double out[4];

#define m m_totalMatrix
  out[0] = v.x * m[0] + v.y * m[4] + v.z * m[8] + m[12];
  out[1] = v.x * m[1] + v.y * m[5] + v.z * m[9] + m[13];
  out[2] = v.x * m[2] + v.y * m[6] + v.z * m[10] + m[14];
  out[3] = v.x * m[3] + v.y * m[7] + v.z * m[11] + m[15];
#undef m

  if (out[3] <= 0.0f)
    return;

  double multiplier = 0.5f / out[3];
  out[0] *= multiplier;
  out[1] *= multiplier;

  // Map x, y and z to range 0-1
  out[0] += 0.5;
  out[1] += 0.5;

  // Map x, y to viewport
  _out->x = out[0] * m_screenW;
  _out->y = out[1] * m_screenH;
  _out->z = out[3];
}

const double* Renderer::GetTotalMatrix() { return m_totalMatrix; }

void Renderer::RasteriseSphere(const Vector3& _pos, float _radius)
{
  const float screenToGridFactor = static_cast<float>(PIXEL_EFFECT_GRID_RES) / static_cast<float>(m_screenW);
  Camera* cam = TheCamera();
  Vector3 centre;
  Vector3 topLeft;
  Vector3 bottomRight;
  const Vector3 camUpRight = (cam->GetRight() + cam->GetUp()) * _radius;
  Get2DScreenPos(_pos, &centre);
  Get2DScreenPos(_pos + camUpRight, &topLeft);
  Get2DScreenPos(_pos - camUpRight, &bottomRight);

  int x1 = floorf(topLeft.x * screenToGridFactor);
  int x2 = ceilf(bottomRight.x * screenToGridFactor);
  int y1 = floorf(bottomRight.y * screenToGridFactor);
  int y2 = ceilf(topLeft.y * screenToGridFactor);

  ClampInPlace(x1, 0, PIXEL_EFFECT_GRID_RES);
  ClampInPlace(x2, 0, PIXEL_EFFECT_GRID_RES);
  ClampInPlace(y1, 0, PIXEL_EFFECT_GRID_RES);
  ClampInPlace(y2, 0, PIXEL_EFFECT_GRID_RES);

  const float nearestZ = centre.z - _radius;

  for (int y = y1; y < y2; ++y)
  {
    for (int x = x1; x < x2; ++x)
    {
      if (nearestZ < m_pixelEffectGrid[x][y])
        m_pixelEffectGrid[x][y] = nearestZ;
    }
  }
}

void Renderer::MarkUsedCells(const ShapeFragment* _frag, const Matrix34& _transform)
{
#if USE_PIXEL_EFFECT_GRID_OPTIMISATION
  Matrix34 total = _frag->m_transform * _transform;
  Vector3 worldPos = _frag->m_centre * total;

  // Return early if this shape fragment isn't on the screen
  {
    if (!TheCamera()->SphereInViewFrustum(worldPos, _frag->m_radius))
      return;
  }

  if (_frag->m_radius > 0.0f)
    RasteriseSphere(worldPos, _frag->m_radius);

  // Recurse into all child fragments
  int numChildren = static_cast<int>(_frag->m_childFragments.size());
  for (int i = 0; i < numChildren; ++i)
  {
    const ShapeFragment* child = _frag->m_childFragments[i];
    MarkUsedCells(child, total);
  }
#endif // USE_PIXEL_EFFECT_GRID_OPTIMISATION
}

void Renderer::MarkUsedCells(const Shape* _shape, const Matrix34& _transform)
{
  START_PROFILE(g_app->m_profiler, "MarkUsedCells");
  MarkUsedCells(_shape->m_rootFragment, _transform);
  END_PROFILE(g_app->m_profiler, "MarkUsedCells");
}
