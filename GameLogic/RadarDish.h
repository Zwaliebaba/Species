#pragma once

#include <vector>

#define RADARDISH_TRANSPORTPERIOD 0.1f // Minimum wait time between sends
#define RADARDISH_TRANSPORTSPEED 50.0f // Speed of in-transit entities (m/s)

#include "Teleport.h"


class RadarDish : public Teleport
{
  protected:
    ShapeFragment* m_dish;
    ShapeFragment* m_upperMount;
    ShapeMarker* m_focusMarker;

    // Braced to zero: Vector3's default constructor did it and XMFLOAT3's does
    // not. RadarDish's constructor zeroes m_target explicitly but leaves the
    // two entrance vectors to the default, so they need the braces.
    DirectX::XMFLOAT3 m_entrancePos{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_entranceFront{0.0f, 0.0f, 0.0f};

    DirectX::XMFLOAT3 m_target{0.0f, 0.0f, 0.0f};
    int m_receiverId;
    float m_range;
    float m_signal;

    bool m_newlyCreated;

    bool m_horizontallyAligned;
    bool m_verticallyAligned;
    bool m_movementSoundsPlaying;

    DirectX::XMFLOAT3 GetDishPos(float _predictionTime);   // Returns the position of the transmission point
    DirectX::XMFLOAT3 GetDishFront(float _predictionTime); // Returns the front vector of the dish

    void RenderSignal(float _predictionTime, float _radius, float _alpha);

  public:
    RadarDish();
    ~RadarDish();

    void SetDetail(int _detail);

    bool Advance();
    void Render(float _predictionTime);
    void RenderAlphas(float _predictionTime);

    void Aim(DirectX::XMFLOAT3 _worldPos);

    bool Connected();
    bool ReadyToSend();

    int GetConnectedDishId();

    // DELIBERATELY STILL LEGACY -- these four override Teleport's virtuals,
    // and a virtual override that stops matching its base silently stops
    // overriding rather than failing to compile (the rule T12 learned the
    // hard way). Teleport and its other deriver Bridge belong to T17, so the
    // whole family converts there, in one commit, exactly as Building's
    // fifteen overriders converted with Building here.
    Vector3 GetStartPoint();
    Vector3 GetEndPoint();
    bool GetEntrance(Vector3& _pos, Vector3& _front);
    bool GetExit(Vector3& _pos, Vector3& _front);

    bool DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius);

    bool UpdateEntityInTransit(Entity* _entity);

    void ListSoundEvents(std::vector<const char*>* _list);
};
