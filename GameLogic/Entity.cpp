#include "pch.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "Debug.h"
#include "DebugRender.h"
#include "MathUtils.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "TextureUv.h"
#include "TextStreamReaders.h"
#include "LanguageTable.h"

#include "Camera.h"
#include "GameTime.h"
#include "SoundSystem.h"
#include "Unit.h"
#include "Location.h"
#include "EntityGrid.h"
#include "ObstructionGrid.h"
#include "RoutingSystem.h"
#include "LevelFile.h"

#include "Entity.h"
#include "RadarDish.h"
#include "Bridge.h"
#include "InsertionSquad.h"
#include "Engineer.h"
#include "Virii.h"
#include "InsertionSquad.h"
#include "LaserTrooper.h"
#include "Egg.h"
#include "SporeGenerator.h"
#include "Lander.h"
#include "Tripod.h"
#include "Centipede.h"
#include "Airstrike.h"
#include "Spider.h"
#include "Darwinian.h"
#include "Officer.h"
#include "ArmyAnt.h"
#include "Armour.h"
#include "SoulDestroyer.h"
#include "Triffid.h"
#include "Ai.h"
#include "LaserFence.h"
#include "WorldPointers.h"
#include "AppState.h"


// ****************************************************************************
//  Class EntityBlueprint
// ****************************************************************************

char	*EntityBlueprint::m_names[Entity::NumEntityTypes];
float	 EntityBlueprint::m_stats[Entity::NumEntityTypes][Entity::NumStats];


void EntityBlueprint::Initialise()
{
	TextReader *theFile = g_resource->GetTextReader("Stats.txt");
	ASSERT_TEXT(theFile && theFile->IsOpen(), "Couldn't open stats.txt");

    int entityIndex = 0;

    while( theFile->ReadLine() )
    {
		if (!theFile->TokenAvailable()) continue;
        ASSERT_TEXT(entityIndex < Entity::NumEntityTypes, "Too many entity blueprints defined");

        m_names[entityIndex] = strdup(theFile->GetNextToken());
		m_stats[entityIndex][0] = atof(theFile->GetNextToken());
        m_stats[entityIndex][1] = atof(theFile->GetNextToken());
        m_stats[entityIndex][2] = atof(theFile->GetNextToken());

        ++entityIndex;
    }

	delete theFile;
}

char *EntityBlueprint::GetName( unsigned char _type )
{
    DEBUG_ASSERT( _type < Entity::NumEntityTypes );
    return m_names[_type];
}

float EntityBlueprint::GetStat( unsigned char _type, int _stat )
{
    DEBUG_ASSERT( _type < Entity::NumEntityTypes );
    DEBUG_ASSERT( _stat < Entity::NumStats );

	if (_stat == Entity::StatSpeed)
	{
		if (_type != Entity::TypeSpaceInvader)
			return m_stats[_type][_stat] * (1.0f + (float) g_difficultyLevel / 10.0f);
	}

    return m_stats[_type][_stat];
}




// ****************************************************************************
//  Class Entity
// ****************************************************************************

Entity::Entity()
:   WorldObject(),
    m_formationIndex(-1),
    m_dead(false),
    m_reloading(0.0f),
    m_shape(nullptr),
    m_justFired(false),
    m_radius(0.0f),
    m_buildingId(-1),
    m_roamRange(0.0f),
    m_inWater(-1.0f),
    m_renderDamaged(false),
	m_routeId(-1),
	m_routeWayPointId(-1),
	m_routeTriggerDistance(10.0f)
{
    memset( m_stats, 0, NumStats * sizeof(m_stats[0]) );
}


Entity::~Entity()
{
}


void Entity::SetType( unsigned char _type )
{
    m_type = _type;

    for( int i = 0; i < NumStats; ++i )
    {
        m_stats[i] = EntityBlueprint::GetStat( m_type, i );
    }


    m_reloading = syncfrand( m_stats[StatRate] );
}


