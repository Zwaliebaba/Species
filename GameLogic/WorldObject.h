#pragma once

#include "NeuronMath.h"
#include "RgbColour.h"
#include "WorldObjectId.h"

// TRANSITIONAL, and deliberately kept after T14 converted this header's
// storage. Forty-five files below still read m_pos and m_vel through AsLegacy
// while they wait for their own conversion task, and every one of them reaches
// this header. Dropping the include here would break them all at once — which
// is failure mode 3 on T10's list, the one no checker sees. T25 removes it.


// ****************************************************************************
//  Class WorldObject
// ****************************************************************************


namespace Species
{
  class WorldObject
  {
    public:
      enum // These enums apply if our unitID is UNIT_EFFECTS
      {
        TypeInvalid,
        EffectThrowableGrenade,
        EffectThrowableAirstrikeMarker,
        EffectThrowableAirstrikeBomb,
        EffectThrowableControllerGrenade,
        EffectGunTurretTarget,
        EffectGunTurretShell,
        EffectSpamInfection,
        EffectBoxKite,
        EffectSnow,
        EffectRocket,
        EffectShockwave,
        EffectMuzzleFlash,
        EffectOfficerOrders,
        EffectZombie
      };

    public:
      WorldObjectId m_id;
      int m_type;
      // ZERO-INITIALISED DELIBERATELY, and this is the one that would have been
      // invisible. Vector3's default constructor zeroed; XMFLOAT3's does not, and
      // WorldObject's constructor never assigned either of these. Every entity,
      // building and effect in the game derives from this class, so an
      // uninitialised m_pos here is garbage coordinates for the whole world —
      // caught by neither the compiler nor CI. See T10's failure mode 5.
      DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
      DirectX::XMFLOAT3 m_vel{0.0f, 0.0f, 0.0f};
      bool m_onGround;
      bool m_enabled;

      WorldObject();
      virtual ~WorldObject();
      void BounceOffLandscape();

      virtual bool Advance();
      virtual void Render(float _time);
      virtual bool RenderPixelEffect(float predictionTime); // Return true if you did anything
  };


  // ****************************************************************************
  //  Class Light
  // ****************************************************************************

  class Light
  {
    public:
      float m_colour[4]; // Forth element seems irrelevant but OpenGL insists we specify it
      float m_front[4];  // Forth element must be 0.0f to signify an infinitely distance light

      Light();
      void SetColour(float colour[4]);
      void SetFront(float front[4]);
      void SetFront(DirectX::XMFLOAT3 front);
      void Normalise();
  };
} // namespace Species
