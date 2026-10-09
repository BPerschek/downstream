# DOWNSTREAM — Design & Development Brief v0.3

**Audience:** AI coding agents and the human directing them  
**Engine:** Unreal Engine 5  
**Primary implementation:** C++ for simulation and major systems; Blueprints for content, configuration, presentation, and glue  
**Art pipeline:** Blender  
**Asset policy:** Original work, open-source assets, CC0 assets, and explicitly approved free/commercial-use assets are allowed  
**Target platform:** Windows PC  
**View:** 2.5D / fixed-angle isometric presentation using a real 3D world  
**World model:** One persistent seeded world with procedural regions and regenerating expedition spaces  
**Hardware ceiling:** The owner has an RTX 5050. Design and validate the complete game on this GPU; do not require a stronger GPU. Target 1080p/60 FPS with scalable settings. The previous 8 GB VRAM / 16 GB RAM figures are planning assumptions, not confirmed specifications of the owner’s machine.
**Progression:** Classless character with a shared branching skill tree; equipment remains essential.
**Offline catch-up:** Accepted cap of 8 hours of game-time catch-up.
**Story:** No authored story planned yet; prove systemic gameplay first.

---

# 0. READ THIS FIRST — DO NOT MISINTERPRET THE GAME

DOWNSTREAM is **not a digging game** and it is **not a canal-building simulator**.

Terrain deformation and simulated water are important signature systems, but they are only one part of a larger game.

The game is a:

- persistent wasteland ARPG
- settlement builder
- scavenging and extraction game
- robot-building and automation sandbox
- faction and world simulation
- terrain and water manipulation sandbox

The player should spend meaningful time:

- personally exploring dangerous areas
- fighting enemies
- finding weapons, gear, robot parts, technology, and recruits
- extracting valuable loot
- building and expanding a settlement
- creating increasingly capable robots
- automating hauling, repair, construction, defense, salvage, and production
- interacting with rival settlements
- responding to raids, storms, shortages, damage, and world events
- using terrain and water engineering when it provides a useful solution

**Terrain manipulation is a tool. It is not the purpose of the game.**

When designing a feature, do not begin with:

> How can the player dig here?

Begin with:

> What problem, opportunity, threat, or objective is the player dealing with?

Digging may be one solution. Combat, trade, construction, automation, stealth, sabotage, recruitment, or waiting for world conditions to change may be others.

This design rule exists specifically to prevent development from collapsing into a civil-engineering sandbox.

---

# 1. ONE-PARAGRAPH PITCH

DOWNSTREAM is a persistent wasteland ARPG, settlement builder, and automation sandbox. The player begins with little more than a weapon, a crude shelter, a few survivors, and damaged machinery. They venture into dangerous ruins, industrial sites, wrecks, hostile settlements, underground facilities, and wilderness to recover weapons, technology, robot components, resources, and people. Everything successfully extracted can strengthen the settlement. Over time, the player builds workshops, defenses, power systems, storage, production, infrastructure, and modular robots that haul, repair, salvage, construct, fight, patrol, mine, and reshape terrain. Rival AI settlements occupy the same world and compete for resources, routes, ruins, and water. Terrain and water are fully systemic: a player can dig canals, drain ruins, build reservoirs, irrigate land, reinforce terrain, create defensive earthworks, or flood an enemy route, but none of those actions are mandatory ways to play.

**Reference feel:** Diablo combat and loot, Tarkov-style extraction tension, Factorio-style automation and logistics, Timberborn-style readable water manipulation, Rust-style settlement pressure and neighboring threats.

These references describe individual qualities. DOWNSTREAM should not simply copy any one of them.

---

# 2. CORE PLAYER FANTASY

The player is building a functioning power center out of the remains of a broken world.

The emotional arc should be:

### Early game

"I have almost nothing."

The player has:

- basic weapons
- limited storage
- a primitive shelter
- a tiny number of survivors
- perhaps one unreliable robot or machine
- immediate local threats
- little knowledge of the surrounding world

### Midgame

"I have systems."

The player now has:

- specialized robots
- workshops
- power generation
- better weapons and armor
- automated hauling
- defended infrastructure
- established relationships with other settlements
- access to more dangerous expedition regions
- basic terrain and water engineering
- growing production capability

### Late game

"I built this machine."

The settlement can include:

- automated logistics
- modular robot fleets
- defensive networks
- specialized production chains
- water infrastructure
- pumps, gates, dams, drainage, or irrigation where useful
- remote outposts
- guarded resource sites
- strong faction relationships or enemies
- high-end equipment fabrication
- long-distance expeditions
- competing strategic interests across the map

The player should frequently be able to look at something functioning in the world and think:

**"I built that, and it works because of decisions I made."**

---

# 3. DESIGN PILLARS

## 3.1 Adventure feeds the settlement

The player must regularly leave home.

Important locations can include:

- ruined towns
- factories
- warehouses
- military facilities
- laboratories
- crashed vehicles or aircraft
- abandoned infrastructure
- flooded locations
- underground facilities
- hostile camps
- neutral settlements
- resource-rich wilderness
- machine graveyards
- old utility infrastructure

Exploration provides things the settlement cannot simply generate from nothing:

- weapons
- armor
- gadgets
- robot parts
- rare materials
- blueprints
- specialized tools
- technology
- recruits
- prisoners
- information
- faction opportunities
- unique machines or components

The base makes future expeditions possible.

Expeditions make the base stronger.

Neither side of the game should feel like a minigame attached to the other.

---

## 3.2 Automation is a force multiplier

Automation is one of the game's central progression fantasies.

The player should gradually stop doing repetitive labor personally and begin issuing intent to machines and workers.

Possible automated tasks include:

