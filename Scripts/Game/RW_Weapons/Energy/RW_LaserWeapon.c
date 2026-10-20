// MechWarrior project only. Uses Mech Core firing, networking and heat handling.
[ComponentEditorProps(category: "Reforger Warfare/Weapons", description: "Configurable MW energy laser (standard or extended-range).")]
class RW_LaserWeaponClass : BFA_MechWeaponModuleComponentClass
{
}

class RW_LaserWeapon : BFA_MechWeaponModuleComponent
{
	[Attribute("850", UIWidgets.Auto, "Laser range in metres.", category: "MW Laser")]
	protected float m_fRange;

	[Attribute("32", UIWidgets.Auto, "Laser damage per pulse.", category: "MW Laser")]
	protected float m_fDamage;

	[Attribute("0", UIWidgets.Auto, "Weapon technology tier (0 = standard).", category: "MW Progression")]
	protected int m_iTechnologyTier;

	[Attribute("300", UIWidgets.Auto, "Visible tracer speed (visual only).", category: "MW Laser")]
	protected float m_fBeamSpeed;

	override protected void FireShot(IEntity instigator)
	{
		vector direction;
		vector start = GetMuzzle(direction);
		IEntity hitEntity;
		vector end = TraceShot(start, direction, m_fRange, hitEntity);
		if (hitEntity)
			// Mech Core's damage routing currently uses kinetic hit-zone damage for lasers.
			DealDamage(hitEntity, m_fDamage, EDamageType.KINETIC, end, direction, instigator);
		BroadcastShot(start, end, hitEntity != null, m_fBeamSpeed);
	}

	override float GetAimRange()
	{
		return m_fRange;
	}
}
