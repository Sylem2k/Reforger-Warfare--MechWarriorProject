//------------------------------------------------------------------------------------------------
//! REFORGER WARFARE - EQUIPMENT. What every installable thing weighs, which hardpoint type it
//! needs and how big it is. One list for the whole game (Configs/RW/RW_Equipment.conf), so mech
//! packs never need to know about these rules.
//! An item is matched to a Core weapon by its Weapon Module Id (the m_sModuleId on the weapon
//! component, e.g. AUTOCANNON).

enum RW_EHardpointType
{
	ANY = 0,        // takes anything (frames) / fits anywhere (equipment)
	ENERGY = 1,
	BALLISTIC = 2,
	MISSILE = 3,
	MELEE = 4,
	SUPPORT = 5     // anti-missile, sensors, utility
}

enum RW_ESize
{
	SMALL = 1,
	MEDIUM = 2,
	LARGE = 3
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sId")]
class RW_EquipmentEntry
{
	[Attribute("", UIWidgets.EditBox, "Weapon Module Id this entry describes (the Module Id on the weapon component, e.g. AUTOCANNON).")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Name shown to players.")]
	string m_sDisplayName;

	[Attribute("0", UIWidgets.ComboBox, "Hardpoint type it needs.", enums: ParamEnumArray.FromEnum(RW_EHardpointType))]
	RW_EHardpointType m_eType;

	[Attribute("1", UIWidgets.ComboBox, "Size. A hardpoint takes items up to its own size.", enums: ParamEnumArray.FromEnum(RW_ESize))]
	RW_ESize m_eSize;

	[Attribute("1", UIWidgets.Auto, "Weight in tons.")]
	float m_fTons;

	[Attribute("0", UIWidgets.Auto, "Heat removed per second (heatsinks).")]
	float m_fCooling;

	[Attribute("0", UIWidgets.Auto, "Mech Lab: damage per shot to show. 0 = read it from the weapon prefab (rockets keep theirs on the rocket, so type it here).")]
	float m_fDamage;

	[Attribute("0", UIWidgets.Auto, "Mech Lab: range in metres to show. 0 = read it from the weapon prefab.")]
	float m_fRange;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class RW_EquipmentCatalog
{
	static const ResourceName DEFAULT_CATALOG = "{2ED7C46307476BA4}Configs/RW/RW_Equipment.conf";

	[Attribute("", UIWidgets.Object, "Every piece of equipment.")]
	ref array<ref RW_EquipmentEntry> m_aEquipment;

	protected static ref RW_EquipmentCatalog s_Instance;

	//------------------------------------------------------------------------------------------------
	static RW_EquipmentCatalog Get()
	{
		if (s_Instance)
			return s_Instance;

		Resource resource = BaseContainerTools.LoadContainer(DEFAULT_CATALOG);
		if (resource && resource.IsValid())
		{
			BaseContainer container = resource.GetResource().ToBaseContainer();
			if (container)
				s_Instance = RW_EquipmentCatalog.Cast(BaseContainerTools.CreateInstanceFromContainer(container));
		}

		if (!s_Instance)
		{
			Print("[RW EQUIPMENT] could not load " + DEFAULT_CATALOG + " - every weapon counts as 0 tons, any type", LogLevel.WARNING);
			s_Instance = new RW_EquipmentCatalog();
		}
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Global weapon IDs are stable string IDs (e.g. MEDIUM_LASER). Unlike Core's
	//! per-frame numeric IDs, this catalog is not restricted to 31 entries.
	//! A frame must still explicitly register a weapon in its Core module list
	//! before it can be installed. Do not renumber existing Core module IDs.
	int GetRegisteredWeaponCount()
	{
		if (!m_aEquipment)
			return 0;
		return m_aEquipment.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Resolve a global weapon ID to an installed frame's local Core module ID.
	//! Returns -1 if this frame doesn't offer that weapon.
	int ResolveLocalModuleId(BFA_MechHardpointComponent hardpoints, string globalWeaponId)
	{
		if (!hardpoints || globalWeaponId.IsEmpty())
			return -1;
		// Core currently packs module IDs into ten bits.
		for (int localId = 1; localId <= 1023; localId++)
		{
			BFA_MechModuleDefinition definition = hardpoints.GetModuleDefinition(localId);
			if (definition && definition.m_sWeaponModuleId == globalWeaponId)
				return localId;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolve an installed local ID back to the persistent global weapon ID.
	//! An empty slot is represented by an empty string.
	string ResolveGlobalWeaponId(BFA_MechHardpointComponent hardpoints, int localId)
	{
		if (!hardpoints || localId <= 0 || localId > 1023)
			return string.Empty;
		BFA_MechModuleDefinition definition = hardpoints.GetModuleDefinition(localId);
		if (!definition)
			return string.Empty;
		return definition.m_sWeaponModuleId;
	}

	//------------------------------------------------------------------------------------------------
	//! The heatsink entry (id HEATSINK): its weight and cooling are used for every extra heatsink.
	RW_EquipmentEntry GetHeatsink()
	{
		return Find("HEATSINK");
	}

	//------------------------------------------------------------------------------------------------
	//! Entry for a weapon module id, or null.
	RW_EquipmentEntry Find(string id)
	{
		if (!m_aEquipment || id.IsEmpty())
			return null;
		foreach (RW_EquipmentEntry entry : m_aEquipment)
		{
			if (entry && entry.m_sId == id)
				return entry;
		}
		return null;
	}
}
