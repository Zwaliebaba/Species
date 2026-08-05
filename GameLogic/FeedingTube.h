#pragma once

#include <vector>

class FeedingTube : public Building
{
  protected:
    ShapeMarker* m_focusMarker;

    int m_receiverId;
    float m_range;
    float m_signal;

    DirectX::XMFLOAT3 GetDishPos(float _predictionTime);   // Returns the position of the transmission point
    DirectX::XMFLOAT3 GetDishFront(float _predictionTime); // Returns the front vector of the dish
    DirectX::XMFLOAT3 GetForwardsClippingDir(float _predictionTime,
                                             FeedingTube* _sender); // Returns a good clipping direction for signal

    void RenderSignal(float _predictionTime, float _radius, float _alpha);


  public:
    FeedingTube();

    void Initialise(Building* _template);
    void Read(TextReader* _in, bool _dynamic);
    void Write(FileWriter* _out);

    bool Advance();
    void Render(float _predictionTime);
    void RenderAlphas(float _predictionTime);

    DirectX::XMFLOAT3 GetStartPoint();
    DirectX::XMFLOAT3 GetEndPoint();

    int GetBuildingLink();
    void SetBuildingLink(int _buildingId);

    bool DoesSphereHit(DirectX::XMFLOAT3 const& _pos, float _radius);

    void ListSoundEvents(std::vector<const char*>* _list);
    bool IsInView();
};
