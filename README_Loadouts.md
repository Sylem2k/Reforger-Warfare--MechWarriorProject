# How mech loadouts work (and how to add a new gun)

This explains how weapons get onto the Light (Test Frame) and Medium (Warden) mechs, so you can add more
guns yourself. Everything here is done in **Workbench** - you do not need to touch the scripts to add a weapon.

---

## The short version

A weapon needs to exist in **3 places**, and all three are tied together by one text name, the **Module Id**
(for example `AUTOCANNON`). If the names don't match exactly, things break quietly.

```
 1. WEAPON PREFAB             2. EQUIPMENT CATALOG              3. EACH MECH PREFAB
 (what it is & how it shoots)  (weight, type, size - game rules)  (which guns THIS mech may use)

 Mech_Autocannon.et           Configs/RW/RW_Equipment.conf      Light_Mech_Base.et / Med_Mech_Base.et
   m_sModuleId "AUTOCANNON" ─── m_sId "AUTOCANNON" ──────────── m_aModules > m_sWeaponModuleId "AUTOCANNON"
   damage, heat, rpm, range     m_fTons 6                         m_iId 2   (number, 1-31)
   model, sound, FX             m_eType BALLISTIC                 m_iClass 1 (which hardpoint class)
                                m_eSize MEDIUM                    m_sPrefab -> Mech_Autocannon.et
```

Then the mech's **hardpoints** decide where a weapon is allowed to go.

---

## The pieces, one by one

### 1. The weapon prefab - *what the gun is*
Folder: `Prefabs/Vehicles/Mech/Weapons/...` or `Prefabs/Vehicles/Mech/Mech_Parts/...`

A small prefab with a model (`MeshObject`) and one **weapon module component** from Mech Core, e.g.:

| Component | Used for |
|---|---|
| `BFA_MechAutocannonModuleComponent` | Guns / cannons that fire shells (Autocannon) |
| `BFA_MechRocketLauncherModuleComponent` | Rocket pods / missile racks (S-13 pod) |
| `CGS_Titan_Cannon` | The Right Cannon arm |

The important fields on that component:

- `m_sModuleId` - **the Module Id**. Must match the catalog and the mech. ALL CAPS, no spaces.
- `m_sDisplayName` - name in game.
- `m_fShotsPerMinute`, `m_iMagazineSize`, `m_fReloadTime`, `m_fRange`, damage fields, `m_fHeatPerShot` - how it behaves.
- `m_MuzzlePoint` / barrel offsets - where the shots come out.
- `m_FX` - muzzle flash, sound, tracer, impact effect.
- `m_iWeaponGroup` - which fire button group (1 = primary, 2 = secondary, 3 = aux).

The Mech Lab reads damage / heat / rpm / range straight from this component, so the numbers players see are the real ones.

### 2. The equipment catalog - *the game rules for the gun*
File: `Configs/RW/RW_Equipment.conf` (one list for the whole game)

```
RW_EquipmentEntry {
 m_sId "AUTOCANNON"          <- same Module Id
 m_sDisplayName "Autocannon"
 m_eType BALLISTIC            <- ENERGY / BALLISTIC / MISSILE / MELEE / SUPPORT / ANY
 m_eSize MEDIUM               <- SMALL / MEDIUM / LARGE
 m_fTons 6                    <- weight
 m_fDamage 0                  <- optional: number to SHOW in the Mech Lab (0 = read from prefab)
 m_fRange 0                   <- optional: same for range
}
```
Rockets/missiles keep their damage on the rocket, not the launcher, so type `m_fDamage` here for those.

> If a weapon is **not** in the catalog it still works, but it has no type/size, so it fits *any* hardpoint,
> and it weighs whatever `m_fWeightTons` says on the mech's module entry. Always add it to the catalog.

### 3. The mech prefab - *which guns this mech can use*
Files: `Prefabs/Vehicles/Mech/SampleMech/Light_Mech_Base.et` and `Med_Mech_Base.et`

Two components matter:

**a) `BFA_MechHardpointComponent` (from Mech Core)**

