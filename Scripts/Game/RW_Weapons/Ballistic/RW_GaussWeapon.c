[ComponentEditorProps(category: "Reforger Warfare/Weapons", description: "Gauss rifle: pinpoint, high-velocity kinetic impact; charge cycle is governed by fire rate.")]
class RW_GaussWeaponClass : BFA_MechAutocannonModuleComponentClass
{
}
class RW_GaussWeapon : BFA_MechAutocannonModuleComponent
{
    override protected void FireShot(IEntity instigator)
    {
        vector direction;
        vector start = GetMuzzle(direction);
        IEntity hit;
        vector end = TraceShot(start, direction, m_fRange, hit);
        if (hit)
            DealDamage(hit, m_fDamage, EDamageType.KINETIC, end, direction, instigator);
        BroadcastShot(start, end, hit != null, m_fShellSpeed);
    }
}
