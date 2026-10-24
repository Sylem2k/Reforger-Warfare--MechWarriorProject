//------------------------------------------------------------------------------------------------
//! Arrow IV guided artillery. Core owns flight, loft, steering, live laser updates,
//! point defense interception, projectile synchronization, damage and ammunition.
//! This add-on requires a usable launch solution and supports a minimum safe range.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Reforger Warfare/Weapons", description: "Arrow IV: long-range artillery missile with laser or crosshair target solution.")]
class RW_ArrowIVWeaponClass : CGS_Titan_MissileClass
{
}

class RW_ArrowIVWeapon : CGS_Titan_Missile
{
	[Attribute("true", UIWidgets.CheckBox, "Require a laser-designated target or a traced world/entity hit before launching. Prevents empty-sky shots.", category: "MW Arrow IV")]
	protected bool m_bRequireFireSolution;

	[Attribute("175", UIWidgets.Auto, "Minimum allowed distance from launcher to designated target or crosshair impact in metres.", category: "MW Arrow IV")]
	protected float m_fMinimumTargetRange;

	[Attribute("false", UIWidgets.CheckBox, "Only launch if a friendly laser-designator spot is available. Overrides the crosshair fallback.", category: "MW Arrow IV")]
	protected bool m_bLaserOnly;

	//------------------------------------------------------------------------------------------------
	//! Invoked by either the fire-pulse or ordinary trigger-down route.
	override protected void TryStartSalvo(IEntity instigator)
	{
		if (!Replication.IsServer() || !m_bBoundToHardpoint)
			return;

		// The Core missile component handles reload, cooldown and salvo limits.
		if (m_bReloading || m_iSalvoRemaining > 0 || GetWorldTimeMs() < m_iNextTriggerTimeMs)
			return;

		vector direction;
		vector start = GetMuzzleRaw(direction, false);
		ApplyFireAtAimPoint(start, direction);

		CGS_LaserSpotComponent spot = FindLaserSpotNow();
		vector aimPoint;

		if (spot)
		{
			// Core's BOTH guidance follows this spot, even beyond terrain.
			aimPoint = spot.GetSpot();
		}
		else
		{
			if (m_bLaserOnly)
				return;

			IEntity hit;
			float fraction = TraceSegment(start, start + direction * Math.Max(m_fRange, 10.0), m_pMechOwner, hit, aimPoint);
			if (m_bRequireFireSolution && fraction >= 1.0)
				return;
		}

		if (vector.Distance(start, aimPoint) < Math.Max(m_fMinimumTargetRange, 0.0))
			return;

		// The actual projectile remains guided by the shared CGS missile implementation.
		super.TryStartSalvo(instigator);
	}
}
