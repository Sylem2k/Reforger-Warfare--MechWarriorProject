//------------------------------------------------------------------------------------------------
// Warlord world boss: target distribution across virtual AI gunner stations.
// Reuses the original CGS_MechBrainHooks firing logic, weapon routing, LOS,
// faction checks, burst timing, and server-authoritative aim replication.
// The base AI applies a distance penalty to already-assigned targets; for a
// 128-player boss that still causes multiple stations to focus the same nearest
// target. This class first tries an untaken hostile and falls back to the base
// selection only when no distinct visible target is available.
//
// Virtual BOSS_LEFT / BOSS_RIGHT stations are defined on the prefab. MechBrain
// runs them without occupants. No per-frame entity search is added: this only
// runs when the existing AI station scan interval expires.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Reforger Warfare/Boss", description: "World boss multi-gunner AI: distribute gunner banks among distinct hostile targets.")]
class RW_WarlordMultiTargetAICrewComponentClass : BFA_MechAICrewComponentClass
{
}

class RW_WarlordMultiTargetAICrewComponent : BFA_MechAICrewComponent
{
    [Attribute("7", UIWidgets.Auto, "Minimum seconds to stay locked on a valid visible target. Prevents turrets switching every scan.", category: "Warlord Targeting")]
    protected float m_fMinimumTargetHoldSeconds;

    override protected IEntity FindBestTarget(BFA_MechAIStationState forState = null)
    {
        if (!forState || !m_bSpreadTargets)
            return super.FindBestTarget(forState);

        // An unchanged target needs time for heavy guns to physically traverse.
        // Keep it instead of bouncing between players at every 1.2 s scan.
        IEntity held = forState.m_pTarget;
        if (held && forState.m_fTargetTime < Math.Max(m_fMinimumTargetHoldSeconds, 0))
        {
            Faction currentFaction = GetMechFaction();
            if (currentFaction && IsValidTarget(held) && IsHostile(held, currentFaction) && HasLineOfSight(GetAimPoint(held), held))
                return held;
        }

        BaseWorld world = GetGame().GetWorld();
        Faction mechFaction = GetMechFaction();
        IEntity owner = GetOwner();
        if (!world || !mechFaction || !owner)
            return super.FindBestTarget(forState);

        float range = Math.Max(m_fEngageRange, 10.0);
        float nearestSq = range * range;
        IEntity best;

        m_aQuery.Clear();
        world.QueryEntitiesBySphere(owner.GetOrigin(), range, CollectQueryEntity, null, EQueryEntitiesFlags.DYNAMIC);

        foreach (IEntity candidate : m_aQuery)
        {
            if (!candidate || candidate == owner || candidate.GetRootParent() == owner)
                continue;

            bool soldier = ChimeraCharacter.Cast(candidate) != null;
            bool vehicle = Vehicle.Cast(candidate) != null;
            if (!soldier && !vehicle)
                continue;

            if (soldier && candidate.GetParent())
                continue;

            if (!IsAlive(candidate) || !IsHostile(candidate, mechFaction))
                continue;

            // Reserve current targets of other boss gunner stations.
            if (IsTargetTakenByOtherStation(candidate, forState))
                continue;

            float distanceSq = vector.DistanceSq(candidate.GetOrigin(), owner.GetOrigin());
            if (distanceSq >= nearestSq)
                continue;

            if (!HasLineOfSight(GetAimPoint(candidate), candidate))
                continue;

            nearestSq = distanceSq;
            best = candidate;
        }

        m_aQuery.Clear();

        if (best)
            return best;

        // Lone target: all stations may engage it. Do not leave two banks idle.
        return super.FindBestTarget(forState);
    }
}
