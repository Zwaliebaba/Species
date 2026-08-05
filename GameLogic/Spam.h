
#pragma once

#include <vector>

#include "Building.h"

#define SPAM_RELOADTIME 60.0f
#define SPAM_DAMAGE 100.0f


namespace Species
{
  class Spam : public Building
  {
    protected:
      float m_timer;
      float m_damage;

      bool m_research;
      bool m_onGround;
      bool m_activated;

    public:
      Spam();

      void Initialise(Building* _template);
      void SetDetail(int _detail);
      void Damage(float _damage);
      void Destroy(float _intensity);

      void Render(float _predictionTime);
      void RenderAlphas(float _predictionTime);

      bool Advance();

      void ListSoundEvents(std::vector<const char*>* _list);

      void SetAsResearch();
      void SendFromHeaven();
      void SpawnInfection();
  };


  // ****************************************************************************
  //  Class SpamInfection
  // ****************************************************************************

#define SPAMINFECTION_MINSEARCHRANGE 100.0f
#define SPAMINFECTION_MAXSEARCHRANGE 200.0f
#define SPAMINFECTION_LIFE 10.0f
#define SPAMINFECTION_TAILLENGTH 30

class SpamInfection : public WorldObject
{
  protected:
    enum
    {
      StateIdle,
      StateAttackingEntity,
      StateAttackingSpirit
    };

    int m_state;
    float m_retargetTimer;
    WorldObjectId m_targetId;
    int m_spiritId;
    DirectX::XMFLOAT3 m_targetPos{0.0f, 0.0f, 0.0f};
    float m_life;

    // Values, not pointers — Virii and Engineer both have a m_positionHistory
    // and neither holds what this one does, which is why check_math_types
    // skips the name.
    std::vector<DirectX::XMFLOAT3> m_positionHistory;

  protected:
    void AdvanceIdle();
    void AdvanceAttackingEntity();
    void AdvanceAttackingSpirit();
    bool AdvanceToTargetPosition();

    bool SearchForEntities();
    bool SearchForRandomPosition();
    bool SearchForSpirits();

  public:
    int m_parentId; // id of Spam that spawned me

  public:
    SpamInfection();

    bool Advance();
    void Render(float _time);
};
} // namespace Species
