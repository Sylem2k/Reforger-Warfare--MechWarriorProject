//------------------------------------------------------------------------------------------------
//! REFORGER WARFARE - MECH FRAME. Put this on a mech prefab (built with Mech Core) to make it a
//! frame in this game: its own name, weight class, how many tons it can carry, and what each of
//! its hardpoints accepts.
//!  - Tonnage: Frame Tons (the empty mech) + every installed item (RW_Equipment.conf) must stay
//!    within Max Tons. Loadouts that are too heavy are refused.
//!  - Hardpoints: list the Core hardpoint names with a type (Energy, Ballistic, Missile, Melee,
//!    Support or Any) and a max size. Items that don't fit are refused. Unlisted hardpoints
//!    take anything.
//!  - Speed: set top speed on the Core movement component as usual; carrying a heavy load slows
//!    the frame a little (Load Slowdown).
[ComponentEditorProps(category: "Reforger Warfare", description: "Mech frame: name, weight class, max tonnage, hardpoint types and sizes.")]
class RW_MechFrameComponentClass : ScriptComponentClass
{
}

enum RW_EWeightClass
{
	LIGHT = 0,
	MEDIUM = 1,
	HEAVY = 2,
	ASSAULT = 3
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sHardpoint")]
class RW_FrameHardpoint
{
	[Attribute("", UIWidgets.EditBox, "Core hardpoint name (Hardpoint component > Name), e.g. HAND_L.")]
	string m_sHardpoint;

	[Attribute("0", UIWidgets.ComboBox, "What it accepts. ANY = everything.", enums: ParamEnumArray.FromEnum(RW_EHardpointType))]
	RW_EHardpointType m_eType;

	[Attribute("2", UIWidgets.ComboBox, "Largest item it takes.", enums: ParamEnumArray.FromEnum(RW_ESize))]
	RW_ESize m_eMaxSize;
}

//------------------------------------------------------------------------------------------------
class RW_MechFrameComponent : ScriptComponent
{
	//! True while a loadout is applied to a mech parked in a Mech Bay (frames with Swap Only At
	//! Mech Bay refuse every other swap).
	static bool s_bMechBayApplying;

	//! Every frame in the world (bays look through this list instead of scanning an area).
	protected static ref array<RW_MechFrameComponent> s_aAllFrames;

	[Attribute("Unnamed Frame", UIWidgets.EditBox, "Frame name shown to players. Use your own names.", category: "Frame")]
	protected string m_sFrameName;

	[Attribute("1", UIWidgets.ComboBox, "Weight class (for spawn limits, the mech bay and AI).", enums: ParamEnumArray.FromEnum(RW_EWeightClass), category: "Frame")]
	protected RW_EWeightClass m_eWeightClass;

	[Attribute("50", UIWidgets.Auto, "Most the frame may weigh with everything installed, in tons.", category: "Tonnage")]
	protected float m_fMaxTons;

	[Attribute("30", UIWidgets.Auto, "Weight of the empty frame (body, legs, engine, armour), in tons. Max Tons minus this is what it can carry.", category: "Tonnage")]
	protected float m_fFrameTons;

	[Attribute("", UIWidgets.Object, "What each hardpoint accepts. Hardpoints not listed take anything.", category: "Hardpoints")]
	protected ref array<ref RW_FrameHardpoint> m_aHardpoints;

	[Attribute("0", UIWidgets.Auto, "Extra heatsinks the frame starts with (the mech bay changes the count). Each one weighs and cools as the HEATSINK entry in RW_Equipment.conf says. The frame's built-in cooling is the Core heat component's Cooling Per Second.", category: "Heat")]
	protected int m_iStartExtraHeatsinks;

	// Extra heatsinks installed right now (server owned, replicated).
	[RplProp()]
	protected int m_iExtraHeatsinks;

	// Last loadout message for the crew HUD (server sets it, everyone sees it).
	[RplProp()]
	protected string m_sCrewMessage;
	[RplProp()]
	protected int m_iCrewMessageSeq;