- hauling
- construction
- repair
- salvage
- mining
- farming
- patrols
- turret operation
- logistics
- charging
- crafting support
- corpse recovery
- expedition support
- terrain excavation
- filling and leveling
- water infrastructure operation

Automation should make the player feel more powerful, not less involved.

The player still makes important decisions:

- what to build
- what to prioritize
- where resources go
- what robots to construct
- how robots are configured
- which threats matter
- what expeditions are worth the risk
- how the settlement responds to shortages and attacks

---

## 3.3 The world is physically changeable

Terrain and water are distinguishing systems.

The environment should not feel like a decorative board that buildings sit on.

Players may:

- excavate
- fill
- flatten
- reinforce
- dam
- pump
- drain
- irrigate
- redirect
- bridge
- wall
- flood
- create defensive earthworks
- alter routes

However:

**A player who barely uses advanced excavation must still be able to succeed.**

A player who loves engineering should be rewarded with additional possibilities, efficiencies, defenses, and emergent strategies.

Terrain manipulation solves problems. It is not the game's sole objective.

---

## 3.4 Salvage creates weird technology

Recovered technology should encourage experimentation.

Robots are assembled from parts rather than appearing as perfectly standardized units from a clean technology tree.

A robot might combine:

- industrial tracks
- mining armor
- a forklift battery
- a military targeting module
- improvised plating
- a salvaged weapon
- an agricultural tool head
- mismatched sensors

And it should work.

Components can have:

- statistics
- quality
- durability
- quirks
- power requirements
- compatibility constraints
- special capabilities
- tradeoffs

Janky combinations are intentional.

Robot construction should become a major collection, experimentation, and progression system.

---

## 3.5 Combat must be fun without the base

DOWNSTREAM is also an ARPG.

Combat must stand on its own.

It needs:

- satisfying movement
- readable attacks
- strong hit feedback
- ranged and melee options
- abilities
- enemy pressure
- elites
- loot
- build variety
- meaningful gear choices
- dangerous encounters
- eventually bosses or major threats

The player should sometimes enter a dangerous location simply because fighting through it sounds fun.

Combat cannot exist only as a resource-acquisition tax.

---

## 3.6 Risk has teeth

Leaving the settlement should involve meaningful stakes.

The player carries limited inventory.

If the player dies in the field:

- carried gear remains on the corpse
- the corpse can be recovered
- recovery bots may eventually retrieve it
- AI factions may loot abandoned gear if left too long

Extraction banks recovered items into the settlement.

Taking a rare weapon into the field should feel like risking something valuable. Leaving with minimal gear should feel dangerous because the player is fragile and less capable.

The goal is tension, not cruelty.

Early regions should rarely erase an entire build or produce unrecoverable failure.

---

## 3.7 Other settlements are competitors, not decorations

AI settlements should appear to participate in the same world.

They can:

- trade
- salvage
- raid
- defend
- expand
- claim resources
- use roads
- control water
- fortify sites
- attack one another
- request help
- lose territory
- collapse

Some become allies.

Some become enemies.

Some remain useful but untrustworthy.

The player should occasionally discover that another settlement reached a valuable location first.

The world should feel inhabited even when the player is elsewhere.

---

## 3.8 Readable simulation

Systemic gameplay is only fun if the player can understand cause and effect.

The player should be able to determine:

- why water flowed somewhere
- why a robot stopped
- why a building lost power
- why hauling backed up
- why a job failed
- why a gate opened
- why a settlement became hostile
- why production stopped

Avoid black-box simulation.

Important systems require debug visualization during development and clear feedback in the final game.

---

## 3.9 Scope discipline

DOWNSTREAM is intentionally ambitious.

Systems must be proven in playable slices rather than built in isolation for months.

Do not add content simply because the architecture supports it.

Each milestone must have acceptance criteria.

A milestone is not complete because code exists. It is complete when the relevant experience is playable and understandable.

---

# 4. MAIN GAMEPLAY LOOP

## EXPLORE

Scout the world and discover:

- ruins
- threats
- settlements
- resources
- wrecks
- strange technology
- infrastructure
- shortcuts
- environmental opportunities

↓

## RAID / SCAVENGE / FIGHT

Enter dangerous areas personally, possibly with companions or machines.

Fight, sneak, salvage, rescue, capture, investigate, or steal.

↓

## EXTRACT

Decide when to leave.

Successfully extracted gear and resources enter the persistent settlement economy.

↓

## BUILD

Expand:

- shelter
- storage
- power
- workshops
- defenses
- production
- infrastructure
- housing
- utilities

↓

## ENGINEER

Construct and configure robots.

Automate jobs.

Improve logistics.

Manipulate terrain or water when useful.

↓

## SURVIVE

Respond to:

- raids
- storms
- flooding
- shortages
- equipment failure
- damaged infrastructure
- faction demands
- accidents
- opportunities

↓

## EXPAND

Reach farther regions.

Secure remote resources.

Create outposts.

Influence neighboring settlements.

Unlock increasingly dangerous expeditions.

↓

Return to exploration.

---

# 5. APPROXIMATE GAMEPLAY EMPHASIS

These percentages are not rigid production quotas.

They exist to prevent accidental overdevelopment of a single subsystem.

Approximate player-facing emphasis:

- **30% exploration and combat**
- **25% settlement building and management**
- **20% robots and automation**
- **15% factions, survival events, and world simulation**
- **10% terrain and water engineering**

Terrain and water can be technically deep even if they are not where the player spends most of their time.

Their value comes from being unusually systemic.

---

# 6. CAMERA, PRESENTATION, AND 2.5D RULES

The world uses real 3D geometry.

