#ifndef _included_incubator_h
#define _included_incubator_h

#include "FastDArray.h"
#include "Building.h"
#include "Spirit.h"

class ShapeMarker;

#define INCUBATOR_PROCESSTIME       5.0f

struct IncubatorIncoming
{
  Vector3 m_pos;
  int m_entrance;
  float m_alpha;
};

class Incubator : public Building
{
  protected:
    FastDArray<Spirit> m_spirits;
    ShapeMarker* m_spiritCentre;
    ShapeMarker* m_exit;
    ShapeMarker* m_dock;
    ShapeMarker* m_spiritEntrance[3];

    int m_troopType;
    float m_timer;

    LList<IncubatorIncoming*> m_incoming;

  public:
    int m_numStartingSpirits;

    Incubator();
    ~Incubator() override;

    void Initialise(Building* _template) override;

    bool Advance() override;
    void SpawnEntity();
    void AddSpirit(Spirit* _spirit);

    void Render(float _predictionTime) override;
    void RenderAlphas(float _predictionTime) override;

    int NumSpiritsInside();

    void Read(TextReader* _in, bool _dynamic) override;
    void Write(FileWriter* _out) override;

    void GetDockPoint(Vector3& _pos, Vector3& _front);

    void ListSoundEvents(LList<const char*>* _list) override;
};

#endif
