S-13 rocket pod (B-13 block) for MechCore

Needs the shared model files in YOUR addon, keeping their .meta files (the GUIDs):
  Common/Models/BLOCKs13+colaiders.xob (+ .meta, GUID 0542AB5CE3CDF4F6)  pod with collider
  3dmodels/s-133333.xob             (+ .meta, GUID F9E24836FE7EB615)  rocket
  3dmodels/Data/a9d5f319ada1462d89cfa7b187ad275f.emat (+ .meta)       pod material
  3dmodels/Data/8ca48d14329640139c5fe6c951585cf3.emat (+ .meta)       rocket material
  textures/su_b13_l_c.edds, textures/su_s_13_rocket_c.edds           (+ .meta) textures
If the pod GUID differs in your project, re-pick MeshObject > Object on S13_Rocket_Pod.et.

Uses only vanilla particles (UB-16 muzzle/backblast, car exhaust smoke trail, TM-62 HE blast).
Does NOT need the WCS mod.

Tune in Workbench: Barrel Points (one PointInfo per tube; replaces the legacy m_aBarrelOffsets),
Backblast Point (replaces m_vBackblastOffset), and the rocket visual's facing (+Z must point forward).
To use on a mech: m_aModules entry with m_sWeaponModuleId "ROCKET_LAUNCHER" and
m_sPrefab = S13_Rocket_Pod.et.
