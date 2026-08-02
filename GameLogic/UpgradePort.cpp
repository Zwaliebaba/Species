#include "pch.h"
#include "Resource.h"

#include "Debug.h"

#include "UpgradePort.h"

#include "App.h"
#include "GlobalWorld.h"
#include "WorldPointers.h"


UpgradePort::UpgradePort()
:   Building()
{
	m_type = TypeUpgradePort;
	SetShape( g_resource->GetShape("UpgradePort.shp") );
}



PrimaryUpgradePort::PrimaryUpgradePort()
:   Building(),
    m_controlTowersOwned(0)
{
    m_type = TypePrimaryUpgradePort;
    SetShape( g_resource->GetShape("PrimaryUpgradePort.shp" ) );
}


void PrimaryUpgradePort::ReprogramComplete()
{
    m_controlTowersOwned++;

    if( m_controlTowersOwned == 3 )
    {
        GlobalBuilding *gb = g_globalWorld->GetBuilding( m_id.GetUniqueId(), g_app->m_locationId );
        if( gb )
        {
            gb->m_online = true;
            g_globalWorld->EvaluateEvents();
        }
    }
}
