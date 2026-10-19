//------------------------------------------------------------------------------------------------
//! REFORGER WARFARE - MECH LAB part stats (damage, heat, fire rate, range).
//! Read from the part's weapon prefab (its BFA weapon module component), so the numbers are
//! always the real ones. Damage / range typed in RW_Equipment.conf win over the prefab
//! (needed for weapons whose damage lives on the rocket / shell, not on the weapon).
class RW_LabStats
{
	float m_fDamage = -1;   // per shot, -1 = unknown
	float m_fHeat = -1;     // per shot
	float m_fRpm = -1;      // shots per minute
	float m_fRange = -1;    // metres

	protected static ref map<string, ref RW_LabStats> s_mCache;
	protected static ref RW_LabStats s_Unknown;

	//------------------------------------------------------------------------------------------------
	//! Heat per second while firing non-stop (-1 = unknown).
	float HeatPerSecond()
	{
		if (m_fHeat < 0 || m_fRpm <= 0)
			return -1;
		return m_fHeat * m_fRpm / 60;
	}

	//------------------------------------------------------------------------------------------------
	//! One line for the parts list, e.g. "DMG 40 · HEAT 3 · 120/min · 800 m".
	string Line()
	{
		string line = "DMG " + Num(m_fDamage);
		line = line + "  |  HEAT " + Num(m_fHeat);
		if (m_fRpm > 0)
			line = line + "  |  " + Num(m_fRpm) + "/min";
		if (m_fRange > 0)
			line = line + "  |  " + Num(m_fRange) + " m";
		return line;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Num(float value)
	{
		if (value < 0)
			return "-";
		return (Math.Round(value * 10) / 10).ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Stats for a module on this mech (cached per prefab + id).
	static RW_LabStats For(BFA_MechHardpointComponent hardpoints, int moduleId)
	{
		if (!s_Unknown)
			s_Unknown = new RW_LabStats();
		if (!hardpoints || moduleId <= 0)
			return s_Unknown;
		BFA_MechModuleDefinition module = hardpoints.GetModuleDefinition(moduleId);
		if (!module)
			return s_Unknown;

		if (!s_mCache)
			s_mCache = new map<string, ref RW_LabStats>();
		string key = module.m_sPrefab + "|" + module.m_sWeaponModuleId;
		RW_LabStats cached = s_mCache.Get(key);
		if (cached)
			return cached;

		RW_LabStats stats = new RW_LabStats();

		// 1) The weapon prefab.
		if (!module.m_sPrefab.IsEmpty())
		{
			Resource resource = Resource.Load(module.m_sPrefab);
			if (resource && resource.IsValid())
				stats.ReadEntitySource(resource.GetResource().ToEntitySource(), 0);
		}

		// 2) Bound weapons with no prefab: the weapon already on the mech with that module id.
		if (stats.m_fHeat < 0)
		{
			for (int i = 0; i < hardpoints.GetHardpointCount(); i++)
			{
				BFA_MechWeaponModuleComponent weapon = hardpoints.GetInstalledModuleAt(i);
				if (weapon && weapon.GetModuleId() == module.m_sWeaponModuleId)
				{
					stats.m_fHeat = weapon.GetHeatPerShot();
					if (stats.m_fRange < 0)
						stats.m_fRange = weapon.GetAimRange();
					break;
				}
			}
		}

		// 3) Catalog overrides.
		RW_EquipmentEntry item = RW_EquipmentCatalog.Get().Find(module.m_sWeaponModuleId);
		if (item)
		{
			if (item.m_fDamage > 0)
				stats.m_fDamage = item.m_fDamage;
			if (item.m_fRange > 0)
				stats.m_fRange = item.m_fRange;
		}

		s_mCache.Set(key, stats);
		return stats;
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadEntitySource(IEntitySource source, int depth)
	{
		if (!source || depth > 2)
			return;

		for (int c = 0; c < source.GetComponentCount(); c++)
		{
			IEntityComponentSource component = source.GetComponent(c);
			if (!component)
				continue;
			typename type = component.GetClassName().ToType();
			if (!type || !type.IsInherited(BFA_MechWeaponModuleComponent))
				continue;

			float value;
			if (m_fHeat < 0 && component.Get("m_fHeatPerShot", value))
				m_fHeat = value;
			if (m_fRpm < 0 && component.Get("m_fShotsPerMinute", value))
				m_fRpm = value;
			if (m_fDamage < 0 && component.Get("m_fDamage", value))
				m_fDamage = value;
			if (m_fRange < 0 && component.Get("m_fRange", value))
				m_fRange = value;
			return;
		}

		for (int k = 0; k < source.GetNumChildren(); k++)
			ReadEntitySource(source.GetChild(k), depth + 1);
	}
}