	[Attribute("20", UIWidgets.Auto, "Most extra heatsinks this frame can take.", category: "Heat")]
	protected int m_iMaxExtraHeatsinks;

	[Attribute("false", UIWidgets.CheckBox, "Loadouts can only be changed at a mech bay. Leave off until the mech bay exists (field swaps with R are allowed while off).", category: "Rules")]
	protected bool m_bSwapOnlyAtMechBay;

	[Attribute("0.15", UIWidgets.Slider, "Speed lost when fully loaded (0.15 = 15% slower at max tons).", "0 0.5 0.01", category: "Movement")]
	protected float m_fLoadSlowdown;

	[Attribute("0.6", UIWidgets.Slider, "Share of the carry capacity used before the frame starts slowing down.", "0 1 0.05", category: "Movement")]
	protected float m_fSlowdownStart;

	//------------------------------------------------------------------------------------------------
	static array<RW_MechFrameComponent> GetAll()
	{
		if (!s_aAllFrames)
			s_aAllFrames = {};
		return s_aAllFrames;
	}

	//------------------------------------------------------------------------------------------------
	static RW_MechFrameComponent Find(IEntity mech)
	{
		if (!mech)
			return null;
		return RW_MechFrameComponent.Cast(mech.FindComponent(RW_MechFrameComponent));
	}

	string GetFrameName()
	{
		return m_sFrameName;
	}

	RW_EWeightClass GetWeightClass()
	{
		return m_eWeightClass;
	}

	float GetMaxTons()
	{
		return m_fMaxTons;
	}

	float GetFrameTons()
	{
		return m_fFrameTons;
	}

	int GetExtraHeatsinks()
	{
		return m_iExtraHeatsinks;
	}

	int GetMaxExtraHeatsinks()
	{
		return m_iMaxExtraHeatsinks;
	}

	bool SwapOnlyAtMechBay()
	{
		return m_bSwapOnlyAtMechBay;
	}

	string GetCrewMessage()
	{
		return m_sCrewMessage;
	}