void Entity::ChangeHealth( int amount )
{
    if( !m_dead )
    {
        if( amount < 0 )
        {
            g_soundSystem->TriggerEntityEvent( this, "LoseHealth" );
        }

        if( m_stats[StatHealth] + amount <= 0 )
        {
            m_stats[StatHealth] = 100;
            m_dead = true;
            g_soundSystem->TriggerEntityEvent( this, "Die" );
            g_location->SpawnSpirit( m_pos, m_vel * 0.5f, m_id.GetTeamId(), m_id );
        }
        else if( m_stats[StatHealth] + amount > 255 )
        {
            m_stats[StatHealth] = 255;
        }
        else
        {
            m_stats[StatHealth] += amount;
        }
    }
}


void Entity::Attack( Vector3 const &pos )
{
    if( m_reloading == 0.0f )
    {
		Vector3 toPos( pos.x + syncsfrand(15.0f),
					   pos.y,
					   pos.z + syncsfrand(15.0f) );
		Vector3 fromPos = m_pos;
		fromPos.y += 2.0f;
		Vector3 velocity = (toPos - fromPos).SetLength(200.0f);
		g_location->FireLaser( fromPos, velocity, m_id.GetTeamId() );

        m_reloading = m_stats[StatRate];
        m_justFired = true;

        g_soundSystem->TriggerEntityEvent( this, "Attack" );
    }
}


bool Entity::AdvanceDead( Unit *_unit )
{
    int newHealth = m_stats[StatHealth];
    if( m_onGround ) newHealth -= 40 * SERVER_ADVANCE_PERIOD;
    else             newHealth -= 20 * SERVER_ADVANCE_PERIOD;

    if( newHealth <= 0 )
    {
        m_stats[StatHealth] = 0;
        return true;
    }
    else
    {
        m_stats[StatHealth] = newHealth;
    }

    return false;
}


int Entity::EnterTeleports( int _requiredId )
{
    LList<int> *buildings = g_location->m_obstructionGrid->GetBuildings( m_pos.x, m_pos.z );

    for( int i = 0; i < buildings->Size(); ++i )
    {
        int buildingId = buildings->GetData(i);
        if( _requiredId != -1 && _requiredId != buildingId )
        {
            // We are only permitted to enter building with id _requiredId
            continue;
        }

        Building *building = g_location->GetBuilding( buildingId );
        DEBUG_ASSERT( building );

        if( building->m_type == Building::TypeRadarDish  )
        {
            RadarDish *radarDish = (RadarDish *) building;
            Vector3 entrancePos, entranceFront;
            radarDish->GetEntrance( entrancePos, entranceFront );
            float range = ( m_pos - entrancePos ).Mag();
            if( radarDish->ReadyToSend() &&
                range < 15.0f &&
                m_front * entranceFront < 0.0f )
            {
                WorldObjectId id(m_id);
                radarDish->EnterTeleport( id );
                g_soundSystem->TriggerEntityEvent( this, "EnterTeleport" );
                return buildingId;
            }
        }
        else if( building->m_type == Building::TypeBridge )
        {
            Bridge *bridge = (Bridge *) building;
            Vector3 entrancePos, entranceFront;
            if( bridge->GetEntrance( entrancePos, entranceFront ) )
            {
                float range = ( m_pos - bridge->m_pos ).Mag();
                if( bridge->ReadyToSend() &&
                    range < 25.0f &&
                    m_front * entranceFront < 0.0f )
                {
                    WorldObjectId id( m_id );
                    bridge->EnterTeleport( id );
                    g_soundSystem->TriggerEntityEvent( this, "EnterTeleport" );
                    return buildingId;
                }
            }
        }
    }

    return -1;
}


void Entity::AdvanceInAir( Unit *_unit )
{
    m_vel += Vector3(0,-15.0,0) * SERVER_ADVANCE_PERIOD;
    m_pos += m_vel * SERVER_ADVANCE_PERIOD;

    if( !m_dead )
    {
        float groundLevel = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z) + 0.1f;
        if( m_pos.y <= groundLevel )
        {
            m_onGround = true;
            m_vel = g_zeroVector;
            m_pos.y = groundLevel + 0.1f;
        }
        if( m_pos.y <= 0.0f /*seaLevel*/ )
        {
            m_inWater = 0;
            m_vel = g_zeroVector;
        }
    }
}


