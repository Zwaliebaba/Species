#pragma once

#ifdef DEBUG_RENDER_ENABLED

#include "SlotMap.h"
#include "NeuronMath.h"


//*****************************************************************************
// Class
//*****************************************************************************


namespace Neuron
{
  class PersistRenderItem
  {
    public:
      DirectX::XMFLOAT3 m_vect1, m_vect2;
      float m_size1, m_size2, m_size3;
      unsigned int m_life;
      unsigned int m_type;
      char m_label[64];
  };


  //*****************************************************************************
  // Class PersistingDebugRenderer
  //*****************************************************************************

  class PersistingDebugRenderer
  {
    protected:
      enum
      {
        TypeSquare2d,
        TypePointMarker,
        TypeSphere,
        TypeVector,
        TypeNumTypes
      };

      Neuron::SlotMap<PersistRenderItem, Neuron::SlotReuse::MostRecentFirst> m_items;

      PersistRenderItem* FindItem(char const* _label);

      // The half every one of the four shared: find the item with this label or
      // take a fresh slot, stamp the common fields, then let the caller fill in
      // the shape-specific ones. It was copied out four times before, which is how
      // Sphere and Vector came to skip the label copy their own FindItem depends
      // on — see the note in the .cpp.
      PersistRenderItem& Record(unsigned int _type, unsigned int _life, std::string const& _label);

      template <typename Fill> void Record(unsigned int _type, unsigned int _life, std::string const& _label, Fill _fill)
      {
        _fill(Record(_type, _life, _label));
      }

    public:
      PersistingDebugRenderer();

      // std::format rather than printf varargs. The old signatures took
      // `char *_fmt, ...` and formatted into a char[512] with no bound, then
      // truncated to 63 characters — an unbounded write into a buffer eight times
      // larger than the field it fed. Nothing in the tree calls any of these four; only
      // Render() is reached, from Species/Renderer.cpp. That is what made
      // changing the signatures free, and it is worth knowing before writing the
      // first caller.
      template <typename... Args> void Square2d(float x, float y, float _size, unsigned int _life, std::format_string<Args...> _fmt, Args&&... _args)
      {
        Record(TypeSquare2d, _life, std::format(_fmt, std::forward<Args>(_args)...),
               [&](PersistRenderItem& _item)
               {
                 _item.m_vect1.x = x;
                 _item.m_vect1.y = y;
                 _item.m_size1 = _size;
               });
      }

      template <typename... Args>
      void PointMarker(DirectX::XMFLOAT3 const& _point, unsigned int _life, std::format_string<Args...> _fmt, Args&&... _args)
      {
        Record(TypePointMarker, _life, std::format(_fmt, std::forward<Args>(_args)...), [&](PersistRenderItem& _item) { _item.m_vect1 = _point; });
      }

      template <typename... Args>
      void Sphere(DirectX::XMFLOAT3 const& _centre, float _radius, int _life, std::format_string<Args...> _fmt, Args&&... _args)
      {
        Record(TypeSphere, _life, std::format(_fmt, std::forward<Args>(_args)...),
               [&](PersistRenderItem& _item)
               {
                 _item.m_vect1 = _centre;
                 _item.m_size1 = _radius;
               });
      }

      template <typename... Args>
      void Vector(DirectX::XMFLOAT3 const& _start, DirectX::XMFLOAT3 const& _end, int _life, std::format_string<Args...> _fmt, Args&&... _args)
      {
        Record(TypeVector, _life, std::format(_fmt, std::forward<Args>(_args)...),
               [&](PersistRenderItem& _item)
               {
                 _item.m_vect1 = _start;
                 _item.m_vect2 = _end;
               });
      }

      void Render();
  };


  extern PersistingDebugRenderer g_debugRenderer;

#endif // ifdef DEBUG_RENDER_ENABLED
} // namespace Neuron
