#pragma once

#include "Teleport.h"

#define BRIDGE_TRANSPORTPERIOD 0.1f
#define BRIDGE_TRANSPORTSPEED 50.0f


class Bridge : public Teleport
{
  public:
    enum
    {
      BridgeTypeEnd,
      BridgeTypeTower,
      NumBridgeTypes
    };
    int m_bridgeType;
    int m_nextBridgeId;
    float m_status; // Construction status, 0=not started, 100=finished, < 0.0f = shutdown

  protected:
    Shape* m_shapes[NumBridgeTypes];
    ShapeMarker* m_signal;

    bool m_beingOperated;

  public:
    Bridge();

    void Initialise(Building* _template);
    void SetBridgeType(int _type);

    void Render(float predictionTime);
    void RenderAlphas(float predictionTime);
    bool Advance();

    bool GetAvailablePosition(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front); // Finds place for engineer
    void BeginOperation();
    void EndOperation();

    bool ReadyToSend();
    DirectX::XMFLOAT3 GetStartPoint();
    DirectX::XMFLOAT3 GetEndPoint();
    bool GetExit(DirectX::XMFLOAT3& _pos, DirectX::XMFLOAT3& _front);

    bool UpdateEntityInTransit(Entity* _entity);

    int GetBuildingLink();
    void SetBuildingLink(int _buildingId);

    void Read(TextReader* _in, bool _dynamic);
    void Write(FileWriter* _out);
};
