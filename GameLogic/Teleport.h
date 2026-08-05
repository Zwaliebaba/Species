#pragma once

#include "NeuronMath.h"


#include "Building.h"


namespace Species
{
  class Entity;

  struct TeleportMap
  {
      int m_teamId;
      int m_fromUnitId;
      int m_toUnitId;
  };


  class Teleport : public Building
  {
    protected:
      float m_timeSync;
      float m_sendPeriod;                            // Minimum time between sends
      static std::vector<TeleportMap> m_teleportMap; // Maps units going in onto units comingout

      ShapeMarker* m_entrance;

    protected:
      void RenderSpirit(DirectX::XMFLOAT3 const& _pos, int _teamId);

    public:
      std::vector<WorldObjectId> m_inTransit; // Entities on the move

    public:
      Teleport();

      void SetShape(Shape* _shape);

      bool Advance();
      void RenderAlphas(float predictionTime);

      void EnterTeleport(WorldObjectId _id, bool _relay = false); // Relay=true means i've entered directly from another teleport

      bool IsInView();

      virtual bool Connected();
      virtual bool ReadyToSend();

      // These four are overridden by Bridge below and by RadarDish under T16,
      // which left its copies legacy on purpose and waited for this commit. A
      // virtual override matches its base exactly or silently stops overriding,
      // so all three classes move together or none of them do.
      virtual bool GetEntrance(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front);
      virtual bool GetExit(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front);

      virtual DirectX::XMFLOAT3 GetStartPoint();
      virtual DirectX::XMFLOAT3 GetEndPoint();

      virtual bool UpdateEntityInTransit(Entity* _entity); // Returns true (remove me) or false (still inside)
  };
} // namespace Species
