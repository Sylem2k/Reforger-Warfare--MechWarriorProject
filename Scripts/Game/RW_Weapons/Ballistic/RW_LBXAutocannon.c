[ComponentEditorProps(category: "Reforger Warfare/Weapons", description: "LB-X cluster autocannon: one round launches multiple independently traced pellets.")]
class RW_LBXAutocannonClass : BFA_MechAutocannonModuleComponentClass
{
}
class RW_LBXAutocannon : BFA_MechAutocannonModuleComponent
{
    [Attribute("10", UIWidgets.Auto, "Pellets per cluster shell.", category: "MW LB-X")]
    protected int m_iPelletCount;
    [Attribute("3.0", UIWidgets.Auto, "Cluster cone angle in degrees.", category: "MW LB-X")]
    protected float m_fClusterSpread;
    override protected void FireShot(IEntity instigator)
    {
        vector direction;
        vector start = GetMuzzle(direction);
        int pellets = Math.Clamp(m_iPelletCount, 1, 32);
        float damagePerPellet = m_fDamage / pellets;
        vector visualEnd = start + direction * m_fRange;
        bool hitAnything = false;
        for (int i = 0; i < pellets; i++)
        {
            vector pelletDir = ApplyConeSpread(direction, m_fClusterSpread);
            IEntity hit;
            vector end = TraceShot(start, pelletDir, m_fRange, hit);
            if (hit)
            {
                DealDamage(hit, damagePerPellet, EDamageType.KINETIC, end, pelletDir, instigator);
                hitAnything = true;
            }
            if (i == 0)
                visualEnd = end;
        }
        // One FX broadcast per shell avoids excessive network traffic.
        BroadcastShot(start, visualEnd, hitAnything, m_fShellSpeed);
    }
}