Gameplay is presented with a fixed isometric-style camera.

Starting camera assumptions:

- pitch approximately 50–60 degrees
- fixed yaw
- zoom in/out
- no free camera rotation during ordinary play

Gameplay simulation runs primarily on a 2D grid with height values.

Do not simulate terrain or water as unrestricted volumetric 3D systems.

Characters, enemies, robots, props, and buildings are low-poly 3D meshes.

Do not use billboard sprites as the primary presentation.

Combat uses top-down mouse targeting similar to an ARPG.

Readability from the fixed camera is more important than fine geometric detail.

---

# 7. TECHNICAL ARCHITECTURE

## 7.1 Fundamental rule: simulation is separate from rendering

Simulation state must live in plain C++ data structures wherever practical.

Rendering reads simulation state.

Rendering must not own authoritative simulation state.

Examples of simulation data:

- terrain height
- water depth
- water flow
- robot jobs
- robot status
- NPC state
- settlement state
- persistent world clock
- resource state

Benefits:

- deterministic simulation
- headless tests
- cleaner save/load
- easier debugging
- future multiplayer readiness
- less Actor overhead

Use a fixed timestep simulation.

Starting target:

**10 Hz simulation tick**

Visuals interpolate between simulation ticks.

The simulation should execute on a worker thread where practical so it cannot stall rendering.

---

## 7.2 Proposed Unreal modules

### `DownstreamSim`

Responsible for:

- core deterministic simulation
- terrain grid
- water flow
- spoil
- fixed tick
- persistent data structures
- serialization helpers
- simulation commands

No rendering dependencies.

Avoid Actor dependencies.

### `DownstreamWorld`

Responsible for:

- seeded world generation
- regions
- chunks
- biomes
- resource placement
- ruins
- settlement locations
- expedition generation hooks

### `DownstreamRender`

Responsible for:

- terrain mesh chunks
- water visualization
- simulation visualization
- fixed camera
- render interpolation

### `DownstreamBots`

Responsible for:

- robot definitions
- robot components
- robot jobs
- movement
- battery
- cargo
- tools
- robot behaviors
- job execution

### `DownstreamCombat`

Responsible for:

- player combat
- enemies
- damage
- abilities
- effects
- weapons
- armor
- loot
- affixes

Use Unreal Gameplay Ability System where appropriate.

### `DownstreamColony`

Responsible for:

- NPCs
- traits
- loyalty
- jobs
- building functionality
- production
- settlement needs

### `DownstreamSettlements`

Responsible for:

- AI settlement data
- personalities
- diplomacy
- abstract distant simulation
- raids
- trading
- expansion logic
- settlement blueprints

### `DownstreamUI`

Responsible for:

- HUD
- inventory
- building UI
- robot configuration
- job management
- terrain tools
- path drawing
- debug overlays where appropriate

Modules may evolve if profiling or implementation proves a better boundary.

Do not collapse major simulation systems into arbitrary gameplay Actors merely for convenience.

---

# 8. TERRAIN SYSTEM

Terrain is represented as a grid.

Each terrain cell should initially support:

- `GroundHeight`
- `Hardness`
- `SpoilDepth`

Possible hardness values:

- soil
- clay
- rock
- bedrock

World rendering should be chunked.

Initial suggested chunk size:

**32 × 32 cells**

Only dirty chunks should rebuild their render mesh.

Possible Unreal implementation:

- `ProceduralMeshComponent`
- or `UDynamicMeshComponent` if profiling demonstrates a need

Stylized height banding is encouraged because it improves readability.

Terrain edits originate from simulation commands.

Rendering never directly changes authoritative terrain state.

Terrain changes should invalidate navigation locally rather than forcing a global navigation rebuild.

---

# 9. WATER SYSTEM

Use a simplified shallow-water grid simulation.

Do not attempt full computational fluid dynamics.

Each relevant cell includes:

- `WaterDepth`

Water moves toward lower neighboring total surface height:

**GroundHeight + SpoilDepth + WaterDepth**

A virtual-pipe-style approach is an acceptable reference.

Requirements:

- flow conserves volume
- stable regions can sleep
- flow can wake when neighboring state changes
- source cells are supported
- sinks are supported
- map-edge drainage can be supported
- rainfall is supported
- evaporation can vary by season

Structures can modify flow between cells:

- dams
- walls
- gates
- locks
- pumps where applicable

Gates can change state at runtime.

---

## 9.1 Required developer visualization

A water debug mode must exist early.

It should display at least:

- water depth
- flow direction
- flow magnitude if useful
- total water volume

This is required for developers and eventually informs player-facing overlays.

---

## 9.2 Water events

Possible systemic events:

### Seasons

Wet and dry cycles alter water availability and evaporation.

### Storms

Temporary rainfall spikes can flood low areas.

### Flooding

Buildings, roads, storage, or defenses can be affected by standing or flowing water.

### Weaponized water

If the player floods an enemy route or location, it should happen using the same simulation rules used everywhere else.

Do not create fake special-case "flood attack" logic when the ordinary system can produce the result.

---

# 10. ROBOTICS AND AUTOMATION

This section replaces the idea that "digger bots" are the game's singular core mechanic.

Robots are a major progression system.

## 10.1 Robot construction

Robots are assembled from salvaged components.

Potential component categories:

- chassis
- locomotion
- power cell
- tool head
- armor/plating
- sensor package
- weapon
- module slots

Parts are data-driven.

Parts can modify:

- speed
- power capacity
- power consumption
- carrying capacity
- armor
- tool effectiveness
- terrain hardness capability
- combat ability
- sensor range
- special behaviors

Quirks are encouraged.

---

## 10.2 Initial robot roles

Examples include:

