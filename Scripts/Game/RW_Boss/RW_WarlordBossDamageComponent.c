//------------------------------------------------------------------------------------------------
// Warlord-only boss damage and shield reporting.
//
// CGS_MechBossComponent treats ALL KINETIC damage as small arms, even when a
// tank, Gauss or RPG gives a very large kinetic damage event. With a small-arms
// hard cap, a 100+ player fight can never use its separate heavy-damage budget.
// This component keeps the existing CGS caps and only reclassifies large
// KINETIC hits using the configured m_fHeavyHitMin threshold.
//
// Attach this to the Warlord in place of CGS_MechBossComponent. The existing
// CGS_MechBossComponent.Find() also finds this subclass, so the shield and
// extension hooks continue to work without modifying other mechs.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Reforger Warfare/Boss", description: "Warlord-only heavy kinetic classification and shield diagnostics.")]
class RW_WarlordBossDamageComponentClass : CGS_MechBossComponentClass
{
}

class RW_WarlordBossDamageComponent : CGS_MechBossComponent
{
    // Heavy kinetic impacts use the heavy budget instead of being limited to
    // rifle damage. Small kinetic bullets stay within the small-arms budget.
    override protected bool CountsAsSmallArms(float damage, int damageType)
    {
        if (damageType == EDamageType.KINETIC)
            return damage < m_fHeavyHitMin;

        return super.CountsAsSmallArms(damage, damageType);
    }

    // Same 10-second cadence and counters as the base boss component, but also
    // show real shield HP. This does not add any per-hit logging.
    override protected void Report(float damageIn, float damageOut)
    {
        if (!m_bDebug)
            return;

        float now = Now();
        if (m_fReportStart <= 0)
            m_fReportStart = now;

        m_fReportIn += damageIn;
        m_fReportOut += damageOut;

        float span = now - m_fReportStart;
        if (span < 10)
            return;

        SCR_DamageManagerComponent manager = SCR_DamageManagerComponent.Cast(
            GetOwner().FindComponent(SCR_DamageManagerComponent)
        );

        string hull = "?";
        if (manager && manager.GetDefaultHitZone())
            hull = string.Format("%1 / %2",
                Math.Round(manager.GetDefaultHitZone().GetHealth()),
                manager.GetDefaultHitZone().GetMaxHealth());

        string shieldText = "none";
        CGS_MechShieldComponent shield = CGS_MechShieldComponent.Find(GetOwner());
        if (shield)
        {
            string shieldState = "UP";
            if (!shield.IsUp())
                shieldState = "DOWN";

            shieldText = string.Format("%1 / %2 (%3)",
                Math.Round(shield.GetShield()),
                Math.Round(shield.GetMaxShield()),
                shieldState);
        }

        string summary = string.Format(
            "[CGS BOSS] last %1 s: %2 damage in (%3/s), %4 counted after caps (%5/s) | hull %6",
            Math.Round(span),
            Math.Round(m_fReportIn),
            Math.Round(m_fReportIn / span),
            Math.Round(m_fReportOut),
            Math.Round(m_fReportOut / span),
            hull
        );

        Print(summary + " | shield " + shieldText);

        m_fReportStart = now;
        m_fReportIn = 0;
        m_fReportOut = 0;
    }
}
