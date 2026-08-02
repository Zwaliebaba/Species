#include "pch.h"

#include "TeamControls.h"

TeamControls::TeamControls() { Clear(); }

unsigned short TeamControls::GetFlags() const
{
  return (m_unitMove ? 0x0001 : 0) | (m_directUnitMove ? 0x0002 : 0) | (m_primaryFireTarget ? 0x0004 : 0) | (m_secondaryFireTarget ? 0x0008 : 0) |
         (m_primaryFireDirected ? 0x0010 : 0) | (m_secondaryFireDirected ? 0x0020 : 0) | (m_cameraEntityTracking ? 0x0040 : 0) |
         (m_unitSecondaryMode ? 0x0080 : 0) | (m_endSetTarget ? 0x0100 : 0);
}
void TeamControls::SetFlags(unsigned short _flags)
{
  m_unitMove = _flags & 0x0001 ? 1 : 0;
  m_directUnitMove = _flags & 0x0002 ? 1 : 0;
  m_primaryFireTarget = _flags & 0x0004 ? 1 : 0;
  m_secondaryFireTarget = _flags & 0x0008 ? 1 : 0;
  m_primaryFireDirected = _flags & 0x0010 ? 1 : 0;
  m_secondaryFireDirected = _flags & 0x0020 ? 1 : 0;
  m_cameraEntityTracking = _flags & 0x0040 ? 1 : 0;
  m_unitSecondaryMode = _flags & 0x0080 ? 1 : 0;
  m_endSetTarget = _flags & 0x0100 ? 1 : 0;
}
void TeamControls::Clear() { memset(this, 0, sizeof(*this)); }
void TeamControls::ClearFlags()
{
  m_unitMove = 0;
  m_directUnitMove = 0;
  m_primaryFireTarget = 0;
  m_secondaryFireTarget = 0;
  m_primaryFireDirected = 0;
  m_secondaryFireDirected = 0;
  m_cameraEntityTracking = 0;
  m_unitSecondaryMode = 0;
  m_endSetTarget = 0;
}
