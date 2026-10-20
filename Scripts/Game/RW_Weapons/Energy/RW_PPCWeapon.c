[ComponentEditorProps(category: "Reforger Warfare/Weapons", description: "PPC: concentrated energy discharge with a small impact splash.")]
class RW_PPCWeaponClass : BFA_MechWeaponModuleComponentClass
{
}
class RW_PPCWeapon : BFA_MechWeaponModuleComponent
{
    [Attribute("1200", UIWidgets.Auto, "Effective PPC range.", category: "MW PPC")]
    protected float m_fRange;
    [Attribute("90", UIWidgets.Auto, "Direct PPC damage.", category: "MW PPC")]
    protected float m_fDamage;
    [Attribute("20", UIWidgets.Auto, "Additional impact splash damage.", category: "MW PPC")]
    protected float m_fSplashDamage;
    [Attribute("2", UIWidgets.Auto, "Impact splash radius.", category: "MW PPC")]
    protected float m_fSplashRadius;
    [Attribute("300", UIWidgets.Auto, "Discharge visual speed.", category: "MW PPC")]
    protected float m_fBeamSpeed;
    override protected void FireShot(IEntity instigator)
    {
        vector direction;
        vector start = GetMuzzle(direction);
        IEntity hit;
        vector end = TraceShot(start, direction, m_fRange, hit);
        if (hit)
            DealDamage(hit, m_fDamage, EDamageType.KINETIC, end, direction, instigator);
        IEntity skipRoot;
        if (hit)
            skipRoot = hit.GetRootParent();
        DealSplashDamage(end, m_fSplashDamage, m_fSplashRadius, EDamageType.EXPLOSIVE, instigator, skipRoot);
        BroadcastShot(start, end, hit != null, m_fBeamSpeed);
    }
    override float GetAimRange()
    {
        return m_fRange;
    }
}
