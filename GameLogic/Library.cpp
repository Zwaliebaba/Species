#include "pch.h"
#include "Resource.h"
#include "Shape.h"
#include "Debug.h"

#include "Library.h"
#include "ResearchItem.h"

#include "Location.h"
#include "WorldPointers.h"


Library::Library()
:   Building()
{
    m_type = Building::TypeLibrary;
    SetShape( g_resource->GetShape( "Library.shp" ) );

    memset( m_scrollSpawned, 0, GlobalResearch::NumResearchItems * sizeof(bool) );
}


bool Library::Advance()
{
    for( int i = 0; i < GlobalResearch::NumResearchItems; ++i )
    {
        if( !m_scrollSpawned[i] &&
            g_globalWorld->m_research->HasResearch(i) )
        {
            char markerName[256];
            sprintf( markerName, "MarkerResearch%02d", i+1 );
            ShapeMarker *scrollMarker = m_shape->m_rootFragment->LookupMarker( markerName );
            DEBUG_ASSERT( scrollMarker );

            Matrix34 rootMat(m_front, g_upVector, m_pos);
            Matrix34 scrollPos = scrollMarker->GetWorldMatrix( rootMat );

            ResearchItem *item = new ResearchItem();
            item->m_researchType = i;
            item->m_inLibrary = true;
            item->m_pos = scrollPos.pos;
            item->m_id.SetUniqueId( g_globalWorld->GenerateBuildingId() );
            g_location->m_buildings.PutData( item );

            m_scrollSpawned[i] = true;
        }
    }

    return Building::Advance();
}


