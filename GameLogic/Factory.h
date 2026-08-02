#pragma once

#include "Building.h"
#include "Entity.h"
#include "SpiritStore.h"


class FileWriter;


class Factory: public Building
{
public:
    unsigned char   m_troopType;
    unsigned char   m_stats[Entity::NumStats];

	int				m_initialCapacity;		// Read from level file

    int             m_unitId;
    int             m_numToCreate;
    int             m_numCreated;

    float           m_timeToCreate;         // Total Time to create ALL troops
    float           m_timeSoFar;

    enum
    {
        StateUnused,
        StateCreating,
        StateRecharging
    };
    int m_state;

    SpiritStore     m_spiritStore;

public:
    Factory();

    void Initialise( Building *_template );

    void Render         ( float predictionTime );
    void RenderAlphas   ( float predictionTime );

    bool Advance();
    void AdvanceStateUnused();
    void AdvanceStateCreating();
    void AdvanceStateRecharging();

    void SetTeamId( int _teamId );

    void RequestUnit( unsigned char _troopType, int _numToCreate );

	void Read(TextReader *_in, bool _dynamic);
	void Write(FileWriter *_out);
};

