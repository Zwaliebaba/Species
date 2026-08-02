#ifndef _INCLUDED_TEAM_H
#define _INCLUDED_TEAM_H

#include "FastDArray.h"
#include "SliceDArray.h"
#include "RgbColour.h"

#include "WorldObject.h"
#include "Entity.h"

class Unit;
class InsertionSquad;


// ****************************************************************************
//  Class Team
// ****************************************************************************

class Team
{
public:
    enum
    {
        TeamTypeUnused = -1,
        TeamTypeLocalPlayer,
        TeamTypeRemotePlayer,
        TeamTypeCPU
    };

    int                         m_teamId;
    int                         m_teamType;

    FastDArray  <Unit *>        m_units;
    SliceDArray <Entity *>      m_others;
    LList       <WorldObjectId> m_specials;             // Officers and tanks for quick lookup

    RGBAColour               m_colour;

    int             m_currentUnitId;                    //
    int             m_currentEntityId;                  // Do not set these directly
    int             m_currentBuildingId;                // They are updated by the network
                                                        //
    Vector3         m_currentMousePos;                  //

public:
    Team();

    void Initialise     (int _teamId);                  // Call when this team enters the game
    void SetTeamType    (int _teamType);

    void SelectUnit     (int _unitId, int _entityId, int _buildingId );

    void RegisterSpecial    ( WorldObjectId _id );
    void UnRegisterSpecial  ( WorldObjectId _id );

	Entity *RayHitEntity(Vector3 const &_rayStart, Vector3 const &_rayEnd);
    Unit   *GetMyUnit   ();
    Entity *GetMyEntity ();
    Unit   *NewUnit     (int _troopType, int _numEntities, int *_unitId, Vector3 const &_pos);
    Entity *NewEntity   (int _troopType, int _unitId, int *_index);

    int  NumEntities    (int _troopType);               // Counts the total number

    void Advance        (int _slice);

    void Render             ();
    void RenderVirii        (float _predictionTime);
    void RenderDarwinians   (float _predictionTime);
    void RenderOthers       (float _predictionTime);
};


// ****************************************************************************
//  Class TeamControls
//
//   capture all the control information necessary to send
//   over the network for "remote" control of units
// ****************************************************************************


#endif