- hauler
- repair robot
- construction robot
- salvage robot
- scout
- combat escort
- digger
- mining unit

Do not implement every role immediately.

Build roles when milestones require them.

---

## 10.3 Job system

Jobs are persistent data objects.

A job may include:

- job type
- target
- path
- parameters
- priority
- status
- assigned robot
- resource requirements

Robots can pull jobs from a queue.

Players can:

- create
- reorder
- prioritize
- pause
- cancel

Later automation can add simple conditions.

Example:

> If reservoir level is above X, open Gate 2.

Do not build a full programming language early.

---

## 10.4 Terrain-capable robots

Excavation is one robot capability.

Possible terrain modes:

- Dig
- Fill
- Level to Height

Player interaction:

1. choose terrain operation
2. draw or designate the affected region/path
3. set depth, target height, and/or width
4. confirm
5. suitable robots execute the job

Terrain jobs interact with physical spoil.

---

## 10.5 Battery and physical constraints

Robots can have:

- battery capacity
- charge requirements
- cargo capacity
- tool limitations
- terrain hardness limits

A terrain robot's bucket fills while digging.

Spoil must go somewhere.

Possible destinations:

- designated spoil zone
- nearby dump location
- stockpile
- fill job

Spoil contributes real elevation.

It can therefore:

- block paths
- obstruct water
- create berms
- fill depressions
- become construction material

Bots can be:

- damaged
- disabled
- destroyed
- stolen by enemies where appropriate

---

# 11. COMBAT AND ARPG LAYER

Combat is top-down and mouse-aimed.

Target feel:

- fast
- responsive
- readable
- strong hit feedback
- manageable crowd pressure

Use Unreal's Gameplay Ability System for:

- abilities
- attributes
- effects
- buffs/debuffs
- cooldowns
- status systems

Player equipment initially includes:

- weapon
- armor
- gadget slots
- small ability loadout

Loot uses rarity tiers and randomized affixes.

All balance values belong in:

- Data Tables
- Data Assets

Do not hardcode tunable values.

Enemy definitions should be data-driven.

An enemy definition can include:

- stats
- ability references
- AI behavior reference
- loot table
- capture eligibility
- capture chance
- faction

---

## 11.1 Classless progression

There are no locked character classes. Use a shared branching skill tree that lets the player specialize or combine disciplines over time. The skill tree direction is approved; exact nodes, costs, and respec rules remain tuning proposals.

| Branch | Specialization and useful choices |
| --- | --- |
| Combat | Weapon handling, active combat abilities, mobility, and tactical advantages |
| Building / engineering | Construction efficiency, repair, fabrication, and robot configuration |
| Defense | Fortification, defensive coordination, turret support, and holding ground |
| Charisma / leadership | Recruitment opportunities, negotiation, morale support, and coordinating people |

Allow hybrids such as a combat engineer or a defensive settlement leader. Do not force a permanent class choice at character creation or lock ordinary weapons and basic building behind branches. Meaningful point costs should prevent mastering everything early.

Skills create options and specialization; gear supplies essential offensive and protective capability. A highly skilled character without armor must still feel squishy. Skill bonuses must not erase equipment risk or make a naked loadout perform like a geared one.

Charisma can influence willingness to join, negotiation outcomes, and loyalty support. It must respect NPC traits, faction relations, available housing, and treatment. It is not guaranteed recruitment or mind control, and ordinary recruitment remains possible without specializing in charisma.

Keep character skills distinct from settlement technology and NPC competencies. Use stable skill IDs, data-driven prerequisites and effects, and persist purchased nodes and unspent points. Exact XP sources and branch sizes are unresolved; avoid requiring repetitive grinding in every discipline.

---

## 11.2 Gear identity and loadout risk

Equipment should feel like a valuable possession rather than disposable colored stat clutter. Rare weapons need a recognizable identity through handling, sound, silhouette, capabilities, and useful tradeoffs. Affixes should support that identity rather than make rarity a flat damage multiplier.

A rare weapon should tempt the player to use it because it changes what they can accomplish. Its possible loss should influence route, destination, escort, and extraction decisions. Do not make it so expensive to operate or replace that keeping it in storage is always the best choice.

Proposed balance expectations:

- **Minimal or naked loadout:** Low protection and limited damage, reach, or utility. Small scavenging runs and careful escape remain possible, but direct fights are punishing. No hidden protection or loot advantage should make naked runs the dominant farming strategy.
- **Ordinary equipped loadout:** A practical, replaceable baseline for routine expeditions. Basic weapons and armor must remain obtainable after a loss so the player can rebuild.
- **Rare equipped loadout:** Clearly expands combat options or effectiveness, while still being vulnerable to bad decisions and valuable enough to fear losing.

Loss follows the existing corpse and recovery rules. Carried rare gear is not automatically returned because of rarity. Equipment safely stored at home is not lost because the player dies elsewhere. Communicate recovery status and any threat of faction looting; avoid unexplained item deletion.

Gear progression and skills should reinforce one another. Do not implement permanent durability loss, insurance, or repair taxes as settled requirements; those rules have not been chosen.

---

## 11.3 Capture mechanic

Some enemies can enter a subdued state at low health.

The player may choose to capture instead of kill.

A captured target may become:

- recruit
- prisoner
- bargaining asset
- faction complication

Initial implementation can remain simple.

---

# 12. EXPEDITIONS AND EXTRACTION

Expeditions are dangerous gameplay spaces containing:

- ruins
- camps
- wrecks
- industrial sites
- facilities
- special encounters

They can use smaller seeded generators combined with hand-authored prefabs.

The player has limited field inventory.

Successful extraction transfers items into the settlement's safe economy.

Death leaves carried gear on the corpse.

