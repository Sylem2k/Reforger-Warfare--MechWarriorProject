OPFOR AUTONOMOUS MECH TEST ROSTER
=================================
16 standalone hostile AI prefabs, 4 in each weight class.
GM entries: search "OPFOR Cricket", "OPFOR Warden", "OPFOR Judiciar", or
"OPFOR Gravemarch". Register in Configs/Editor/PlaceableEntities/Vehicles/EnemyMechs.conf.

Core AI: CGS_MechBrainComponent + existing BFA_MechAICrewComponent. Brain drives
and fires unattended with HUNT / FIRE_AT_WILL. This setup does NOT spawn an AI
driver/hidden pilot, making it relatively lightweight but without selectable GM
pilot waypoints. It searches for enemies and chases them within its hunt/leash
ranges; the enemy's vehicle FactionAffiliation is USSR, so it should fight US
and other hostile forces in a scenario with normal faction hostility.

IMPORTANT: Faction key "USSR" must exist in the mission. If your faction
manager lacks USSR or your player faction is not hostile to it, these
AI will not select players as targets. The AI brain prints a warning if
faction resolution fails. Change BOTH the brain faction and faction
affiliation in each prefab to the appropriate actual hostile faction.

This uses the existing mech bodies, limbs and mount PointInfo positions as
temporary visual models. New meshes will be added later. No Mech Core changes.
Modules are selected in each BFA_MechHardpointDefinition's
m_iDefaultModuleId; module IDs are specific to the source chassis.
Every default build was statically checked against RW_Equipment.conf,
frame max tonnage, listed hardpoint sizes and restrictions. No new scripts.

CLASS    ENEMY VARIANT          ROLE / FOCUS             MASS / LIMIT
LIGHT    Cricket - Dart        Lasers + SRM2             33 / 40
LIGHT    Cricket - Marksman    AC2 + Laser + LRM5        34 / 40
LIGHT    Cricket - Pulse       Pulse Lasers + SRM4       36 / 40
LIGHT    Cricket - Longshot    ER Lasers + LRM10/5       36 / 40
MEDIUM   Warden - Linebreaker  AC5 + Laser + SRMs        50 / 55
MEDIUM   Warden - Arc          PPC + ER Laser + missiles 49.5 / 55
MEDIUM   Warden - Storm        ER Laser + LRM15/10       49 / 55
MEDIUM   Warden - Breacher     AC10 + laser + SRMs       53 / 55
HEAVY    Judiciar - Ironwall   AC10 + PPC + AC5 + SRM2   78 / 80
HEAVY    Judiciar - Bastion    LBX + ER Laser + AC5      77.5 / 80
HEAVY    Judiciar - Rainfall   ER Lasers + five LRMs     74 / 80
HEAVY    Judiciar - Lancer     PPC + mixed LRM/SRM       77 / 80
ASSAULT  Gravemarch - Breaker  AC20 + PPC + AC10         99 / 100
ASSAULT  Gravemarch - Siege    Heavy Gauss + ER PPC      99 / 100
ASSAULT  Gravemarch - Tempest  ER Lasers + multiple LRM  99 / 100
ASSAULT  Gravemarch - Reaper   PPC + AC5 + mixed rockets 100 / 100

MP TESTING:
1. Build and compile with CURRENT Mech Core CGS dependency enabled.
2. In GM search for "OPFOR" and place ONE Cricket. Confirm USSR faction.
3. Watch movement/aim/firing on dedicated server. Try 1v1, then 2v2.
4. Check each variant's default modules are visible and fire; check
   rocket lock behavior and faction relationships.
5. Ramp to 4-8 spawned mechs while checking FPS. All sixteen active at
   once are NOT advisable for the initial test; these drive, scan for
   targets and fire multiple weapon modules.
6. No dedicated server compile or real GM placement test has been run
   yet. Treat all AI combat behavior as pending validation.
