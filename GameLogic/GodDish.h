
#pragma once

#include <vector>

#include "Building.h"


class GodDish : public Building
{
  public:
    bool m_activated;
    float m_timer;
    int m_numSpawned;
    bool m_spawnSpam;

  public:
    GodDish();

    void Initialise(Building* _template);

    bool Advance();
    void Render(float _predictionTime);
    void RenderAlphas(float _predictionTime);

    bool IsInView();

    void Activate();
    void DeActivate();
    void SpawnSpam(bool _isResearch);

    void TriggerSpam();

    void ListSoundEvents(std::vector<const char*>* _list);
};
