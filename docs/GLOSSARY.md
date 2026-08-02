# Glossary

The vocabulary of `GameLogic/` and `Species/`, which is roughly 80,000 lines of
code built on domain terms that appear nowhere else in software.

Every term here is read from the source, with a pointer to where. Where a
mechanic is stated in a code comment or an enum, that is quoted; where the code
only shows structure, this says what the structure is rather than guessing at
intent.

> **These names are inherited from Darwinia and are not all settled.** The UI
> scaffolding has been renamed (`DarwiniaWindow` → `SpeciesWindow` and friends);
> **every domain name below is frozen.** `Darwinian` appears in 45 `GameData`
> files and as a literal string in level files, and names derived from it —
> `TypeDarwinian`, `numDarwinians` — move with it or not at all. The full policy
> is in
> [`CODING_STANDARDS.md`](../CODING_STANDARDS.md#renaming-away-from-darwinia).

---

## World structure

| Term | Where | What it is |
|---|---|---|
| **GlobalWorld** | `Species/GlobalWorld.h` | The campaign-level model: which locations exist, what has been unlocked, research progress. Persists across missions. |
| **GlobalLocation** | `Species/GlobalWorld.h:21` | A location as seen from the campaign map — name, position, whether it is available. |
| **GlobalEventCondition** | `Species/GlobalWorld.h:69` | A trigger predicate used for mission objectives and unlocks. |
| **Location** | `Species/Location.h` | One loaded, playable map: its teams, entities, buildings, lasers, effects, landscape, water. `Location::Advance` is the simulation tick. |
| **LevelFile** | `Species/LevelFile.h` | The on-disk description a Location is built from — buildings, spawn points, camera mounts, scripts. |
| **Team** | `Species/Team.h` | A faction within a Location. `TeamTypeLocalPlayer`, `TeamTypeRemotePlayer`, `TeamTypeCPU`, `TeamTypeUnused`. Owns `m_units`, `m_others`, and `m_specials` (officers and armour, kept for quick lookup). |
| **Unit** | `Species/Unit.h` | A *formation* of entities of one `m_troopType`, moving together with a shared `m_centrePos`, waypoint and formation offset. Not every entity belongs to a unit. |
| **Entity** | `GameLogic/Entity.h` | One creature or vehicle. 19 types, listed below. |
| **`m_others`** | `Species/Team.h:34` | A team's entities that are *not* in a unit — Darwinians, officers, armour. |
| **WorldObject** | `GameLogic/WorldObject.h` | Base for anything with a position and velocity in the world: entities, buildings, lasers, effects, spirits. |
| **WorldObjectId** | `GameLogic/WorldObject.h:18` | Identity of a world object: `m_teamId`, `m_unitId`, `m_index`, `m_uniqueId`. **`m_index` is a raw `DArray` slot and is serialised onto the wire** — see [determinism](../CODING_STANDARDS.md#determinism). |

---

## Entities

The 19 values of the entity type enum (`GameLogic/Entity.h:23`).

### The player's side

| Entity | File | Notes |
|---|---|---|
| **Darwinian** | `Darwinian.*` | The green population the game is named for. Not directly commanded — they are led, promoted, and herded. 14 states including `StateWorshipSpirit`, `StateOperatingPort`, `StateInsideArmour`, `StateCapturedByAnt`, `StateOnFire`. |
| **Engineer** | `Engineer.*` | Collects Spirits and delivers them; the player's manipulator of the spirit economy. |
| **Officer** | `Officer.*` | A promoted Darwinian that issues standing orders to nearby Darwinians. Orders are `OrderNone`, `OrderGoto`, `OrderFollow` (`Officer.h:24`). |
| **Armour** | `Armour.*` | A troop transport Darwinians board (`StateApproachingArmour` → `StateInsideArmour`). |
| **InsertionSquadie** | `InsertionSquad.*` | The directly-controlled squad summoned by the Squad program. |
| **LaserTroop** | `LaserTrooper.*` | Basic armed unit type. |
| **AI** | `Ai.*` | Entity type driving CPU teams. |

### The virus

The antagonist faction. No single "virus" class — it is this set of entity and
building types.

| Entity | File | Notes |
|---|---|---|
| **Virii** | `Virii.*` | The basic infection creature. |
| **Centipede** | `Centipede.*` | Segmented multi-part entity; segments are separate linked entities. |
| **SoulDestroyer** | `SoulDestroyer.*` | Large segmented entity that consumes Spirits — it holds an `m_spirits` array of capture timestamps (`SoulDestroyer.cpp:204`). |
| **Spider** | `Spider.*` | Leaping attacker. Uses `EntityLeg` for procedural legs. |
| **SporeGenerator** | `SporeGenerator.*` | Spawns spores/eggs. |
| **TriffidEgg** | `Triffid.*` | Projectile-laid egg from the Triffid building. |
| **ArmyAnt** | `ArmyAnt.*` | Captures Darwinians (`StateCapturedByAnt`). Spawned from an AntHill. |
| **Tripod**, **SpaceInvader**, **Lander** | `Tripod.*`, … | Additional hostile types. |

### Other

| Entity | File | Notes |
|---|---|---|
| **Egg** | `Egg.*` | States `StateDormant` → `StateFertilised` → `StateOpen`. Unfertilised eggs expire after `EGG_DORMANTLIFE` (120) seconds (`Egg.h:7`). Fertilised by a Spirit. |

---

## The Spirit economy

The central mechanic, and the one most worth understanding before reading
`GameLogic/`.

**Spirit** (`GameLogic/Spirit.h`) — released when an entity dies.
`Spirit::m_worldObjectId` is commented *"The Id of the entity that died"*.
Spawned via `Location::SpawnSpirit(pos, vel, teamId, worldObjectId)`.

Its lifecycle is the state enum at `Spirit.h:13`:

| State | Comment in source |
|---|---|
| `StateBirth` | "Just been created, float to a certain height" |
| `StateFloating` | "Float around, can be captured" |
| `StateAttached` | "Attached to a garbage collector" |
| `StateInStore` | "Locked in a Spirit Store" |
| `StateInEgg` | "Fertilising an Egg" |
| `StateDeath` | "Fade away" |

A floating Spirit tracks up to `SPIRIT_MAXNEARBYEGGS` (8) nearby eggs and can
fertilise one. Engineers collect them (`CollectorArrives`, `CollectorDrops`).

The buildings that move Spirits around:

| Building | File | Role |
|---|---|---|
| **Incubator** | `Incubator.*` | Spirits in, entities out. `AddSpirit()`, `NumSpiritsInside()`, `SpawnEntity()`, with an `m_troopType` deciding what is produced. |
| **SpiritReceiver** / **ReceiverBuilding** | `SpiritReceiver.h:12` | Receives Spirits. Renders "unprocessed" spirits, so receipt and processing are distinct stages. |
| **SpiritProcessor** | `SpiritReceiver.h:55` | Processes received Spirits. |
| **SpiritStore** | `SpiritStore.*` | Holds Spirits (`StateInStore`). |
| **ReceiverSpiritSpawner** | building type | Emits Spirits into the receiver network. |
| **SpawnPoint** / **SpawnLink** | `SpawnPoint.*` | Spirits travel along links between spawn points (`SpawnPoint.cpp:282`). |

---

## Buildings

~55 types (`GameLogic/Building.h:26`). The ones you will meet most:

| Building | Role |
|---|---|
| **Factory**, **ConstructionYard** (`TypeYard`) | Produce entities. |
| **Generator**, **PowerStation**, **SolarPanel**, **FuelGenerator** | Power generation. |
| **Refinery**, **Mine**, **TrackLink/Junction/Start/End** | Resource processing and the ore track network. |
| **TrunkPort** | Inter-location connection — how Darwinians and control move between maps. |
| **Teleport** | Intra-location movement. Officers can direct Darwinians into one (`Officer::m_wayPointTeleportId`). |
| **RadarDish** | Aimed by the player (`NetworkUpdate::AimBuilding`); links locations. |
| **ControlTower** | Captured to take control of a structure. |
| **GunTurret** | Player-aimable defensive turret. |
| **LaserFence** | Toggleable barrier (`NetworkUpdate::ToggleLaserFence`). |
| **ResearchItem** | Picked up to unlock or advance a program. |
| **Library**, **BlueprintStore**, **BlueprintConsole**, **BlueprintRelay** | The blueprint/research chain. |
| **UpgradePort**, **PrimaryUpgradePort** | Operated by Darwinians (`StateOperatingPort`). |
| **SafeArea** | A zone objective — hold it with enough Darwinians. |
| **AntHill**, **Triffid**, **Spam**, **GodDish** | Hostile structures. |
| **ScriptTrigger** | Fires a script when entered. |
| **StaticShape**, **Tree**, **Wall**, **Bridge**, **Pylon** | Scenery and terrain structures. |
| **AITarget**, **AISpawnPoint** | Navigation and spawn hints for CPU teams. |

---

## Player interaction

| Term | Where | What it is |
|---|---|---|
| **Task Manager** | `Species/TaskManager.*` | The in-game interface for running **Programs**. Deliberately styled as an OS task manager — the fiction is that you are a user operating a computer. |
| **Task** | `Species/TaskManager.h` | One running Program instance. States `StateIdle` ("not yet run"), `StateStarted` ("running, not yet targetted"), `StateRunning` ("targetted, running"), `StateStopping`. |
| **Program** | `GlobalResearch` enum, `Species/GlobalWorld.h:165` | What the player can run: `Squad`, `Laser`, `Grenade`, `Rocket`, `Controller`, `AirStrike`, `Armour`, `TaskManager`, `Engineer`. Each has a research *level* and *progress*. |
| **Controller** | `TypeController` | The program that promotes a Darwinian to Officer. `Task::Promote` / `Task::Demote`. |
| **Research** | `GlobalResearch` | Levels and progress per program, advanced by collecting ResearchItems. |
| **Route** | `Species/RoutingSystem.*` | A waypoint path. Units follow one via `m_routeId` / `m_routeWayPointId`; the Controller task builds one. |
| **Sepulveda** | `NeuronClient/SepulvedaStrings.cpp`, `SoundSystem.h:46` (`TypeSepulveda`) | The narrator voice. A distinct sound channel type, with its own string table. |
| **Attract mode** | `Species/Attract.*` | The idle demo loop. |

---

## Engine terms

| Term | Where | What it is |
|---|---|---|
| **Neuron** | `NeuronCore/`, `NeuronClient/`, `NeuronServer/` | The engine layers, as distinct from the game. See [ARCHITECTURE.md](ARCHITECTURE.md). |
| **Eclipse** | `NeuronClient/Eclipse.*`, `EclWindow.*`, `EclButton.*` | The immediate-mode-ish UI toolkit every in-game window derives from. `DarwiniaWindow` is the game's styled subclass. |
| **DArray** | `NeuronClient/DArray.h` | Slot map: *"an entry's index never changes"*. A `shadow` array marks live slots. **Not a `std::vector`** — see [determinism](../CODING_STANDARDS.md#determinism). |
| **LList** | `NeuronClient/LList.h` | Linked list. Often owns its elements (`EmptyAndDelete`). |
| **SliceDArray**, **FastDArray** | `NeuronClient/` | `DArray` variants. `SliceDArray` supports advancing a subset per slice. |
| **Slice** | `Species/Globals.h:5` | `NUM_SLICES_PER_FRAME` = 10. Heavy simulation is spread across 10 slices per frame; `g_sliceNum` is the current one. |
| **Sequence id** | `NeuronCore/Server.h` | The server's monotonic tick counter. Every broadcast carries one; clients apply them in order. |
| **Sync value** | `Species/Main.cpp:274` | One-byte checksum of all entity positions and velocities, compared across clients to detect desync. |
| **Shape / `.shp`** | `NeuronClient/Shape.*`, `GameData/Shapes/` | The model format. `ShapeMarker` names an attachment point on a model — buildings use them for spirit entrances, docks and exits. |
| **`darwiniaRandom()`** | `NeuronClient/Random.cpp` | The single global LCG the simulation shares. Its call sequence is load-bearing. |

---

## A note on naming

Several terms here describe mechanics whose names will change as the project
moves away from Darwinia — `Darwinian` most obviously. This glossary describes
the code **as it is**.

Renaming is governed by
[`CODING_STANDARDS.md`](../CODING_STANDARDS.md#renaming-away-from-darwinia). The
short version: the UI scaffolding rename is done, and every domain term in this
glossary is frozen until the game runs again. 372 Darwinia-derived occurrences
remain: 342 either named in `GameData/` or derived from something that is, and 30
that are game-name *strings* — on-screen branding and user-data paths — rather
than identifiers. Before renaming anything, ask both questions the policy sets
out: `grep -rlw "<TheName>" GameData/`, *and* whether the name derives from one
that would fail that grep.