- `m_aHardpoints` - the slots on the mech body:

  | Hardpoint | Class | Light | Medium |
  |---|---|---|---|
  | `ARM` (left arm) | 1 | yes | yes |
  | `RIGHT_ARM` | 1 | yes | yes |
  | `SHOULDER` | 2 | yes | yes |
  | `BACK` | 3 | no | yes |

- `m_aModules` - **the parts list for this mech**. Every weapon this mech is allowed to use is one entry:
  ```
  BFA_MechModuleDefinition {
   m_iId 2                       <- unique number on THIS mech, 1 to 31
   m_sLabel "Autocannon"
   m_iClass 1                    <- must equal the hardpoint class it goes on (1 arm, 2 shoulder, 3 back)
   m_sWeaponModuleId "AUTOCANNON"
   m_sPrefab "{CBBAB4D632239450}Prefabs/.../Mech_Autocannon.et"
   m_fWeightTons 0               <- ignored when the gun is in the catalog
   m_fSwapTime 3                 <- seconds to swap it on
  }
  ```

**b) `RW_MechFrameComponent` (our script)** - the frame rules:

- `m_fMaxTons` / `m_fFrameTons` - Light: 40 max, 25 empty (15 t to spend). Medium: 55 max, 35 empty (20 t).
- `m_aHardpoints` - what type and max size each hardpoint takes:

  | Hardpoint | Light | Medium |
  |---|---|---|
  | ARM | ANY, up to MEDIUM | ANY, up to LARGE |
  | RIGHT_ARM | ANY, up to MEDIUM | ANY, up to LARGE |
  | SHOULDER | SUPPORT, SMALL | SUPPORT, SMALL |
  | BACK | - | MISSILE, up to MEDIUM |

  A hardpoint that is not listed here takes anything.
- Heatsinks: each extra heatsink uses the `HEATSINK` catalog entry (1 t, 1.5 cooling).

---

## What happens when a player picks a gun

When a player puts a part on a hardpoint (Mech Lab, wrench, or Mech Bay), it is checked in this order:

1. **Class** - module `m_iClass` must match the hardpoint class. Fail = *"wrong hardpoint class"*.
2. **Type** - catalog `m_eType` must match the frame hardpoint type (ANY on either side passes). Fail = *"needs MISSILE, hardpoint is SUPPORT"*.
3. **Size** - catalog `m_eSize` must be <= the frame hardpoint `m_eMaxSize`. Fail = *"too big for this hardpoint"*.
4. **Weight** - frame tons + every part + heatsinks must be <= max tons. Fail = *"too heavy: X of Y tons"*.

The server re-checks everything, so players can't cheat past it. Heavier loads also slow the mech a bit
(up to 15% at max tons).

---

## Firing: one, some or all

This is all Mech Core, so nothing custom is needed:

- **The weapon group comes from the hardpoint** (`m_iWeaponGroup` on the hardpoint), not the weapon prefab. ARM / RIGHT_ARM = 1, SHOULDER = 2, BACK spots = 3.
- **Group fire (default):** one pull fires every weapon in the selected group.
- **Chain fire (X):** the weapons in that group take turns, one per pull.
- **B** cycles the selected group. **Right mouse** fires the aux group.
- **Rocket and missile weapons** fire **Salvo Count** rockets per pull (`m_iSalvoCount`, default 1). If a rack only fires one missile, this is the field to set.
- **To fire a hardpoint on its own key,** give it its own group number and add that group to the station's Weapon Group Mask and Cycle Weapon Groups (`BFA_MechCoreComponent`).

---

## Step by step: adding a new gun

1. **Make the weapon prefab.** In Workbench, right-click an existing weapon that shoots the way you want
   (Autocannon for guns, S13_Rocket_Pod for rockets) > **Duplicate** (or create a child prefab of it).
   Don't copy files in Windows Explorer - the copy keeps the same GUID and Workbench gets confused.
2. On its weapon component set a **new** `m_sModuleId` (e.g. `GAUSS_RIFLE`), display name, model,
   stats, muzzle point and FX.
3. **Add it to the catalog.** Open `Configs/RW/RW_Equipment.conf`, add an `RW_EquipmentEntry` with
   `m_sId` = the same id, plus type, size and tons.
