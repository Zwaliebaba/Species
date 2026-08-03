#include "pch.h"

#include "Building.h"
#include "Entity.h"
#include "WorldPointers.h"
#include "WorldTypeRoster.h"

namespace
{
  // Forwards to the two static tables that already exist. Nothing is duplicated
  // here — Entity::GetTypeName and Building::GetTypeName remain the single
  // definition of what the types are called, which is what keeps the names in
  // Sounds.txt and the names in the level files the same names.
  class WorldTypeRoster : public WorldTypeNames
  {
    public:
      int NumEntityTypes() const override { return Entity::NumEntityTypes; }
      int EntityTypeId(char const* _name) const override { return Entity::GetTypeId(_name); }
      char const* EntityTypeName(int _type) const override { return Entity::GetTypeName(_type); }

      int NumBuildingTypes() const override { return Building::NumBuildingTypes; }
      int BuildingTypeId(char const* _name) const override { return Building::GetTypeId(_name); }
      char const* BuildingTypeName(int _type) const override { return Building::GetTypeName(_type); }
  };

  WorldTypeRoster s_roster;
} // namespace


void InstallWorldTypeRoster() { g_worldTypeNames = &s_roster; }
