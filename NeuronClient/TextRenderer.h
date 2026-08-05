#pragma once

#include <format>
#include <string>
#include <string_view>

#include "NeuronMath.h"


#define DEF_FONT_SIZE 12.0f


// ****************************************************************************
//  Class TextRenderer
// ****************************************************************************


namespace Neuron
{
  class RGBAColour;

  // EVERY ENTRY POINT COMES IN TWO SHAPES, and which one a call gets is decided
  // by whether it passes anything after the text. strings-modernised T12 made
  // that split; before it there was one variadic per entry point, each of which
  // formatted through the C library into a char[512] with no bound.
  //
  //   DrawText2D(x, y, size, text)          the PLAIN overload. `text` is drawn
  //                                         as it stands, and nothing in it is
  //                                         interpreted.
  //   DrawText2D(x, y, size, "n = {}", n)   the FORMATTING template. The format
  //                                         string is checked against the
  //                                         argument types at COMPILE time.
  //
  // THE TEMPLATE REQUIRES AT LEAST ONE ARGUMENT, and that is what keeps the two
  // unambiguous: with only the text, the template is not viable and the plain
  // overload takes the call. It is also the right split for this tree — 128 of
  // the 199 call sites pass a RUNTIME string with nothing after it, and
  // std::format_string is consteval, so one formatting template could not have
  // served any of them.
  //
  // The plain overload is a FIX as well as a translation. Those 128 sites used
  // to hand a caption, a level-file name or a translated phrase to the C
  // formatter AS ITS FORMAT STRING, so a percent sign anywhere in that data was
  // undefined behaviour. Now it is a percent sign.
  //
  // One hazard this does NOT remove: a std::string_view cannot be built from a
  // null pointer, and the C formatter could not read one either. A caller
  // that may pass null — EclButton::m_caption starts null — has to check it,
  // exactly as it had to before.
  class TextRenderer
  {
    protected:
      double m_projectionMatrix[16];
      double m_modelviewMatrix[16];
      char* m_filename;
      unsigned int m_textureID;
      unsigned int m_bitmapWidth;
      unsigned int m_bitmapHeight;
      bool m_renderShadow;
      bool m_renderOutline;

      float GetTexCoordX(unsigned char theChar);
      float GetTexCoordY(unsigned char theChar);

    public:
      void Initialise(char const* _filename);

      void BuildOpenGlState();

      void BeginText2D();
      void EndText2D();

      void SetRenderShadow(bool _renderShadow);
      void SetRenderOutline(bool _renderOutline);

      void DrawText2DSimple(float _x, float _y, float _size, std::string_view _text);
      void DrawText2D(float _x, float _y, float _size, std::string_view _text);
      void DrawText2DRight(float _x, float _y, float _size, std::string_view _text);  // Right justified
      void DrawText2DCentre(float _x, float _y, float _size, std::string_view _text); // Centre justified
      void DrawText2DJustified(float _x, float _y, float _size, int _xJustification, std::string_view _text);
      void DrawText2DUp(float _x, float _y, float _size, std::string_view _text);   // Rotated 90 ccw
      void DrawText2DDown(float _x, float _y, float _size, std::string_view _text); // Rotated 90 cw


      void DrawText3DSimple(DirectX::XMFLOAT3 const& _pos, float _size, std::string_view _text);
      void DrawText3D(DirectX::XMFLOAT3 const& _pos, float _size, std::string_view _text);
      void DrawText3DCentre(DirectX::XMFLOAT3 const& _pos, float _size, std::string_view _text);
      void DrawText3DRight(DirectX::XMFLOAT3 const& _pos, float _size, std::string_view _text);

      void DrawText3D(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up, float _size,
                      std::string_view _text);


      template <typename T, typename... Args>
      void DrawText2D(float _x, float _y, float _size, std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args)
      {
        std::string const text = std::format(_fmt, std::forward<T>(_arg), std::forward<Args>(_args)...);
        DrawText2D(_x, _y, _size, std::string_view(text));
      }

      template <typename T, typename... Args>
      void DrawText2DRight(float _x, float _y, float _size, std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args)
      {
        std::string const text = std::format(_fmt, std::forward<T>(_arg), std::forward<Args>(_args)...);
        DrawText2DRight(_x, _y, _size, std::string_view(text));
      }

      template <typename T, typename... Args>
      void DrawText2DCentre(float _x, float _y, float _size, std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args)
      {
        std::string const text = std::format(_fmt, std::forward<T>(_arg), std::forward<Args>(_args)...);
        DrawText2DCentre(_x, _y, _size, std::string_view(text));
      }

      template <typename T, typename... Args>
      void DrawText2DJustified(float _x, float _y, float _size, int _xJustification, std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args)
      {
        std::string const text = std::format(_fmt, std::forward<T>(_arg), std::forward<Args>(_args)...);
        DrawText2DJustified(_x, _y, _size, _xJustification, std::string_view(text));
      }

      template <typename T, typename... Args>
      void DrawText2DUp(float _x, float _y, float _size, std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args)
      {
        std::string const text = std::format(_fmt, std::forward<T>(_arg), std::forward<Args>(_args)...);
        DrawText2DUp(_x, _y, _size, std::string_view(text));
      }

      template <typename T, typename... Args>
      void DrawText2DDown(float _x, float _y, float _size, std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args)
      {
        std::string const text = std::format(_fmt, std::forward<T>(_arg), std::forward<Args>(_args)...);
        DrawText2DDown(_x, _y, _size, std::string_view(text));
      }

      template <typename T, typename... Args>
      void DrawText3D(DirectX::XMFLOAT3 const& _pos, float _size, std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args)
      {
        std::string const text = std::format(_fmt, std::forward<T>(_arg), std::forward<Args>(_args)...);
        DrawText3D(_pos, _size, std::string_view(text));
      }

      template <typename T, typename... Args>
      void DrawText3DCentre(DirectX::XMFLOAT3 const& _pos, float _size, std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args)
      {
        std::string const text = std::format(_fmt, std::forward<T>(_arg), std::forward<Args>(_args)...);
        DrawText3DCentre(_pos, _size, std::string_view(text));
      }

      template <typename T, typename... Args>
      void DrawText3DRight(DirectX::XMFLOAT3 const& _pos, float _size, std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args)
      {
        std::string const text = std::format(_fmt, std::forward<T>(_arg), std::forward<Args>(_args)...);
        DrawText3DRight(_pos, _size, std::string_view(text));
      }

      template <typename T, typename... Args>
      void DrawText3D(DirectX::XMFLOAT3 const& _pos, DirectX::XMFLOAT3 const& _front, DirectX::XMFLOAT3 const& _up, float _size,
                      std::format_string<T, Args...> _fmt, T&& _arg, Args&&... _args)
      {
        std::string const text = std::format(_fmt, std::forward<T>(_arg), std::forward<Args>(_args)...);
        DrawText3D(_pos, _front, _up, _size, std::string_view(text));
      }

      float GetTextWidth(unsigned int _numChars, float _size = 13.0f);
  };


  extern TextRenderer g_gameFont;
  extern TextRenderer g_editorFont;
} // namespace Neuron