Recovery options may include:

- return personally
- send appropriate bots later

AI factions can eventually recover abandoned gear if it remains too long.

Early tuning should be tense but forgiving enough that new players are not repeatedly reset.

---

# 13. SETTLEMENT AND COLONY

## 13.1 NPCs

Each NPC can have:

- traits
- skills
- job preferences or competencies
- loyalty

Example skills:

- hauling
- repair
- pumping
- crafting
- construction
- defense

Initial loyalty range:

**0–100**

Loyalty can react to:

- housing
- food
- danger
- treatment
- recruitment method
- security
- major player choices

Low loyalty may produce:

- reduced work
- refusal
- sabotage
- desertion

Keep job assignment understandable.

A simple priority board is preferred to an overly complex management interface.

---

## 13.2 Buildings

Buildings use grid placement.

Each building includes:

- footprint
- placement requirements
- flatness tolerance
- resource cost
- function
- power/water requirements where relevant

Initial building set:

- shelter
- workshop
- charger
- storage
- pump
- gate
- wall
- turret

Future buildings are added when gameplay proves they are needed.

Buildings can be affected by physical world state.

Examples:

- flooding
- undermining
- loss of power
- damaged access routes

---

# 14. AI SETTLEMENTS

AI settlements are represented primarily as data.

Each settlement should have a versioned blueprint.

Potential format:

**versioned JSON**

Blueprint information can include:

- building layout
- defenses
- water infrastructure
- population
- storage
- important machines
- claimed region

Settlement personality influences behavior.

Initial personality examples:

- Trader
- Raider
- Hermit
- Dam Cult

Possible behavior:

- trade offers
- raid triggers
- demands
- expansion
- defensive construction
- territorial responses

When far from the player, settlements run at reduced fidelity.

Do not simulate every NPC and projectile across the entire world.

---

## 14.1 Multiplayer readiness

The same settlement blueprint format should eventually be capable of representing a real player's exported settlement.

A future asynchronous feature could allow another player's settlement blueprint to appear in a different player's world.

Do not build online networking now.

Only keep:

- data formats clean
- saves versioned
- simulation command-based
- authoritative state separated from presentation

---

# 15. PROCEDURAL WORLD GENERATION

World generation is seeded.

The same seed should reproduce the same untouched world.

Generation layers can include:

1. base heightmap
2. river and basin formation
3. biomes
4. resource nodes
5. ruins
6. infrastructure remnants
7. settlement locations
8. special sites

World generation should create tactically interesting geography:

- ridges
- valleys
- basins
- chokepoints
- river crossings
- elevated defensible areas
- low flood-prone areas
- isolated resource pockets

Do not generate terrain solely to showcase water.

It must also support:

- combat
- traversal
- settlement placement
- exploration
- faction territory
- resources

Expedition spaces may combine seeded generation with hand-built room, encounter, and ruin prefabs.

---

# 16. ART DIRECTION

Target style:

**stylized low-poly 3D**

Priorities:

1. readability
2. strong silhouette
3. performance
4. visual identity
5. detail

Not photorealism.

Possible visual language:

- chunky silhouettes
- limited material complexity
- clear faction shapes
- water strongly readable
- danger readable
- loot rarity readable
- machinery visibly modular

The fixed camera means geometry only needs to hold up at expected gameplay distances and angles.

---

# 17. BLENDER AND ASSET PIPELINE

Blender is the source of truth for authored 3D models.

Export into Unreal using a consistent pipeline.

Supported export choices can include:

- FBX
- glTF where appropriate

Follow consistent Unreal naming conventions.

Examples:

- `SM_` — Static Mesh
- `SK_` — Skeletal Mesh
- `M_` — Material
- `MI_` — Material Instance
- `T_` — Texture
- `BP_` — Blueprint

Maintain consistent scale and orientation.

Do not allow individual agents to invent incompatible asset conventions.

---

# 18. OPEN-SOURCE, FREE, AND THIRD-PARTY ASSETS

Open-source and free assets are allowed and encouraged when they accelerate development.

Potential sources include:

- Kenney
- Quaternius
- OpenGameArt
- Unreal Fab marketplace free content
- other reputable sources approved by the human

Preferred licensing:

- CC0
- permissive commercial-use licenses
- assets with clearly documented terms

Avoid assets that are:

- non-commercial only
- legally ambiguous
- unlicensed
- ripped from another game
- incompatible with commercial distribution
- share-alike unless the human explicitly approves the consequences

---

## 18.1 Mandatory asset license log

Every imported third-party asset, audio file, plugin, code package, or content pack must be recorded in:

`ASSET_LICENSES.md`

Record:

- asset/package name
- creator or publisher if known
- source URL
- license
- date acquired
- local project location if useful
- any required attribution

An asset is not considered properly integrated until its license is recorded.

---

## 18.2 Audio

Free audio sources may be used.

Examples include CC0 content from sites such as Freesound where licensing is clear.

Apply the same license logging requirements.

Do not assume "free download" means free for commercial use.

---

# 19. DATA-DRIVEN DESIGN

All tuning values should live in Unreal Data Assets or Data Tables wherever reasonable.

Examples:

- weapons
- enemy stats
- loot tables
- traits
- robot parts
- batteries
- tools
- terrain hardness values
- resource definitions
- building costs
- weather tuning
- faction values

Avoid magic numbers in C++.

The code should implement behavior.

Data should define tuning.

---

# 20. SAVES AND PERSISTENCE

Save format is versioned.

Simulation state serializes independently of Actors.

The world is primarily represented by:

- original world seed
- changed state
- persistent entities
- player progression

Do not save a gigantic duplicate of untouched procedural data.

---

