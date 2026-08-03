#pragma once

// The world's roster of entity and building types, by name.
//
// SoundSystem needs it because sound blueprints are content: `Sounds.txt` says
// `ENTITY Citizen` and `BUILDING RadarDish`, and the blueprint tables are
// indexed by the type id those names resolve to. Reading that file means
// mapping name to id, and writing it back out means mapping id to name.
//
// The knowledge itself belongs to GameLogic. Citizen, Officer, Squad, RadarDish
// and the rest are the game's roster, not the engine's, and moving the tables
// down into NeuronClient to satisfy an include check would put them in the
// wrong layer to make a graph look right. So the dependency inverts instead,
// the same way AppCommands does it: GameLogic implements this, App installs it
// during startup before SoundSystem::Initialise reads any blueprints.
//
// See tasks/layering-inversion.yaml T17.
class WorldTypeNames
{
  public:
    virtual ~WorldTypeNames() = default;

    virtual int NumEntityTypes() const = 0;
    virtual int EntityTypeId(char const* _name) const = 0;
    virtual char const* EntityTypeName(int _type) const = 0;

    virtual int NumBuildingTypes() const = 0;
    virtual int BuildingTypeId(char const* _name) const = 0;
    virtual char const* BuildingTypeName(int _type) const = 0;
};

// Null until App installs it, and null in a test DLL. SoundSystem checks —
// without a roster it has no blueprints to load, which is the correct
// behaviour for a sound system with no game attached rather than a crash.
extern WorldTypeNames* g_worldTypeNames;
