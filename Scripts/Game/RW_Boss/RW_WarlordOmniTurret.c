// Warlord boss: independent 360-degree ground-defense turret.
// Reuses the proven CGS_AntiAir weapon networking, target leading, burst fire,
// and per-client turret rotation. Never runs targeting on multiplayer clients.
// Requires Mech-Core-CGS (already an MW project dependency).
[ComponentEditorProps(category: "Reforger Warfare/Boss", description: "Independent 360-degree ground defense (infantry and ground vehicles).")]
class RW_WarlordOmniTurretClass : CGS_AntiAirModuleComponentClass
{
}

class RW_WarlordOmniTurret : CGS_AntiAirModuleComponent
{
    [Attribute("10", UIWidgets.Auto, "Highest permitted ground vehicle altitude above terrain (metres).", category: "Warlord Targeting")]
    protected float m_fGroundVehicleMaxAltitude;

    [Attribute("2.5", UIWidgets.Auto, "Metres of extra target score per degree away from this mount's preferred sector. Prevents every turret from preferring the exact same player.", category: "Warlord Targeting")]
    protected float m_fSectorBias;

    [Attribute("0", UIWidgets.Auto, "0 = kinetic, 1 = incendiary (laser-like), 2 = explosive.", category: "Warlord Damage")]
    protected int m_iImpactType;

    [Attribute("6", UIWidgets.Auto, "Minimum seconds on a valid target before picking a new target.", category: "Warlord Targeting")]
    protected float m_fTargetHoldSeconds;

    protected float m_fCanRetargetAt;

    // The CGS anti-air base only gathers vehicles. Include dismounted infantry.
    override protected bool CollectEntity(IEntity entity)
    {
        if (entity && (Vehicle.Cast(entity) || ChimeraCharacter.Cast(entity)))
            s_aCollect.Insert(entity);
        return true;
    }

    override protected bool IsValidTarget(IEntity entity, Faction mechFaction)
    {
        if (!entity || !mechFaction || !m_pMechOwner)
            return false;

        if (entity == m_pMechOwner || entity.GetRootParent() == m_pMechOwner)
            return false;

        bool isVehicle = Vehicle.Cast(entity) != null;
        bool isSoldier = ChimeraCharacter.Cast(entity) != null;
        if (!isVehicle && !isSoldier)
            return false;

        // Vehicle occupants are handled by targeting the vehicle itself.
        if (isSoldier && entity.GetParent())
            return false;

        SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
        if (damage && damage.GetState() == EDamageState.DESTROYED)
            return false;

        FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
        if (!affiliation)
            return false;

        Faction targetFaction = affiliation.GetAffiliatedFaction();
        if (!targetFaction)
            targetFaction = affiliation.GetDefaultAffiliatedFaction();
        if (!targetFaction || !mechFaction.IsFactionEnemy(targetFaction))
            return false;

        vector targetPos = entity.GetOrigin();
        if (vector.Distance(targetPos, GetOwner().GetOrigin()) > m_fRange)
            return false;

        // Aircraft are handled separately by the dedicated AA turrets.
        if (isVehicle)
        {
            BaseWorld world = GetGame().GetWorld();
            if (!world)
                return false;
            float altitude = targetPos[1] - world.GetSurfaceY(targetPos[0], targetPos[2]);
            if (altitude > m_fGroundVehicleMaxAltitude)
                return false;
        }

        return true;
    }

    // Pick targets by distance AND sector. Each hardpoint has its own index,
    // so flanking players don't all get ignored in favor of the nearest one.
    override protected IEntity FindTarget()
    {
        BaseWorld world = GetGame().GetWorld();
        Faction ourFaction = GetMechFaction();
        if (!world || !ourFaction || !GetOwner())
            return null;

        float now = Now();
        // Don't chase a different nearby player each scan. This also gives the
        // existing CGS yaw/pitch animation time to traverse to the target.
        if (m_pTarget && now < m_fCanRetargetAt && IsValidTarget(m_pTarget, ourFaction))
            return m_pTarget;

        s_aCollect.Clear();
        world.QueryEntitiesBySphere(GetOwner().GetOrigin(), m_fRange, CollectEntity, null, EQueryEntitiesFlags.DYNAMIC);

        int sector = Math.Mod(Math.Max(m_iHardpointIndex, 0) * 3 + 1, 8);
        float sectorYaw = sector * 45.0 - 180.0;
        IEntity best;
        float bestScore = m_fRange * 10.0;
        vector pivot = GetOwner().GetOrigin();

        foreach (IEntity candidate : s_aCollect)
        {
            if (!IsValidTarget(candidate, ourFaction))
                continue;

            vector worldDirection = candidate.GetOrigin() - pivot;
            vector localDirection = m_pMechOwner.VectorToLocal(worldDirection);
            float yaw = Math.Atan2(localDirection[0], localDirection[2]) * Math.RAD2DEG;
            float delta = yaw - sectorYaw;
            while (delta > 180)
                delta -= 360;
            while (delta < -180)
                delta += 360;

            float distance = worldDirection.Length();
            float score = distance + Math.AbsFloat(delta) * Math.Max(m_fSectorBias, 0);
            if (score < bestScore)
            {
                best = candidate;
                bestScore = score;
            }
        }

        s_aCollect.Clear();
        if (best)
            m_fCanRetargetAt = now + Math.Max(m_fTargetHoldSeconds, 0);
        return best;
    }

    // The original AA module always deals explosive damage; allow true cannon
    // and energy-like profiles while keeping its authoritative blast scheduling.
    override protected void Detonate(vector position, IEntity instigator, int token)
    {
        if (!Replication.IsServer() || token != m_iShellToken)
            return;

        if (m_iImpactType == 1)
            DealSplashDamage(position, m_fBurstDamage, m_fBurstRadius, EDamageType.INCENDIARY, instigator);
        else if (m_iImpactType == 2)
            DealSplashDamage(position, m_fBurstDamage, m_fBurstRadius, EDamageType.EXPLOSIVE, instigator);
        else
            DealSplashDamage(position, m_fBurstDamage, m_fBurstRadius, EDamageType.KINETIC, instigator);
    }
}
