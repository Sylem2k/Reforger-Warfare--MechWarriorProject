//------------------------------------------------------------------------------------------------
// Warlord-only shield behaviour.
//
// The shared CGS shield returns hull damage immediately while the shield is DOWN.
// As a result, its last-hit timestamp normally remains unchanged during that
// period, allowing shield regeneration even under continuing enemy fire.
//
// This boss-only subclass restarts the configured broken-shield recharge delay
// every time positive, post-armour/post-boss-cap hull damage arrives while DOWN.
// Once attackers stop dealing damage for the full broken delay, regeneration
// resumes at the prefab's normal rate.
//
// No modifications to shared MechCore or CGS Shield scripts are required.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Reforger Warfare/Boss", description: "Warlord shield: hull hits delay shield recharge while broken.")]
class RW_WarlordShieldComponentClass : CGS_MechShieldComponentClass
{
}

class RW_WarlordShieldComponent : CGS_MechShieldComponent
{
    // Called by the existing CGS shield extension for damage routed to the
    // Warlord's hull. Preserve standard shield costs, bleed, and replication.
    override float Absorb(float damage, int damageKind, IEntity shooterMech)
    {
        if (Replication.IsServer() && damage > 0 && m_bDown && !IsMechDestroyed())
        {
            m_fLastHitTime = GetTime();
            EnsureTick();
        }

        return super.Absorb(damage, damageKind, shooterMech);
    }
}
