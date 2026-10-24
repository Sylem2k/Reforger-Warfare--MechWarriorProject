// Warlord-only AI weapon aiming. Other MechCore/MW weapons are unchanged.
// The standard Fire At Aim Point helper only corrects weapons registered on
// BFA_MechWeaponMountComponent; boss rack hardpoints are mounted directly
// onto the root and must read the DRIVER station's replicated aim instead.
// All corrections happen on the authoritative server through normal weapon fire.
[ComponentEditorProps(category: "Reforger Warfare/Boss", description: "Warlord AI LRM: uses shared AI aim for locks and launches.")]
class RW_WarlordBossLRMClass : RW_LRMWeaponClass
{
}

class RW_WarlordBossLRM : RW_LRMWeapon
{
    override protected void ApplyFireAtAimPoint(vector start, inout vector direction)
    {
        if (!m_bFireAtAimPoint || !m_pMechOwner)
        {
            super.ApplyFireAtAimPoint(start, direction);
            return;
        }

        vector aimPoint;
        if (!GetLiveAimPoint(aimPoint))
        {
            super.ApplyFireAtAimPoint(start, direction);
            return;
        }

        vector toAim = aimPoint - start;
        if (toAim.LengthSq() <= 0.01)
            return;

        toAim.Normalize();
        // Boss launchers intentionally cover the full arc around the Titan.
        // Accuracy still depends on lock stability, missile spread and steering.
        direction = toAim;
    }
}

[ComponentEditorProps(category: "Reforger Warfare/Boss", description: "Warlord AI autocannon: fires at shared station aim with normal spread.")]
class RW_WarlordBossAutocannonClass : BFA_MechAutocannonModuleComponentClass
{
}

class RW_WarlordBossAutocannon : BFA_MechAutocannonModuleComponent
{
    override protected void ApplyFireAtAimPoint(vector start, inout vector direction)
    {
        if (!m_bFireAtAimPoint || !m_pMechOwner)
        {
            super.ApplyFireAtAimPoint(start, direction);
            return;
        }

        BFA_MechAimComponent aim = BFA_MechAimComponent.Cast(
            m_pMechOwner.FindComponent(BFA_MechAimComponent)
        );
        if (!aim)
        {
            super.ApplyFireAtAimPoint(start, direction);
            return;
        }

        vector aimPoint = m_pMechOwner.CoordToParent(
            aim.GetAimLocalPointForStation(GetControlStation())
        );
        vector toAim = aimPoint - start;
        if (toAim.LengthSq() <= 0.01)
            return;

        toAim.Normalize();
        direction = toAim;
    }
}