## 20.1 Save only what changed

Store:

- world seed
- per-chunk terrain deltas
- changed water state where required
- structures
- robots
- NPCs
- jobs
- inventories
- settlement state
- faction state
- world clock
- important expedition persistence

Chunk saves should be independently manageable.

---

## 20.2 Safe writes

Autosave:

- periodically
- on orderly exit
- at major safe transitions where appropriate

Write to a temporary file first.

Then rename/replace.

Maintain rolling backups.

A crash during saving should not destroy the only valid save.

---

# 21. PERSISTENT WORLD CLOCK

The world has a persistent clock.

It can drive:

- seasons
- storms
- settlement behavior
- production
- robot jobs
- resource regeneration where appropriate
- faction actions
- event timing

---

## 21.1 Offline catch-up

When the player returns after being away, perform low-fidelity catch-up.

Initial cap:

**8 hours of game-time catch-up, accepted by the owner.**

Keep this configurable for balancing. This is an offline simulation cap, not a campaign length or an eight-hour development deadline.

Possible offline effects:

- water settles
- queued robot jobs progress or finish
- production advances
- AI settlements trade
- AI settlements raid
- abstract settlement damage occurs
- weather cycles advance

Do not simulate offline time tick-by-tick at full gameplay fidelity.

---

# 22. SERVER-AUTHORITATIVE DESIGN WITHOUT NETWORKING YET

Authoritative simulation should receive commands.

Examples:

- `CreateJob`
- `PlaceBuilding`
- `OpenGate`
- `AssignRobot`
- `StartCraft`
- `MoveItem`

Player input should not directly mutate authoritative world state from UI or rendering code.

Single-player runs the authoritative simulation locally.

This architecture exists so later networking is possible without rewriting the entire game.

Do not implement actual multiplayer yet.

---

# 23. PERFORMANCE TARGETS

Initial terrain/water target:

**256 × 256 playable simulation grid at 10 Hz**

Target rendering:

**60 FPS at 1080p on the owner’s RTX 5050, the hardware ceiling**

Validate a packaged build on this GPU during representative combat, raids, robot activity, terrain edits, and water simulation. Reduce visual or simulation costs when needed; do not raise the GPU requirement. Medium-high settings are an aspiration, not a promise that overrides the frame target.

Use measured frame times and report upscaling settings. Frame generation must not hide a failure to meet the underlying performance budget.

Planning assumptions, to verify against the actual machine:

- approximately 8 GB VRAM
- 16 GB system RAM target

Profile instead of guessing.

Use Unreal Insights at the end of each major milestone.

A milestone that badly misses the target frame budget is not complete.

---

## 23.1 Rendering guidance

Prefer:

- one main directional sun
- lightweight shadows
- inexpensive ambient lighting
- shared materials
- instancing
- texture atlases where useful

Treat the following as optional enhancements, not requirements:

- Lumen
- Nanite
- hardware ray tracing

Offer:

- DLSS where available
- Unreal TSR

Initial geometry budgets:

- characters and robots: under ~5k triangles
- buildings: under ~3k triangles
- small props: under ~1k triangles

These are starting budgets, not immutable laws.

Adjust after profiling.

Water rendering should initially be simple.

Avoid expensive screen-space reflections as a requirement.

Use instanced meshes for repeated props.

---

# 24. ACTIVE SIMULATION AND PERFORMANCE RULES

Do not update stable world regions unnecessarily.

For water:

- active cells simulate
- stable cells sleep
- changes wake relevant neighbors

For distant settlements:

- use abstract simulation

For rendering:

- rebuild only dirty chunks
- budget terrain rebuilds across frames if needed

For pathfinding:

- invalidate locally
- avoid global navigation rebuilds after every terrain edit

If bot counts become high, benchmark before selecting an architecture.

---

# 25. ACTORS VS MASS ENTITY

Benchmark the expected unit count.

If normal gameplay is under approximately 50 active robots, plain Actors may be acceptable.

If the design expects hundreds of lightweight units, evaluate Unreal Mass Entity.

Do not adopt Mass simply because it is sophisticated.

Run a real benchmark.

An early tech spike should compare approximately 100 simple moving units using candidate approaches.

Choose the simpler solution that meets the target.

---

# 26. DEBUGGING AND DEVELOPER TOOLS

Whenever an important simulation system becomes functional, give it a visualization.

Examples:

### Water

- depth
- direction
- active cells
- total volume

### Terrain

- height
- hardness
- dirty chunks
- spoil

### Robots

- current job
- job queue
- path
- battery
- cargo
- failure reason

### Settlements

- territory
- personality
- hostility
- current abstract action

### Navigation

- invalidated regions
- robot path failures

Debugging should make systemic failure explainable.

---

# 27. DEVELOPMENT MILESTONES

The milestone order is intentionally changed from v0.1.

The goal is to prove **the complete game loop early**, rather than spending the first major phase building a sophisticated digging simulator.

Do not begin the next milestone until the human signs off on the current milestone.

---

## M0 — Technical spike

Goals:

- create Unreal project
- establish module layout
- fixed 2.5D camera
- fixed simulation tick
- one simple test map
- establish source control conventions
- verify headless automation tests
- compare Actor vs Mass approach for approximately 100 simple units
- test terrain chunk mesh approach
- profile on baseline hardware

Acceptance:

- project builds cleanly
- simulation ticks independently
- camera is usable
- profiling workflow works
- basic architecture decision log exists

---

## M1 — Playable combat/exploration slice

Build:

- player movement
- fixed camera
- one ranged weapon/ability
- one melee option or ability
- two enemy types
- health and death
- hit feedback
- tiny hand-built or seeded ruin
- basic loot
- one starter classless skill choice, with a data structure that supports later branches
- return-to-home extraction point
- inventory sufficient for the slice

