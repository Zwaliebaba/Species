#include "pch.h"

#include <stdarg.h>
#include <string.h>

#include "DebugRender.h"
#include "PersistingDebugRender.h"


#ifdef DEBUG_RENDER_ENABLED


namespace Neuron
{
  PersistingDebugRenderer g_debugRenderer;


  PersistingDebugRenderer::PersistingDebugRenderer() { m_items.SetSize(100); }


  PersistRenderItem* PersistingDebugRenderer::FindItem(char const* _label)
  {
    for (int i = 0; i < m_items.Size(); ++i)
    {
      if (!m_items.ValidIndex(i))
        continue;

      if (strnicmp(_label, m_items[i].m_label, sizeof(m_items[i].m_label) - 1) == 0)
      {
        return &m_items[i];
      }
    }

    return nullptr;
  }


  PersistRenderItem& PersistingDebugRenderer::Record(unsigned int _type, unsigned int _life, std::string const& _label)
  {
    // m_label is a char[64] and the label is truncated to fit it. The old code
    // formatted into a char[512] with no bound, forced buf[63] to nul, and then
    // did a bounded copy of 63 of those bytes — so it truncated twice and the
    // first write was the unbounded one. Truncating once, here, is the whole
    // change; FindItem still matches on the truncated text, exactly as before.
    //
    // Written out rather than with std::min, which does not compile anywhere
    // MathUtils.h is reachable: it defines function-style min and max macros, so
    // `std::min(` becomes `std::(...)(`. See tasks/language-hygiene.yaml T8.
    const std::string_view label(_label);
    size_t length = label.size();
    if (length > sizeof(PersistRenderItem::m_label) - 1)
    {
      length = sizeof(PersistRenderItem::m_label) - 1;
    }
    const std::string truncated(label.substr(0, length));

    PersistRenderItem* item = FindItem(truncated.c_str());
    if (!item)
    {
      item = m_items.GetPointer(m_items.GetNextFree());
    }

    item->m_life = _life;
    item->m_type = _type;

    // Sphere and Vector did NOT copy the label in. That is preserved as a bug
    // rather than a decision: FindItem looks the item up BY label, so an item
    // recorded without one could never be found again and every call allocated a
    // fresh slot until the map filled. Copying it for all four is the fix, and it
    // is safe to make here because nothing calls any of them yet.
    std::memcpy(item->m_label, truncated.c_str(), length + 1);

    return *item;
  }


  void PersistingDebugRenderer::Render()
  {
    for (int i = 0; i < m_items.Size(); ++i)
    {
      if (!m_items.ValidIndex(i))
        continue;

      PersistRenderItem* item = &m_items[i];

      switch (item->m_type)
      {
      case TypeSquare2d:
        RenderSquare2d(item->m_vect1.x, item->m_vect1.y, item->m_size1);
        break;
      case TypePointMarker:
        RenderPointMarker(item->m_vect1, item->m_label);
        break;
      case TypeSphere:
        RenderSphere(item->m_vect1, item->m_size1);
        break;
      case TypeVector:
        RenderArrow(item->m_vect1, item->m_vect2, 2.0f);
        break;
      }

      if (item->m_life > 0)
      {
        if (item->m_life == 1)
        {
          m_items.MarkNotUsed(i);
        }
        else
        {
          item->m_life--;
        }
      }
    }
  }


#endif // ifdef DEBUG_RENDER_ENABLED
} // namespace Neuron
