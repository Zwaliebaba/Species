#include "pch.h"

#include <math.h>

#include "Debug.h"
#include "FileWriter.h"
#include "Profiler.h"
#include "Resource.h"
#include "Shape.h"
#include "TextStreamReaders.h"

#include "App.h"
#include "Location.h"

#include "Building.h"
#include "LaserFence.h"
#include "PowerStation.h"


// ****************************************************************************
// Class Powerstation
// ****************************************************************************

// *** Constructor
Powerstation::Powerstation()
:   Building(),
    m_linkedBuildingId(-1)
{
    m_type = Building::TypePowerstation;
	SetShape( g_app->m_resource->GetShape("PowerStation.shp") );
}


// *** Initialise
void Powerstation::Initialise( Building *_template )
{
    Building::Initialise( _template );
	DEBUG_ASSERT(_template->m_type == Building::TypePowerstation);
    m_linkedBuildingId = ((Powerstation *) _template)->m_linkedBuildingId;
}


// *** Advance
bool Powerstation::Advance()
{
	Building *b = g_app->m_location->GetBuilding(m_linkedBuildingId);
	if (b->m_type == Building::TypeLaserFence)
    {
        LaserFence *fence = (LaserFence *) b;
        if( !fence->IsEnabled() && GetNumPorts() == GetNumPortsOccupied() )
        {
            fence->Enable();
        }

        if( fence->IsEnabled() && GetNumPortsOccupied() <= GetNumPorts() * 3.0f/4.0f )
        {
            fence->Disable();
        }
    }

    return Building::Advance();
}


// *** Render
void Powerstation::Render( float predictionTime )
{
	Building::Render(predictionTime);
}


// *** GetBuildingLink
int Powerstation::GetBuildingLink()
{
    return m_linkedBuildingId;
}


// *** SetBuildingLink
void Powerstation::SetBuildingLink( int _buildingId )
{
    m_linkedBuildingId = _buildingId;
}


// *** Read
void Powerstation::Read( TextReader *_in, bool _dynamic )
{
    Building::Read( _in, _dynamic );

    char *word = _in->GetNextToken();
    m_linkedBuildingId = atoi(word);
}


// *** Write
void Powerstation::Write( FileWriter *_out )
{
    Building::Write( _out );

    _out->printf( "%-8d", m_linkedBuildingId);
}
