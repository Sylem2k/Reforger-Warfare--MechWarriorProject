// MW Streak SRM: refuses to launch without an entity lock; saves ammunition.
[ComponentEditorProps(category: "Reforger Warfare/Weapons", description: "Streak SRM requires a target under the crosshair before firing.")]
class RW_StreakSRMWeaponClass : CGS_Titan_MissileClass
{
}
class RW_StreakSRMWeapon : CGS_Titan_Missile
{
    override protected void TryStartSalvo(IEntity instigator)
    {
        if (!Replication.IsServer() || !m_bBoundToHardpoint)
            return;
        vector direction;
        vector start = GetMuzzle(direction);
        IEntity target;
        vector impact = TraceShot(start, direction, m_fRange, target);
        if (!target)
            return;
        IEntity targetRoot = target.GetRootParent();
        if (!targetRoot)
            targetRoot = target;
        if (targetRoot == m_pMechOwner)
            return;
        super.TryStartSalvo(instigator);
    }
}