void Entity::AdvanceInWater( Unit *_unit )
{
    m_inWater += SERVER_ADVANCE_PERIOD;
    if( m_inWater > 10.0f /* && m_type != TypeInsertionSquadie */ )
    {
        ChangeHealth( -500 );
    }

    Vector3 targetPos;
    if( _unit )
    {
        targetPos = _unit->GetWayPoint();
        targetPos.y = 0.0f;
        Vector3 offset = _unit->GetFormationOffset(Unit::FormationRectangle, m_id.GetIndex() );
        targetPos += offset;
    }

    if( targetPos != g_zeroVector )
    {
        Vector3 distance = targetPos - m_pos;
        distance.y = 0.0f;
        m_vel = distance;
        m_vel.Normalise();
        m_front = m_vel;
        m_vel *= m_stats[StatSpeed] * 0.3f;

        if (distance.Mag() < m_vel.Mag() * SERVER_ADVANCE_PERIOD)
        {
            m_vel = distance / SERVER_ADVANCE_PERIOD;
        }
    }

    m_vel.SetLength( 5.0f );
    m_pos += m_vel * SERVER_ADVANCE_PERIOD;
    m_pos.y = 0.0f - sinf(m_inWater * 3.0f) - 4.0f;

    float groundLevel = g_location->m_landscape.m_heightMap->GetValue(m_pos.x, m_pos.z);
    if( groundLevel > 0.0f )
    {
        m_inWater = -1;
    }
}


void Entity::Begin()
{
    g_soundSystem->TriggerEntityEvent( this, "Create" );

    if( m_shape )
    {
        m_centrePos = m_shape->CalculateCentre( g_identityMatrix34 );
        m_radius = m_shape->CalculateRadius( g_identityMatrix34, m_centrePos );
    }
}


bool Entity::Advance(Unit *_unit)
{
    if( !m_enabled ) return false;

    if( m_dead )
    {
        bool amIDead = AdvanceDead( _unit );
        if( amIDead )
		{
			return true;
		}
    }

    //m_pos = PushFromObstructions( m_pos );
    //m_pos = PushFromEachOther( m_pos );

    if( m_reloading > 0.0f )
    {
        m_reloading -= SERVER_ADVANCE_PERIOD;
        if( m_reloading < 0.0f ) m_reloading = 0.0f;
    }

    float healthFraction = (float) m_stats[StatHealth] / (float) EntityBlueprint::GetStat( m_type, StatHealth );
    float timeIndex = g_gameTime + m_id.GetUniqueId() * 10;
    m_renderDamaged = ( frand(0.75f) * (1.0f - fabs(sinf(timeIndex))*0.8f) > healthFraction );

    if( m_inWater != -1 )   AdvanceInWater(_unit);

    float worldSizeX = g_location->m_landscape.GetWorldSizeX();
    float worldSizeZ = g_location->m_landscape.GetWorldSizeZ();
    if( m_pos.x < 0.0f ) m_pos.x = 0.0f;
    if( m_pos.z < 0.0f ) m_pos.z = 0.0f;
    if( m_pos.x >= worldSizeX ) m_pos.x = worldSizeX;
    if( m_pos.z >= worldSizeZ ) m_pos.z = worldSizeZ;

    if (_unit && !m_dead )
	{
		_unit->UpdateEntityPosition( m_pos, m_radius );
	}

	if( m_routeId != -1 )
	{
		FollowRoute();
	}

    return false;
}


void Entity::Render(float predictionTime)
{
}


bool Entity::IsInView ()
{
    return true;
}