4. **Add it to each mech that should get it.** Open `Light_Mech_Base.et` and/or `Med_Mech_Base.et` >
   `BFA_MechHardpointComponent` > `m_aModules` > add a `BFA_MechModuleDefinition`:
   - `m_iId` = the next free number (Light is using 1-6, Medium 1-7 right now)
   - `m_iClass` = 1 for arms, 2 for shoulder, 3 for back
   - `m_sWeaponModuleId` = the same id
   - `m_sPrefab` = your new prefab
5. Make sure the frame hardpoint rules allow it (type + size), or it will show greyed out in the Mech Lab.
6. Play test: open the Mech Lab with the Mech Wrench - the gun should be in the PARTS list.

### Rules you must not break
- **Ids 1-31 only, max 31 parts per mech.** Builds are packed into 5 bits per slot for the network.
- **Never renumber an existing `m_iId`.** Saved builds store these numbers - renumbering swaps players' guns.
  Removing one is fine; just don't reuse its number for something different.
- **Max 18 hardpoints per mech** (same network packing).
- The Module Id must be spelled **exactly** the same in all 3 places.

---

## Things in the repo right now that need cleaning up

Found while writing this - worth fixing before adding more:

| Issue | Where | Fix |
|---|---|---|
| S-13 pod uses id `S13_POD`, but its README says to use `ROCKET_LAUNCHER`. It's not in the catalog and no mech uses it. | `Weapons/S13/S13_Rocket_Pod.et` | Keep `S13_POD`, add a catalog entry (MISSILE, MEDIUM), add it to the mechs. |
| Sen Missile Launcher (`SEN_MISSILE`) is not in the catalog or on any mech. | `Weapons/SenMissile/Sen_Missile_Launcher.et` | Same as above. |
| `New_Cannon_Arm.et` and `Mini_Head_Rocket.et` both say `POINT_DEFENSE` (looks copy-pasted). | `Mech_Parts/` | Give each its own id. |
| `New_Cannon_Left.et` uses `RIGHT_CANNON`. | `Mech_Parts/` | Give it `LEFT_CANNON` if it's meant to be a separate part. |
| Catalog has entries no mech uses yet: `ARM_CANNON`, `TOP_CANNON`, `TITAN_MISSILE`, `TITAN_CANNON`, `MELEE`, `HAND_SWORD`, `FIST`, `POINT_DEFENSE`. | `RW_Equipment.conf` | Fine to keep - they're ready for when the prefabs/mechs exist. `POINT_DEFENSE` has a prefab (`Point_Defense_Module.et`) and could go on the SHOULDER now. |
| Light mech has no `BACK` hardpoint, so missile pods are Medium-only. | `Light_Mech_Base.et` | Intended? If not, add a BACK hardpoint + frame rule. |

---

## Where the code lives (if you ever need it)

| File | What it does |
|---|---|
| `Scripts/Game/RW_Equipment/RW_Equipment.c` | Loads the catalog; defines types and sizes. |
| `Scripts/Game/RW_Frame/RW_MechFrameComponent.c` | Frame rules: tonnage, hardpoint type/size checks, heatsinks, load slowdown. |
| `Scripts/Game/RW_MechLab/RW_MechLabMenu.c` | The Mech Lab screen. |
| `Scripts/Game/RW_MechLab/RW_LabStats.c` | Reads the stats shown in the Mech Lab from the weapon prefab. |
| `Scripts/Game/RW_MechLab/RW_MechLabNet.c` | Sends a build from the player to the server. |
| `Scripts/Game/RW_MechLab/RW_SavedBuilds.c` | Saved builds (per player, on the server). |
| `Scripts/Game/RW_MechBay/RW_MechBayComponent.c` | Mech Bay: order a saved build. |
| `Scripts/Game/RW_Tools/RW_MechWrenchActions.c` | Mech Wrench fit / remove / heatsink actions. |

All the `BFA_...` components (hardpoints, weapon modules, movement, aim) come from the **Mech Core**
dependency mod, not this repo.
