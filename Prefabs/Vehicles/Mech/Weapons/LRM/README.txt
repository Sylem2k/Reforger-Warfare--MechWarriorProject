LRM racks (Catapult-style back missiles) for MechWarrior Project
Built on Mech Core's CGS_Titan_Missile (homing rocket launcher). One prefab = ONE rack.

  Prefab      Module Id  Tubes (grid)  Size    Tons  Fits on
  LRM5.et     LRM5        5  (5 x 1)   SMALL   2     Light + Medium back spots
  LRM10.et    LRM10      10  (5 x 2)   MEDIUM  4     Light + Medium back spots
  LRM15.et    LRM15      15  (5 x 3)   LARGE   6     Medium back spots
  LRM20.et    LRM20      20  (5 x 4)   LARGE   8     Medium back spots
Weight / size live in Configs/RW/RW_Equipment.conf. Light Frame B lists LRM5 + LRM10,
Medium Frame B lists all four, so e.g. one LRM15 on BACK_L and one LRM20 on BACK_R.

FIRING - one, some or all (all Mech Core, nothing custom)
  - One trigger pull = the whole rack ripples out (Salvo Count = tube count, 60 ms apart).
  - BACK_L and BACK_R are both weapon group 3, so:
      Group fire (default)  : one pull fires BOTH racks.
      Chain fire (X)        : the racks take turns, one rack per pull.
      B                     : cycle groups (1 arms, 2 shoulder, 3 back) to pick what to fire.
  - Want each rack on its own key? Give BACK_R its own group (e.g. 4) on the mech's hardpoint and
    add 4 to the DRIVER station's Weapon Group Mask / Cycle Weapon Groups (BFA_MechCoreComponent).
  - Ammo: magazine = 4 volleys (LRM5/10) or 3 volleys (LRM15/20), 2 spare magazines.
  - Heat is charged PER MISSILE (Heat Per Shot 1): an LRM20 volley = 20 heat.

GUIDANCE (Homing category)
  m_eGuidance BOTH    - flies to a friendly Target Designator spot if there is one in front
                        (inside m_fLaserConeDeg 120), otherwise locks what the crosshair is on.
  m_bLockTargets 1    - follows the locked mech / vehicle while it moves.
  m_fLoftAngle 45     - arcs up, then dives on the target (top attack).
  m_fTurnRate 45      - low = dodgeable. m_fArmingDistance 60 = flies straight 60 m first.
  To designate: TARGET_DESIGNATOR on the SHOULDER (toggle on), or a teammate's designator.
  Enemy point defense can shoot LRMs down.

WHEN THE MODEL IS DONE
  1. MeshObject > Object: your rack model (placeholder is Left_Right_4_Pod.xob).
  2. Tubes > Tube Grids > First Tube: drag onto the TOP-LEFT tube mouth. Blue (Z) arrow out of
     the tube, red (X) right, green (Y) up.
  3. Column Spacing / Row Spacing: distance to the next tube right / down (placeholder 0.12).
     Change Columns / Rows if your layout differs - keep Salvo Count = Columns x Rows.
  4. Check: tick Debug, fire, watch where the missiles start; adjust until they leave the tubes.
  5. Muzzle Point: centre of the rack (used for the crosshair / aim).

DAMAGE (per missile): 60 direct, 25 splash, 4 m radius. Range 1800 m, speed 90 m/s.