Vector3 Entity::PushFromEachOther( Vector3 const &_pos )
{
    Vector3 result = _pos;

    int numFound;
    WorldObjectId *neighbours = g_location->m_entityGrid->GetNeighbours( _pos.x, _pos.z, 2.0f, &numFound );


    for( int i = 0; i < numFound; ++i )
    {
        WorldObjectId id = neighbours[i];
        if( !( id == m_id ) )
        {
            WorldObject *obj = g_location->GetEntity( id );
//            float distance = (obj->m_pos - thisPos).Mag();
//            while( distance < 1.0f )
//            {
//                Vector3 pushForce = (obj->m_pos - thisPos).Normalise() * 0.1f;
//                if( obj->m_pos == thisPos ) pushForce = Vector3(0.0f,0.0f,1.0f);
//                thisPos -= pushForce;
//                distance = (obj->m_pos - thisPos).Mag();
//            }

            Vector3 diff = ( _pos - obj->m_pos );
            float force = 0.1f / diff.Mag();
            diff.Normalise();

            Vector3 thisDiff = ( diff * force );
            result += thisDiff;

        }
    }

    result.y = g_location->m_landscape.m_heightMap->GetValue( result.x, result.z );
    return result;
}


Vector3 Entity::PushFromCliffs( Vector3 const &pos, Vector3 const &oldPos )
{
    Vector3 horiz = ( pos - oldPos );
    horiz.y = 0.0f;
    float horizDistance = horiz.Mag();
    float gradient = (pos.y - oldPos.y) / horizDistance;

    Vector3 result = pos;

    if( gradient > 1.0f )
    {
        float distance = 0.0f;
        while( distance < 50.0f )
        {
            float angle = distance * 2.0f * M_PI;
            Vector3 offset( cosf(angle) * distance, 0.0f, sinf(angle) * distance );
            Vector3 newPos = result + offset;
            newPos.y = g_location->m_landscape.m_heightMap->GetValue( newPos.x, newPos.z );
            horiz = ( newPos - oldPos );
            horiz.y = 0.0f;
            horizDistance = horiz.Mag();
            float newGradient = ( newPos.y - oldPos.y ) / horizDistance;
            if( newGradient < 1.0f )
            {
                result = newPos;
                break;
            }
            distance += 0.2f;
        }
    }

    return result;
}


Vector3 Entity::PushFromObstructions( Vector3 const &pos, bool killem )
{
    Vector3 result = pos;
    if( m_onGround )
    {
        result.y = g_location->m_landscape.m_heightMap->GetValue( result.x, result.z );
    }

    Matrix34 transform( m_front, g_upVector, result );

    //
    // Push from Water

    if( result.y <= 1.0f )
    {
        float pushAngle = syncsfrand(1.0f);
        float distance = 0.0f;
        while( distance < 50.0f )
        {
            float angle = distance * pushAngle * M_PI;
            Vector3 offset( cosf(angle) * distance, 0.0f, sinf(angle) * distance );
            Vector3 newPos = result + offset;
            float height = g_location->m_landscape.m_heightMap->GetValue( newPos.x, newPos.z );
            if( height > 1.0f )
            {
                result = newPos;
                result.y = height;
                break;
            }
            distance += 1.0f;
        }
    }


    //
    // Push from buildings

    LList<int> *buildings = g_location->m_obstructionGrid->GetBuildings( result.x, result.z );

    for( int b = 0; b < buildings->Size(); ++b )
    {
        int buildingId = buildings->GetData(b);
        Building *building = g_location->GetBuilding( buildingId );
        if( building )
        {
            bool hit = false;
            if( m_shape && building->DoesShapeHit( m_shape, transform ) ) hit = true;
            if( !m_shape && building->DoesSphereHit( result, 1.0f ) ) hit = true;

            if( hit )
            {
                if( building->m_type == Building::TypeLaserFence && killem &&
					((LaserFence *) building)->IsEnabled() )
                {
                    ChangeHealth( -9999 );
                    ((LaserFence *) building)->Electrocute( m_pos );
                }
                else
                {
                    Vector3 pushForce = (building->m_pos - result).SetLength(2.0f);
                    while( building->DoesSphereHit( result, 1.0f ) )
                    {
                        result -= pushForce;
                        //result.y = g_location->m_landscape.m_heightMap->GetValue( result.x, result.z );
                    }
                }
            }
        }
    }

    return result;
}