	int GetCrewMessageSeq()
	{
		return m_iCrewMessageSeq;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tell the crew something (shown on their HUD for a few seconds, and in the log).
	void ServerCrewMessage(string message)
	{
		if (!Replication.IsServer())
			return;
		m_sCrewMessage = message;
		m_iCrewMessageSeq++;
		Replication.BumpMe();
		Print(string.Format("[RW FRAME] %1: %2", m_sFrameName, message));
	}

	//------------------------------------------------------------------------------------------------
	//! Weight of the extra heatsinks.
	float HeatsinkTons(int count)
	{
		RW_EquipmentEntry sink = RW_EquipmentCatalog.Get().GetHeatsink();
		if (!sink)
			return 0;
		return Math.Max(count, 0) * Math.Max(sink.m_fTons, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Extra heat removed per second by the extra heatsinks.
	float ExtraCooling()
	{
		RW_EquipmentEntry sink = RW_EquipmentCatalog.Get().GetHeatsink();
		if (!sink)
			return 0;
		return Math.Max(m_iExtraHeatsinks, 0) * Math.Max(sink.m_fCooling, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server (Mech Lab / mech bay): apply a whole build at once - weapons per hardpoint and the
	//! extra heatsink count. Checked against the frame rules; the weapons swap with the normal timer.
	bool ServerApplyBuild(notnull array<int> modules, int heatsinks, out string reason)
	{
		reason = string.Empty;
		if (!Replication.IsServer())
			return false;
		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(GetOwner().FindComponent(BFA_MechHardpointComponent));
		if (!hardpoints)
		{
			reason = "this mech has no hardpoints";
			return false;
		}
		if (hardpoints.IsSwapInProgress())
		{
			reason = "already working on it";
			return false;
		}
		if (!CheckBuild(hardpoints, modules, heatsinks, reason))
			return false;

		int oldHeatsinks = m_iExtraHeatsinks;
		m_iExtraHeatsinks = heatsinks;

		s_bMechBayApplying = RW_MechBayComponent.IsAtBay(GetOwner());
		bool ok = hardpoints.ServerRequestLoadout(hardpoints.m_sSwapStation, modules);
		s_bMechBayApplying = false;

		if (!ok)
		{
			m_iExtraHeatsinks = oldHeatsinks;
			if (reason.IsEmpty())
				reason = "the mech must stand still";
			return false;
		}
		Replication.BumpMe();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server (mech bay): set the extra heatsink count. Refused if it breaks the tonnage.
	bool ServerSetExtraHeatsinks(int count, out string reason)
	{
		reason = string.Empty;
		if (!Replication.IsServer())
			return false;

		count = Math.Clamp(count, 0, Math.Max(m_iMaxExtraHeatsinks, 0));
		int old = m_iExtraHeatsinks;
		m_iExtraHeatsinks = count;
		float tons = CurrentTons();
		if (tons > m_fMaxTons + 0.001)
		{
			m_iExtraHeatsinks = old;
			reason = string.Format("too heavy: %1 of %2 tons", tons, m_fMaxTons);
			return false;
		}
		Replication.BumpMe();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Rules for one hardpoint (null = takes anything).
	RW_FrameHardpoint FindHardpoint(string name)
	{
		if (!m_aHardpoints)
			return null;
		foreach (RW_FrameHardpoint hp : m_aHardpoints)
		{
			if (hp && hp.m_sHardpoint == name)
				return hp;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Weight of one module (catalog first, else the module's own Weight Tons).
	static float ModuleTons(BFA_MechModuleDefinition module)
	{
		if (!module)
			return 0;
		RW_EquipmentEntry entry = RW_EquipmentCatalog.Get().Find(module.m_sWeaponModuleId);
		if (entry)
			return Math.Max(entry.m_fTons, 0);
		return Math.Max(module.m_fWeightTons, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Total weight of a loadout (frame + every module), for the HUD and the mech bay.
	float LoadoutTons(BFA_MechHardpointComponent hardpoints, array<int> modules)
	{
		return BuildTons(hardpoints, modules, m_iExtraHeatsinks);
	}

	//------------------------------------------------------------------------------------------------
	//! Weight of a planned build: frame + modules + extra heatsinks (the Mech Lab uses this).
	float BuildTons(BFA_MechHardpointComponent hardpoints, array<int> modules, int heatsinks)
	{
		float total = m_fFrameTons + HeatsinkTons(heatsinks);
		if (!hardpoints || !modules)
			return total;
		foreach (int id : modules)
			total += ModuleTons(hardpoints.GetModuleDefinition(id));
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Weight right now.
	float CurrentTons()
	{
		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(GetOwner().FindComponent(BFA_MechHardpointComponent));
		array<int> modules = {};
		if (hardpoints)
		{
			for (int i = 0; i < hardpoints.GetHardpointCount(); i++)
				modules.Insert(hardpoints.GetModuleAt(i));
		}
		return LoadoutTons(hardpoints, modules);
	}

	//------------------------------------------------------------------------------------------------
	//! Is this loadout allowed? reason says why not (shown in the log / to the player).
	bool CheckLoadout(BFA_MechHardpointComponent hardpoints, notnull array<int> modules, out string reason)
	{
		return CheckBuild(hardpoints, modules, m_iExtraHeatsinks, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Can this module go on this hardpoint at all (class, type, size)? Tonnage not included.
	bool CheckItem(BFA_MechHardpointComponent hardpoints, int hardpointIndex, int moduleId, out string reason)
	{
		reason = string.Empty;
		if (!hardpoints || moduleId == 0)
			return true;
		if (!hardpoints.CanModuleFit(hardpointIndex, moduleId))
		{
			reason = "wrong hardpoint class";
			return false;
		}

		BFA_MechModuleDefinition module = hardpoints.GetModuleDefinition(moduleId);
		RW_FrameHardpoint rules = FindHardpoint(hardpoints.GetHardpointName(hardpointIndex));
		RW_EquipmentEntry item;
		if (module)
			item = RW_EquipmentCatalog.Get().Find(module.m_sWeaponModuleId);
		if (!rules || !item)
			return true;

		if (rules.m_eType != RW_EHardpointType.ANY && item.m_eType != RW_EHardpointType.ANY && item.m_eType != rules.m_eType)
		{
			reason = string.Format("needs %1, hardpoint is %2", typename.EnumToString(RW_EHardpointType, item.m_eType), typename.EnumToString(RW_EHardpointType, rules.m_eType));
			return false;
		}
		if (item.m_eSize > rules.m_eMaxSize)
		{
			reason = "too big for this hardpoint";
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Is this planned build allowed (items + tonnage with this many extra heatsinks)?
	bool CheckBuild(BFA_MechHardpointComponent hardpoints, notnull array<int> modules, int heatsinks, out string reason)
	{
		reason = string.Empty;
		if (!hardpoints)
			return true;
		if (heatsinks < 0 || heatsinks > m_iMaxExtraHeatsinks)
		{
			reason = string.Format("heatsinks must be 0 - %1", m_iMaxExtraHeatsinks);
			return false;
		}

		RW_EquipmentCatalog catalog = RW_EquipmentCatalog.Get();
		for (int i = 0; i < modules.Count(); i++)
		{
			BFA_MechModuleDefinition module = hardpoints.GetModuleDefinition(modules[i]);
			if (!module)
				continue; // empty

			RW_FrameHardpoint rules = FindHardpoint(hardpoints.GetHardpointName(i));
			RW_EquipmentEntry item = catalog.Find(module.m_sWeaponModuleId);
			if (!rules || !item)
				continue; // unlisted hardpoint or unknown item: no type / size rule

			if (rules.m_eType != RW_EHardpointType.ANY && item.m_eType != RW_EHardpointType.ANY && item.m_eType != rules.m_eType)
			{
				reason = string.Format("%1 does not fit %2 (needs %3, hardpoint is %4)", ItemName(item, module), rules.m_sHardpoint, typename.EnumToString(RW_EHardpointType, item.m_eType), typename.EnumToString(RW_EHardpointType, rules.m_eType));
				return false;
			}

			if (item.m_eSize > rules.m_eMaxSize)
			{
				reason = string.Format("%1 is too big for %2", ItemName(item, module), rules.m_sHardpoint);
				return false;
			}
		}

		float tons = BuildTons(hardpoints, modules, heatsinks);
		if (tons > m_fMaxTons + 0.001)
		{
			reason = string.Format("too heavy: %1 of %2 tons", tons, m_fMaxTons);
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string ItemName(RW_EquipmentEntry item, BFA_MechModuleDefinition module)
	{
		if (item && !item.m_sDisplayName.IsEmpty())
			return item.m_sDisplayName;
		return module.m_sLabel;
	}

	//------------------------------------------------------------------------------------------------
	//! Speed multiplier from the load: 1 until Slowdown Start of the carry capacity is used, then
	//! down to 1 - Load Slowdown at Max Tons.
	float LoadSpeedScale()
	{
		return SpeedScaleForTons(CurrentTons());
	}

	//------------------------------------------------------------------------------------------------
	float SpeedScaleForTons(float tons)
	{
		float capacity = m_fMaxTons - m_fFrameTons;
		if (capacity <= 0 || m_fLoadSlowdown <= 0)
			return 1;

		float used = Math.Clamp((tons - m_fFrameTons) / capacity, 0, 1);
		float start = Math.Clamp(m_fSlowdownStart, 0, 0.99);
		if (used <= start)
			return 1;
		float t = (used - start) / (1.0 - start);
		return 1.0 - Math.Clamp(m_fLoadSlowdown, 0, 0.9) * t;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		GetAll().Insert(this);
		m_iExtraHeatsinks = Math.Clamp(m_iStartExtraHeatsinks, 0, Math.Max(m_iMaxExtraHeatsinks, 0));
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;
		// Check the default loadout once everything is spawned, so frame authors see mistakes.
		GetGame().GetCallqueue().CallLater(CheckDefaultLoadout, 2000, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckDefaultLoadout()
	{
		if (!GetOwner())
			return;
		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(GetOwner().FindComponent(BFA_MechHardpointComponent));
		if (!hardpoints)
			return;

		array<int> modules = {};
		for (int i = 0; i < hardpoints.GetHardpointCount(); i++)
			modules.Insert(hardpoints.GetModuleAt(i));

		string reason;
		if (!CheckLoadout(hardpoints, modules, reason))
			Print(string.Format("[RW FRAME] %1: the default loadout breaks the frame rules: %2", m_sFrameName, reason), LogLevel.WARNING);
		else
			Print(string.Format("[RW FRAME] %1 (%2): %3 / %4 tons, %5 extra heatsinks (+%6 cooling/s)", m_sFrameName, typename.EnumToString(RW_EWeightClass, m_eWeightClass), LoadoutTons(hardpoints, modules), m_fMaxTons, m_iExtraHeatsinks, ExtraCooling()));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(CheckDefaultLoadout);
		if (s_aAllFrames)
			s_aAllFrames.RemoveItem(this);
		super.OnDelete(owner);
	}
}

//------------------------------------------------------------------------------------------------
//! The frame rules plugged into Mech Core's hooks.
modded class BFA_MechExtension
{
	protected ref map<IEntity, float> m_mRwLoadScale = new map<IEntity, float>();
	protected ref map<IEntity, int> m_mRwLoadTime = new map<IEntity, int>();

	//------------------------------------------------------------------------------------------------
	override bool CanApplyLoadout(IEntity mech, BFA_MechHardpointComponent hardpoints, notnull array<int> targetModules, out string reason)
	{
		if (!super.CanApplyLoadout(mech, hardpoints, targetModules, reason))
			return false;

		RW_MechFrameComponent frame = RW_MechFrameComponent.Find(mech);
		if (!frame)
			return true;

		// The mech bay sets this while it applies a loadout.
		if (frame.SwapOnlyAtMechBay() && !RW_MechFrameComponent.s_bMechBayApplying)
		{
			reason = "loadouts can only be changed at a mech bay";
			frame.ServerCrewMessage("REFUSED: " + reason);
			return false;
		}

		if (!frame.CheckLoadout(hardpoints, targetModules, reason))
		{
			frame.ServerCrewMessage("REFUSED: " + reason);
			return false;
		}

		frame.ServerCrewMessage(string.Format("Installing... %1 / %2 tons", frame.LoadoutTons(hardpoints, targetModules), frame.GetMaxTons()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Extra heatsinks cool the mech (Core heat asks every tick).
	override float GetExtraCooling(IEntity mech)
	{
		float cooling = super.GetExtraCooling(mech);
		RW_MechFrameComponent frame = RW_MechFrameComponent.Find(mech);
		if (frame)
			cooling += frame.ExtraCooling();
		return cooling;
	}

	//------------------------------------------------------------------------------------------------
	//! Heavy loads slow the frame down (re-read twice a second; movement asks every step).
	override float GetMechSpeedScale(IEntity mech)
	{
		float scale = super.GetMechSpeedScale(mech);
		RW_MechFrameComponent frame = RW_MechFrameComponent.Find(mech);
		if (!frame || !GetGame() || !GetGame().GetWorld())
			return scale;

		int now = GetGame().GetWorld().GetWorldTime();
		int last;
		float load = 1;
		if (m_mRwLoadTime.Find(mech, last) && now - last < 500 && now >= last && m_mRwLoadScale.Find(mech, load))
			return scale * load;

		load = frame.LoadSpeedScale();
		m_mRwLoadScale.Set(mech, load);
		m_mRwLoadTime.Set(mech, now);
		return scale * load;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMechDeleted(IEntity mech)
	{
		super.OnMechDeleted(mech);
		m_mRwLoadScale.Remove(mech);
		m_mRwLoadTime.Remove(mech);
	}
}