Acceptance:

A human can:

1. leave a tiny home area
2. enter a dangerous location
3. fight enemies
4. collect something valuable
5. return alive
6. keep the recovered item

The game must already feel like an adventure game.

---

## M2 — Tiny settlement and first useful robot

Build:

- storage
- workshop
- charger
- basic building placement
- one modular robot
- robot battery
- one non-terrain job such as hauling
- persistent save/load for the slice
- basic resource movement

Acceptance:

The human can return from an expedition with salvage, use it at the settlement, construct or improve a robot, and watch the robot perform a useful task.

---

## M3 — Terrain and water sandbox

Build:

- terrain grid
- chunk rendering
- shallow water simulation
- water conservation
- sources and sinks
- debug overlay
- manual developer raise/lower terrain tool
- simple dam
- simple gate
- save/load for terrain and water deltas

Acceptance:

A human can create a dam, watch water accumulate, alter terrain, and understand visually why the water behaves as it does.

Important:

This milestone proves the environmental simulation.

It does **not** redefine the game as being about dams.

---

## M4 — Terrain robot and systems crossover

Build:

- Dig
- Fill
- Level
- path/area designation
- spoil
- bucket/cargo rules
- terrain hardness
- robot return-to-charge behavior
- terrain-aware job queue

Then combine systems.

Acceptance:

A human can solve at least two practical settlement or expedition problems using terrain manipulation.

Examples:

- drain standing water from a useful site
- create a defensive berm
- redirect water away from a building
- create a crossing or route
- divert water toward useful infrastructure

At least one solution should be optional rather than mandatory.

---

## M5 — Vertical slice

This is the most important early milestone.

Combine:

- exploration
- combat
- extraction
- settlement
- robot automation
- terrain/water
- a simple world event

Example slice:

A valuable industrial ruin is difficult to reach or operate because of enemies and environmental conditions.

The player can:

- fight through
- recover useful technology
- return home
- build or upgrade equipment
- assign robot jobs
- optionally alter terrain/water to improve the site or settlement

Acceptance:

A new tester can play the slice and describe DOWNSTREAM as more than "the digging game."

If testers consistently describe digging as the entire game, stop and rebalance the design before expanding scope.

---

## M6 — Loot depth and extraction consequences

Build:

- rarity tiers
- randomized affixes
- more meaningful inventory decisions
- corpse gear persistence
- recovery
- additional expedition generator/prefabs
- better enemy variety
- a small classless skill tree with combat, building, defense, and charisma branches
- distinct ordinary and rare loadouts

Acceptance:

The player makes meaningful risk decisions about whether to keep exploring or extract. Compare the same encounter using minimal, ordinary, and rare equipment: minimal gear should feel fragile, ordinary gear viable, and rare gear meaningfully useful without removing danger. Verify that skill investment does not eliminate this difference and that corpse recovery preserves item identity.

---

## M7 — NPCs and capture

Build:

- capture/subdue mechanic
- NPC traits
- skills
- loyalty
- simple job priority board
- basic housing/needs interactions
- charisma interactions with recruitment and loyalty that respect NPC traits and conditions

Acceptance:

At least one captured or rescued person can become meaningfully integrated into the settlement.

---

## M8 — AI settlements

Build:

- versioned settlement blueprint format
- two initial personalities
- trade
- raid behavior
- distant abstract simulation
- basic expansion or territory behavior

Acceptance:

Another settlement takes at least one visible action that changes the player's situation without the player directly triggering a scripted quest.

---

## M9 — Automation conditions and advanced world interaction

Potential systems:

- conditional robot jobs
- water-level triggers
- gate automation
- production rules
- logistics priorities
- patrol rules

Keep interfaces understandable.

Do not turn the game into industrial programming software.

---

## M10 and beyond

Possible future areas:

- deeper factions
- seasons
- more biomes
- bosses
- vehicles
- remote outposts
- async imported player settlements
- dedicated-server investigation
- larger worlds
- mod support

None of these are commitments until earlier gameplay proves fun.

---

# 28. EXAMPLE OF SYSTEMIC GAMEPLAY

A hostile settlement controls an old hydroelectric station.

The player wants a specialized generator component stored inside.

Possible approaches could include:

- assault the front entrance
- use ranged combat from elevated terrain
- sneak through a maintenance route
- trade for access
- recruit someone with useful knowledge
- attack during another faction's raid
- send combat robots
- sabotage power
- excavate a route around a defense
- block or divert water to shut down part of the facility
- flood a low access route
- wait for a storm to change conditions

Not every option must exist in the first implementation.

The design target is that systems increasingly overlap.

The important thing is:

**The canal is not the gameplay objective. The facility is.**

Terrain is one possible strategy.

---

# 29. AI CODER INSTRUCTIONS

Every AI coding agent working on DOWNSTREAM must follow these rules.

## 29.1 Read before coding

Read this entire document before modifying architecture or implementing a milestone.

Do not infer the game's identity from one subsystem.

---

## 29.2 Do not over-focus on terrain

Terrain and water are signature mechanics, not the whole game.

Never expand the terrain system simply because additional engineering features would be interesting.

Tie new terrain work to:

- settlement gameplay
- exploration
- combat
- survival
- automation
- faction interaction

---

## 29.3 Work one milestone at a time

Do not implement later features because they are convenient or exciting.

Keep branches and commits small.

---

## 29.4 Keep simulation clean

`DownstreamSim` must remain free of presentation dependencies.

If an implementation requires authoritative simulation state inside rendering Actors, reconsider the design.

---

## 29.5 Tests are required

