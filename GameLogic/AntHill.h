
#pragma once

#include <vector>

#include "Building.h"

#define ANTHILL_SEARCHRANGE 400.0f


class FileWriter;


struct AntObjective
{
    // Braced to zero: Vector3's default constructor did it, XMFLOAT3's does not.
    DirectX::XMFLOAT3 m_pos{0.0f, 0.0f, 0.0f};
    WorldObjectId m_targetId;
    int m_numToSend;
};


class AntHill : public Building
{
  protected:
    std::vector<AntObjective*> m_objectives;

    float m_objectiveTimer;
    float m_spawnTimer;
    float m_eggConvertTimer;
    int m_health;
    int m_unitId;
    int m_populationLock;
    bool m_renderDamaged;

  protected:
    bool SearchingArea(DirectX::XMFLOAT3 _pos);
    bool TargettedEntity(WorldObjectId _id);

    bool SearchForSpirits(DirectX::XMFLOAT3& _pos);
    bool SearchForCitizens(DirectX::XMFLOAT3& _pos, WorldObjectId& _id);
    bool SearchForEnemies(DirectX::XMFLOAT3& _pos, WorldObjectId& _id);

    bool SearchForScoutArea(DirectX::XMFLOAT3& _pos);

    bool PopulationLocked();

  public:
    int m_numAntsInside;
    int m_numSpiritsInside;

  public:
    AntHill();

    void Initialise(Building* _template);

    bool Advance();
    void Render(float _predictionTime);
    void Damage(float _damage);
    void Destroy(float _intensity);

    void Read(TextReader* _in, bool _dynamic);
    void Write(FileWriter* _out);

    void ListSoundEvents(std::vector<const char*>* _list);
};
