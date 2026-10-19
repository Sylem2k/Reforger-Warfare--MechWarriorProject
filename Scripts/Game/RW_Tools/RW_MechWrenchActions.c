//------------------------------------------------------------------------------------------------
//! REFORGER WARFARE - MECH WRENCH (field refits). A player carrying the Mech Wrench walks up to a
//! frame and uses the actions on it. A mech parked in a Mech Bay shows the same actions to
//! everyone, no wrench needed (area check: the mech is within the bay's radius):
//!   - Fit next weapon (hardpoint) : installs the next weapon that fits the frame's rules
//!   - Remove weapon (hardpoint)   : leaves the hardpoint empty
//!   - Add / Remove heatsink
//! Every change goes through the same frame rules (tonnage, hardpoint type and size) as the mech
//! bay will, and takes the normal swap time. The mech has to stand still.
//! Add the actions to the mech's ActionsManagerComponent (context "default"); see the README.

//------------------------------------------------------------------------------------------------
class RW_MechWrench
{
	//! Default tool: the Mech Wrench item.
	static const ResourceName DEFAULT_TOOL = "{33B2DFDCD0EBA3DB}Prefabs/Items/Equipment/Kits/RepairKit_01/Mech Wrench.et";

	//------------------------------------------------------------------------------------------------
	//! "{GUID}" part of a resource name (paths can be written differently, the GUID can't).
	protected static string GuidOf(string resource)
	{
		int close = resource.IndexOf("}");
		if (resource.StartsWith("{") && close > 0)
			return resource.Substring(0, close + 1);
		return resource;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsTool(IEntity item, string toolGuid)
	{
		if (!item)
			return false;
		EntityPrefabData data = item.GetPrefabData();
		if (!data)
			return false;
		return GuidOf(data.GetPrefabName()) == toolGuid;
	}

	// Cache for HasToolCached (the action prompt asks every frame; the inventory scan is slow).
	protected static IEntity s_pCachedUser;
	protected static bool s_bCachedHasTool;
	protected static float s_fCachedUntil;

	//------------------------------------------------------------------------------------------------
	//! HasTool, re-checked at most twice a second. For action prompts only (the server's real
	//! check uses HasTool).
	static bool HasToolCached(IEntity user, ResourceName tool)
	{
		float now = 0;
		if (GetGame() && GetGame().GetWorld())
			now = GetGame().GetWorld().GetWorldTime() * 0.001;
		if (user == s_pCachedUser && now < s_fCachedUntil)
			return s_bCachedHasTool;
		s_pCachedUser = user;
		s_fCachedUntil = now + 0.5;
		s_bCachedHasTool = HasTool(user, tool);
		return s_bCachedHasTool;
	}

	//------------------------------------------------------------------------------------------------
	//! Does this character hold or carry the tool?
	static bool HasTool(IEntity user, ResourceName tool)
	{
		if (!user)
			return false;
		string toolGuid = GuidOf(tool);

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(user.FindComponent(CharacterControllerComponent));
		if (controller && IsTool(controller.GetRightHandItem(), toolGuid))
			return true;

		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(user.FindComponent(InventoryStorageManagerComponent));
		if (inventory)
		{
			array<IEntity> items = {};
			inventory.GetItems(items);
			foreach (IEntity item : items)
			{
				if (IsTool(item, toolGuid))
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Module ids that could go on this hardpoint, in list order (0 = empty is not included).
	static void GetCandidates(BFA_MechHardpointComponent hardpoints, int hardpointIndex, notnull array<int> outIds)
	{
		outIds.Clear();
		if (!hardpoints || hardpointIndex < 0)
			return;
		for (int id = 1; id <= 31; id++)
		{
			if (hardpoints.GetModuleDefinition(id))
				outIds.Insert(id);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Current loadout as module ids per hardpoint.
	static void GetLoadout(BFA_MechHardpointComponent hardpoints, notnull array<int> outIds)
	{
		outIds.Clear();
		for (int i = 0; i < hardpoints.GetHardpointCount(); i++)
			outIds.Insert(hardpoints.GetModuleAt(i));
	}

	//------------------------------------------------------------------------------------------------
	//! Next module after the current one that passes the frame rules (0 = none fits).
	static int FindNextFitting(RW_MechFrameComponent frame, BFA_MechHardpointComponent hardpoints, int hardpointIndex)
	{
		array<int> candidates = {};
		GetCandidates(hardpoints, hardpointIndex, candidates);
		if (candidates.IsEmpty())
			return 0;

		int current = hardpoints.GetModuleAt(hardpointIndex);
		int start = candidates.Find(current);
		array<int> loadout = {};
		GetLoadout(hardpoints, loadout);

		for (int step = 1; step <= candidates.Count(); step++)
		{
			int index = (start + step) % candidates.Count();
			if (index < 0)
				index += candidates.Count();
			int id = candidates[index];
			if (id == current)
				continue;
			// Core class check too (a SHOULDER-only part never goes on the ARM).
			if (!hardpoints.CanModuleFit(hardpointIndex, id))
				continue;

			loadout[hardpointIndex] = id;
			string reason;
			if (!frame || frame.CheckLoadout(hardpoints, loadout, reason))
				return id;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put module id on the hardpoint through the normal swap (rules, swap time).
	static bool ServerRequest(IEntity mech, int hardpointIndex, int moduleId)
	{
		if (!Replication.IsServer() || !mech)
			return false;
		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(mech.FindComponent(BFA_MechHardpointComponent));
		if (!hardpoints)
			return false;

		RW_MechFrameComponent.s_bMechBayApplying = RW_MechBayComponent.IsAtBay(mech);
		bool ok = hardpoints.ServerRequestModule(hardpoints.m_sSwapStation, hardpointIndex, moduleId);
		RW_MechFrameComponent.s_bMechBayApplying = false;
		return ok;
	}
}

//------------------------------------------------------------------------------------------------
//! Base for the wrench actions: tool check, frame / hardpoint lookup, "stand still".
class RW_MechWrenchActionBase : ScriptedUserAction
{
	[Attribute("{33B2DFDCD0EBA3DB}Prefabs/Items/Equipment/Kits/RepairKit_01/Mech Wrench.et", UIWidgets.ResourceNamePicker, "Tool the player must hold or carry.", "et")]
	protected ResourceName m_sTool;

	[Attribute("", UIWidgets.EditBox, "Core hardpoint name this action works on (weapon actions only), e.g. ARM.")]
	protected string m_sHardpoint;

	//------------------------------------------------------------------------------------------------
	protected BFA_MechHardpointComponent GetHardpoints()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;
		return BFA_MechHardpointComponent.Cast(owner.GetRootParent().FindComponent(BFA_MechHardpointComponent));
	}

	protected RW_MechFrameComponent GetFrame()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;
		return RW_MechFrameComponent.Find(owner.GetRootParent());
	}

	protected int GetHardpointIndex()
	{
		BFA_MechHardpointComponent hardpoints = GetHardpoints();
		if (!hardpoints || m_sHardpoint.IsEmpty())
			return -1;
		return hardpoints.FindHardpointIndex(m_sHardpoint);
	}

	protected string ModuleName(int moduleId)
	{
		BFA_MechHardpointComponent hardpoints = GetHardpoints();
		if (!hardpoints || moduleId == 0)
			return "nothing";
		BFA_MechModuleDefinition module = hardpoints.GetModuleDefinition(moduleId);
		if (!module)
			return "?";
		RW_EquipmentEntry item = RW_EquipmentCatalog.Get().Find(module.m_sWeaponModuleId);
		string name = module.m_sLabel;
		if (item && !item.m_sDisplayName.IsEmpty())
			name = item.m_sDisplayName;
		return string.Format("%1 (%2 t)", name, RW_MechFrameComponent.ModuleTons(module));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!GetFrame())
			return false;
		// Parked in a Mech Bay: the bay's crew does the work, no wrench needed.
		if (RW_MechBayComponent.IsAtBay(GetOwner().GetRootParent()))
			return true;
		return RW_MechWrench.HasToolCached(user, m_sTool);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		BFA_MechHardpointComponent hardpoints = GetHardpoints();
		if (hardpoints && hardpoints.IsSwapInProgress())
		{
			SetCannotPerformReason("Already working on it");
			return false;
		}

		IEntity mech = GetOwner().GetRootParent();
		Physics physics = mech.GetPhysics();
		if (physics && physics.GetVelocity().Length() > 0.5)
		{
			SetCannotPerformReason("The mech must stand still");
			return false;
		}
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Fit the next weapon that fits on the hardpoint.
class RW_MechWrenchFitAction : RW_MechWrenchActionBase
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return super.CanBeShownScript(user) && GetHardpointIndex() >= 0;
	}

	//------------------------------------------------------------------------------------------------
	protected string m_sCachedName;
	protected float m_fNameUntil;

	override bool GetActionNameScript(out string outName)
	{
		// Working out the next fitting weapon checks the whole loadout: do it twice a second.
		float now = GetGame().GetWorld().GetWorldTime() * 0.001;
		if (now < m_fNameUntil && !m_sCachedName.IsEmpty())
		{
			outName = m_sCachedName;
			return true;
		}
		m_fNameUntil = now + 0.5;

		int next = RW_MechWrench.FindNextFitting(GetFrame(), GetHardpoints(), GetHardpointIndex());
		if (next == 0)
			outName = string.Format("%1: nothing else fits", m_sHardpoint);
		else
			outName = string.Format("Fit %1 on %2", ModuleName(next), m_sHardpoint);
		m_sCachedName = outName;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;
		if (RW_MechWrench.FindNextFitting(GetFrame(), GetHardpoints(), GetHardpointIndex()) == 0)
		{
			SetCannotPerformReason("Nothing else fits (tonnage / type)");
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int hp = GetHardpointIndex();
		int next = RW_MechWrench.FindNextFitting(GetFrame(), GetHardpoints(), hp);
		if (hp >= 0 && next != 0)
			RW_MechWrench.ServerRequest(pOwnerEntity.GetRootParent(), hp, next);
	}
}

//------------------------------------------------------------------------------------------------
//! Take the weapon off the hardpoint.
class RW_MechWrenchRemoveAction : RW_MechWrenchActionBase
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!super.CanBeShownScript(user))
			return false;
		int hp = GetHardpointIndex();
		BFA_MechHardpointComponent hardpoints = GetHardpoints();
		return hp >= 0 && hardpoints.GetModuleAt(hp) != 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		BFA_MechHardpointComponent hardpoints = GetHardpoints();
		int hp = GetHardpointIndex();
		int current = 0;
		if (hardpoints && hp >= 0)
			current = hardpoints.GetModuleAt(hp);
		outName = string.Format("Remove %1 from %2", ModuleName(current), m_sHardpoint);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int hp = GetHardpointIndex();
		if (hp >= 0)
			RW_MechWrench.ServerRequest(pOwnerEntity.GetRootParent(), hp, 0);
	}
}

//------------------------------------------------------------------------------------------------
//! Add (or with Remove ticked, take out) one extra heatsink.
class RW_MechWrenchHeatsinkAction : RW_MechWrenchActionBase
{
	[Attribute("false", UIWidgets.CheckBox, "Take a heatsink out instead of adding one.")]
	protected bool m_bRemove;

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		RW_MechFrameComponent frame = GetFrame();
		int count = 0;
		if (frame)
			count = frame.GetExtraHeatsinks();
		if (m_bRemove)
			outName = string.Format("Remove heatsink (%1 fitted)", count);
		else
			outName = string.Format("Add heatsink (%1 fitted, 1 t each)", count);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;
		RW_MechFrameComponent frame = GetFrame();
		if (!frame)
			return false;
		if (m_bRemove && frame.GetExtraHeatsinks() <= 0)
		{
			SetCannotPerformReason("No extra heatsinks fitted");
			return false;
		}
		if (!m_bRemove && frame.GetExtraHeatsinks() >= frame.GetMaxExtraHeatsinks())
		{
			SetCannotPerformReason("No room for more heatsinks");
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		RW_MechFrameComponent frame = GetFrame();
		if (!frame)
			return;

		int count = frame.GetExtraHeatsinks() + 1;
		if (m_bRemove)
			count = frame.GetExtraHeatsinks() - 1;

		string reason;
		if (frame.ServerSetExtraHeatsinks(count, reason))
			frame.ServerCrewMessage(string.Format("Heatsinks: %1 extra, %2 / %3 tons", frame.GetExtraHeatsinks(), frame.CurrentTons(), frame.GetMaxTons()));
		else
			frame.ServerCrewMessage("REFUSED: " + reason);
	}
}

//------------------------------------------------------------------------------------------------
//! Open the Mech Lab build screen (runs only on the player's own machine).
class RW_MechLabOpenAction : RW_MechWrenchActionBase
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Open Mech Lab";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true; // the lab itself says when the mech must stand still
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (pUserEntity != SCR_PlayerController.GetLocalControlledEntity())
			return;
		RW_MechLabMenu.OpenFor(pOwnerEntity.GetRootParent());
	}
}