Write headless tests for deterministic simulation systems.

Examples:

- water conservation
- terrain edit correctness
- spoil accounting
- job execution
- save/load round trip
- deterministic seeded generation where required

---

## 29.6 Tuning is data

Put tunable numbers in Data Assets or Data Tables.

No arbitrary balance constants scattered throughout C++.

---

## 29.7 Comment why

Comments should explain:

- assumptions
- invariants
- architecture decisions
- non-obvious tradeoffs

Do not fill code with comments that merely translate obvious syntax into English.

---

## 29.8 License everything

Before committing a third-party asset or plugin, update `ASSET_LICENSES.md`.

No exceptions.

---

## 29.9 Keep the build green

Prefer:

- small commits
- descriptive messages
- compilable intermediate states
- reversible changes

Do not submit enormous speculative rewrites.

---

## 29.10 Debug visibility is part of the feature

When a simulation system works, add enough visualization to understand it.

A system that fails invisibly is not complete.

---

## 29.11 Prefer boring architecture

Readable, explicit code is preferred over clever abstraction.

Another coding agent will eventually inherit the system.

Optimize for handoff.

---

## 29.12 Ask on genuine design ambiguity

If the document leaves a major player-facing design decision unresolved, ask the human.

Do not silently invent lore, monetization, major progression structures, story direction, or irreversible architecture choices.

For implementation details that can safely be changed later, make a reasonable documented choice and proceed.

---

# 30. RISKS AND MITIGATIONS

| Risk | Mitigation |
| --- | --- |
| Game accidentally becomes a digging simulator | Maintain gameplay-emphasis targets; prove combat and settlement before terrain depth; require vertical slice |
| Water simulation becomes expensive | Active-cell simulation, sleeping, conservation tests, capped flow per tick |
| Terrain mesh rebuild causes stutter | Chunking, dirty-only rebuilds, rebuild budget per frame |
| Navigation rebuild becomes expensive | Local invalidation, consider grid pathing fallback for bots |
| Combat feels floaty | Prototype early; tune hit stop, sound, animation, screen shake, enemy reactions |
| Automation becomes tedious | Player sets intent and priorities; robots handle repetition |
| Settlement management becomes spreadsheet-heavy | Simple job priority systems; visible world feedback |
| Loot becomes meaningless stat inflation | Strong affix identity, limited useful slots, clear build tradeoffs |
| AI settlements consume too much CPU | Low-fidelity distant simulation |
| Save files grow excessively | Seed + delta architecture, chunked state |
| Asset licensing becomes a mess | Mandatory `ASSET_LICENSES.md` from the first imported asset |
| Scope creep | Human sign-off at every milestone; reject unneeded later features |
| Agents over-engineer architecture | Benchmark first; prefer simplest system meeting requirements |
| Persistent simulation punishes absence | Low-fidelity catch-up, capped offline simulation, conservative damage tuning |
| Procedural generation produces boring worlds | Visual seed review, authored prefabs, generation validation rules |

---

# 31. OUT OF SCOPE FOR NOW

Do not build these during the early milestones:

- real-time multiplayer
- voice chat
- console ports
- full modding tools
- massive story campaign
- MMO infrastructure
- advanced dedicated server support
- complicated scripting language for bots
- photoreal rendering
- unrestricted camera modes
- realistic fluid dynamics

Future compatibility is useful.

Premature implementation is not.

---

# 32. OPEN QUESTIONS FOR THE HUMAN

These questions are intentionally unresolved.

Do not block unrelated development on them.

### Tone

Should the world be:

- grim and gritty
- bleak but darkly funny
- more stylized and adventurous

### Confirmed direction

- RTX 5050 is the owner’s hardware ceiling. Do not require a stronger GPU.
- Eight hours of game-time offline catch-up is accepted.
- Progression is classless, with a shared skill tree supporting combat, building, defense, and charisma / recruitment specialization.
- Gear should feel special. Taking rare equipment out is a meaningful risk; going naked is hard and squishy.
- No authored story yet. Do not invent a campaign, protagonist backstory, dialogue pipeline, or mandatory story quests. Environmental context and emergent events can support gameplay without committing to a narrative.

### Player character progression details

Still unresolved: exact skill nodes, point economy, respec rules, XP sources, and whether implants are needed. These should be proven in small playable examples rather than treated as established requirements.

### World structure

How large should the initial persistent overworld feel before remote regions or separate expedition zones become necessary?

### Future story

Authored narrative is deferred. Revisit only after the core gameplay is proven and the owner chooses a story direction.

These can be answered after core gameplay is proven.

---

# 33. DEFINITION OF SUCCESS

DOWNSTREAM succeeds if the player can tell stories like:

> I went out looking for a replacement battery, found a military targeting module instead, barely escaped a raider camp, put the module on a stupid little salvage robot, automated scrap recovery, then used the extra material to build defenses before the next storm.

Or:

> I couldn't beat the group holding the factory, so I repaired an old pump station, drained a flooded maintenance entrance, and came in underneath them.

Or:

> A neighboring settlement kept raiding my convoy route, so I stopped using the route entirely and built an automated outpost on the other side of the ridge.

Those stories involve several systems.

That is the target.

The game should not primarily produce stories like:

> I dug a deeper canal.

Unless digging that canal changed something the player actually cared about.

---

# 34. FINAL NORTH STAR

**DOWNSTREAM is a persistent wasteland ARPG where the player raids the remains of the old world to build an automated new one, and almost anything in the environment can become part of the strategy.**

Combat matters.

Loot matters.

Robots matter.

The settlement matters.

Other settlements matter.

The persistent world matters.

Terrain and water matter.

None of them alone are the game.

The game is what happens when they interact.