static float s_nearPlaneStart;

void Entity::BeginRenderShadow()
{
    glEnable        ( GL_TEXTURE_2D );
    glBindTexture   ( GL_TEXTURE_2D, g_resource->GetTexture( "Textures/Glow.bmp" ) );
	glTexParameteri	( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri	( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
    glDisable       ( GL_CULL_FACE );
    glDepthMask     ( false );

    glEnable        ( GL_BLEND );
    glBlendFunc     ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR );
    glColor4f       ( 0.6f, 0.6f, 0.6f, 0.0f );

	s_nearPlaneStart = g_renderer->GetNearPlane();
	g_camera->SetupProjectionMatrix(s_nearPlaneStart * 1.05f,
							 			   g_renderer->GetFarPlane());
}


void Entity::EndRenderShadow()
{
    glBlendFunc     ( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glDisable       ( GL_BLEND );

    glDepthMask     ( true );
    glEnable        ( GL_CULL_FACE );
    glDisable       ( GL_TEXTURE_2D );

	g_camera->SetupProjectionMatrix(s_nearPlaneStart,
								 		   g_renderer->GetFarPlane());
}


void Entity::RenderShadow( Vector3 const &_pos, float _size )
{
    Vector3 shadowPos = _pos;
    Vector3 shadowR = Vector3(_size,0,0);
    Vector3 shadowU = Vector3(0,0,_size);

    Vector3 posA = shadowPos - shadowR - shadowU;
    Vector3 posB = shadowPos + shadowR - shadowU;
    Vector3 posC = shadowPos + shadowR + shadowU;
    Vector3 posD = shadowPos - shadowR + shadowU;

    posA.y = g_location->m_landscape.m_heightMap->GetValue( posA.x, posA.z ) + 0.9f;
    posB.y = g_location->m_landscape.m_heightMap->GetValue( posB.x, posB.z ) + 0.9f;
    posC.y = g_location->m_landscape.m_heightMap->GetValue( posC.x, posC.z ) + 0.9f;
    posD.y = g_location->m_landscape.m_heightMap->GetValue( posD.x, posD.z ) + 0.9f;

    posA.y = max( posA.y, 1.0f );
    posB.y = max( posB.y, 1.0f );
    posC.y = max( posC.y, 1.0f );
    posD.y = max( posD.y, 1.0f );

    if( posA.y > _pos.y && posB.y > _pos.y && posC.y > _pos.y && posD.y > _pos.y )
    {
        // The object casting the shadow is beneath the ground
        return;
    }

    glBegin( GL_QUADS );
        glTexCoord2f( 0.0f, 0.0f );     glVertex3fv     ( posA.GetData() );
        glTexCoord2f( 1.0f, 0.0f );     glVertex3fv     ( posB.GetData() );
        glTexCoord2f( 1.0f, 1.0f );     glVertex3fv     ( posC.GetData() );
        glTexCoord2f( 0.0f, 1.0f );     glVertex3fv     ( posD.GetData() );
    glEnd();
}


Entity *Entity::NewEntity( int _troopType )
{
    Entity *entity = nullptr;

    switch(_troopType)
    {
        case Entity::TypeLaserTroop:            entity = new LaserTrooper();        break;
        case Entity::TypeInsertionSquadie:      entity = new Squadie();             break;
        case Entity::TypeEngineer:              entity = new Engineer();            break;
        case Entity::TypeVirii:                 entity = new Virii();               break;
        case Entity::TypeEgg:                   entity = new Egg();                 break;
        case Entity::TypeSporeGenerator:        entity = new SporeGenerator();      break;
        case Entity::TypeLander:                entity = new Lander();              break;
		case Entity::TypeTripod:				entity = new Tripod();				break;
        case Entity::TypeCentipede:             entity = new Centipede();           break;
        case Entity::TypeSpaceInvader:          entity = new SpaceInvader();        break;
		case Entity::TypeSpider:				entity = new Spider();				break;
        case Entity::TypeDarwinian:             entity = new Darwinian();           break;
        case Entity::TypeOfficer:               entity = new Officer();             break;
        case Entity::TypeArmyAnt:               entity = new ArmyAnt();             break;
        case Entity::TypeArmour:                entity = new Armour();              break;
        case Entity::TypeSoulDestroyer:         entity = new SoulDestroyer();       break;
        case Entity::TypeTriffidEgg:            entity = new TriffidEgg();          break;
        case Entity::TypeAI:                    entity = new AI();                  break;

        default:                                DEBUG_ASSERT(false);
    }

    entity->m_id.GenerateUniqueId();

    return entity;
}


int Entity::GetTypeId( char const *_typeName )
{
    for( int i = 0; i < NumEntityTypes; ++i )
    {
        if( stricmp( _typeName, GetTypeName(i) ) == 0 )
        {
            return i;
        }
    }
    return -1;
}


char const *Entity::GetTypeName( int _troopType )
{
    static char const *typeNames[NumEntityTypes] = {
                                                "InvalidType",
                                                "LaserTroop",
                                                "Engineer",
                                                "Virii",
                                                "Squadie",
                                                "Egg",
                                                "SporeGenerator",
                                                "Lander",
												"Tripod",
                                                "Centipede",
                                                "SpaceInvader",
												"Spider",
                                                "Darwinian",
                                                "Officer",
                                                "ArmyAnt",
                                                "Armour",
                                                "SoulDestroyer",
                                                "TriffidEgg",
                                                "AI"
                                                };

    DEBUG_ASSERT( _troopType >= 0 && _troopType < NumEntityTypes );
    return typeNames[ _troopType ];
}


char const *Entity::GetTypeNameTranslated ( int _troopType )
{
    char const *typeName = GetTypeName( _troopType );

    char stringId[256];
    sprintf( stringId, "entityname_%s", typeName );

    if( ISLANGUAGEPHRASE( stringId ) )
    {
        return LANGUAGEPHRASE( stringId );
    }
    else
    {
        return typeName;
    }
}


void Entity::ListSoundEvents(std::vector<const char*>* _list)
{
  _list->push_back("Create");
  _list->push_back("Attack");
  _list->push_back("LoseHealth");
  _list->push_back("EnterTeleport");
  _list->push_back("ExitTeleport");
  _list->push_back("Die");
}


bool Entity::RayHit(Vector3 const &_rayStart, Vector3 const &_rayDir)
{
	if (m_shape)
	{
		RayPackage package(_rayStart, _rayDir);
		Matrix34 mat(m_front, g_upVector, m_pos);
		return m_shape->RayHit(&package, mat);
	}
	else
	{
		return RaySphereIntersection(_rayStart, _rayDir, m_pos, 5.0f);
	}
}


void Entity::DirectControl( TeamControls const& _teamControls )
{
}

Vector3 Entity::GetCameraFocusPoint()
{
	return m_pos + m_vel;
}

void Entity::SetWaypoint( Vector3 const _waypoint )
{
}

void Entity::FollowRoute()
{
	DEBUG_ASSERT(m_routeId != -1);
	Route *route = g_location->m_levelFile->GetRoute(m_routeId);
	DEBUG_ASSERT(route);

	if (m_routeWayPointId == -1)
	{
		m_routeWayPointId = 0;
	}

    WayPoint *waypoint = route->m_wayPoints.GetData(m_routeWayPointId);
    SetWaypoint ( waypoint->GetPos() );
	Vector3 targetVect = waypoint->GetPos() - m_pos;

	m_spawnPoint = waypoint->GetPos();

    if (waypoint->m_type != WayPoint::TypeBuilding &&
        targetVect.Mag() < m_routeTriggerDistance )
	{

		m_routeWayPointId++;
		if (m_routeWayPointId >= route->m_wayPoints.Size())
		{
			m_routeWayPointId = -1;
			m_routeId = -1;
		}
	}

    //
    // If its a building instead of a 3D pos, this unit will never
    // get to the next waypoint.  A new unit is created when the unit
    // enters the teleport, and taht new unit will automatically
    // continue towards the next waypoint instead.
    //
}
