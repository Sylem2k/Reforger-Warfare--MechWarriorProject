//------------------------------------------------------------------------------------------------
// REFORGER WARFARE - MW LRM-5 / LRM-10 / LRM-15 / LRM-20
//
// MW4-style fire control:
//   - The server acquires a target while the gunner aims, without using ammunition.
//   - A friendly laser-designator spot takes priority over a crosshair entity lock.
//   - An uninterrupted lock is required before the trigger can launch a salvo.
//   - Pressing fire before LOCKED does NOT auto-fire later; press fire after locking.
//   - Core CGS_Titan_Missile handles homing, designation, loft, flight, replication,
//     heat, reloads and point-defense interception.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Reforger Warfare/Weapons", description: "LRM rack: acquire crosshair or friendly laser lock before firing the guided salvo.")]
class RW_LRMWeaponClass : CGS_Titan_MissileClass
{
}

class RW_LRMWeapon : CGS_Titan_Missile
{
    [Attribute("1200", UIWidgets.Auto, "Uninterrupted target acquisition time (milliseconds).", category: "MW LRM Lock")]
    protected int m_iLockTimeMs;

    protected static const int RW_LRM_LOCK_SCAN_MS = 100;

    // Server-authoritative lock. No RPC or client-controlled lock state is trusted.
    protected IEntity m_pLockTarget;
    protected CGS_LaserSpotComponent m_pLockSpot;
    protected int m_iLockStartMs;
    protected bool m_bLockReady;

    //------------------------------------------------------------------------------------------------
    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);

        if (!Replication.IsServer() || !GetGame())
            return;

        ScriptCallQueue queue = GetGame().GetCallqueue();
        if (queue)
            queue.CallLater(LRMLockTick, RW_LRM_LOCK_SCAN_MS, false);
    }

    //------------------------------------------------------------------------------------------------
    protected void ClearLRMLock()
    {
        m_pLockTarget = null;
        m_pLockSpot = null;
        m_iLockStartMs = 0;
        m_bLockReady = false;
    }

    //------------------------------------------------------------------------------------------------
    //! Probe without cycling muzzle/barrel points, consuming missiles or starting a salvo.
    //! Laser designation gets priority, as it does in Core's BOTH guidance mode.
    protected void RefreshLRMLock()
    {
        if (!Replication.IsServer() || !m_bBoundToHardpoint || !m_pMechOwner)
        {
            ClearLRMLock();
            return;
        }

        vector direction;
        vector start = GetMuzzleRaw(direction, false);
        ApplyFireAtAimPoint(start, direction);

        CGS_LaserSpotComponent candidateSpot;
        IEntity candidateTarget;

        if (m_bHoming && GuidanceUsesLaser())
        {
            candidateSpot = CGS_LaserSpotComponent.FindBestSpot(
                CGS_LaserSpotComponent.FactionOf(m_pMechOwner),
                start, direction, LaserRange(), m_fLaserConeDeg
            );

            if (candidateSpot && !candidateSpot.IsLasing())
                candidateSpot = null;
        }

        if (!candidateSpot)
        {
            IEntity hit;
            vector hitPoint;
            TraceSegment(start, start + direction * Math.Max(m_fRange, 10.0), m_pMechOwner, hit, hitPoint);

            if (hit)
            {
                candidateTarget = hit.GetRootParent();
                if (!candidateTarget)
                    candidateTarget = hit;

                if (candidateTarget == m_pMechOwner || candidateTarget == GetOwner())
                    candidateTarget = null;
            }
        }

        // Ground/sky alone is not a valid MW-style crosshair lock.
        // A friendly TAG/laser spot is a valid guided launch solution.
        if (!candidateSpot && !candidateTarget)
        {
            ClearLRMLock();
            return;
        }

        int nowMs = GetWorldTimeMs();

        // Retargeting or losing the old designation resets acquisition.
        if (candidateSpot != m_pLockSpot || candidateTarget != m_pLockTarget)
        {
            m_pLockSpot = candidateSpot;
            m_pLockTarget = candidateTarget;
            m_iLockStartMs = nowMs;
            m_bLockReady = false;
        }

        if (!m_bLockReady && nowMs - m_iLockStartMs >= Math.Max(m_iLockTimeMs, 100))
        {
            m_bLockReady = true;
            if (m_bDebug)
                Print(string.Format("[RW LRM] %1 LOCKED (%2 ms)", m_sModuleId, m_iLockTimeMs));
        }
    }

    //------------------------------------------------------------------------------------------------
    protected void LRMLockTick()
    {
        if (!Replication.IsServer() || !GetGame())
            return;

        RefreshLRMLock();

        ScriptCallQueue queue = GetGame().GetCallqueue();
        if (queue)
            queue.CallLater(LRMLockTick, RW_LRM_LOCK_SCAN_MS, false);
    }

    //------------------------------------------------------------------------------------------------
    //! Used by either trigger-down or fire-pulse: only a current, completed lock fires.
    override protected void TryStartSalvo(IEntity instigator)
    {
        if (!Replication.IsServer() || !m_bBoundToHardpoint)
            return;

        // A final server-side check prevents shooting an already-lost target.
        RefreshLRMLock();
        if (!m_bLockReady)
        {
            if (m_bDebug)
                Print(string.Format("[RW LRM] %1 fire blocked: acquiring target", m_sModuleId));
            return;
        }

        // The Core launcher still enforces cooldowns, ammo, reloads and swap state.
        super.TryStartSalvo(instigator);
    }

    //------------------------------------------------------------------------------------------------
    //! Exposes current SERVER lock for future replicated HUD integration.
    bool HasLRMLock()
    {
        return m_bLockReady;
    }

    //------------------------------------------------------------------------------------------------
    override void UnbindFromHardpoint()
    {
        ClearLRMLock();
        super.UnbindFromHardpoint();
    }

    //------------------------------------------------------------------------------------------------
    override void OnDelete(IEntity owner)
    {
        if (GetGame())
        {
            ScriptCallQueue queue = GetGame().GetCallqueue();
            if (queue)
                queue.Remove(LRMLockTick);
        }

        ClearLRMLock();
        super.OnDelete(owner);
    }
}
