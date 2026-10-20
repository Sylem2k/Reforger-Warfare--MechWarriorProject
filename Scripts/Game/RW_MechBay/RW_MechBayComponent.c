//------------------------------------------------------------------------------------------------
//! REFORGER WARFARE - MECH BAY. Put this on a bay prefab. A mech parked within the radius can be
//! refitted with the Mech Lab from the bay's "Open Mech Bay" action, without the Mech Wrench.
//! Frames with "Swap Only At Mech Bay" can ONLY be refitted while parked in a bay.
//! ORDER FRAME: with the pad empty, "Open Mech Bay" shows the bay's frame list; pick one and the
//! bay builds the bare frame on the parking spot, then the Mech Lab opens on it.
//! Later: repair, rearm, cool-down and supply cost go here.

//------------------------------------------------------------------------------------------------
//! One frame the bay can build.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class RW_BayFrameEntry
{
	[Attribute("", UIWidgets.EditBox, "Name in the order list. Empty = the frame's own name.")]
	string m_sName;

	[Attribute("", UIWidgets.ResourceNamePicker, "Mech prefab to build (spawns with its default, usually empty, loadout).", "et")]
	ResourceName m_sPrefab;
}

//------------------------------------------------------------------------------------------------
class RW_MechBayComponentClass : ScriptComponentClass
{
}

class RW_MechBayComponent : ScriptComponent
{
	[Attribute("Mech Bay", UIWidgets.EditBox, "Name shown in the Mech Lab.", category: "Mech Bay")]
	protected string m_sBayName;

	[Attribute("15", UIWidgets.Auto, "A mech whose centre is within this many metres of the parking spot counts as in the bay.", category: "Mech Bay")]
	protected float m_fRadius;

	[Attribute("0 0 0", UIWidgets.Auto, "Parking spot, relative to the bay (X right, Y up, Z forward). 0 0 0 = the bay's own origin.", category: "Mech Bay")]
	protected vector m_vParkingOffset;

	[Attribute("true", UIWidgets.CheckBox, "Refits here need no Mech Wrench.", category: "Mech Bay")]
	protected bool m_bNoToolNeeded;

	[Attribute("", UIWidgets.Object, "Frames this bay can build (Order Frame).", category: "Frames")]
	protected ref array<ref RW_BayFrameEntry> m_aFrames;

	[Attribute("20", UIWidgets.Auto, "Seconds between two builds at this bay.", category: "Frames")]
	protected float m_fBuildCooldown;

	[Attribute("0.5", UIWidgets.Auto, "The new mech appears this many metres above the ground at the parking spot.", category: "Frames")]
	protected float m_fSpawnHeight;

	// Last bay message (built / refused and why), shown in the order screen.
	[RplProp()]
	protected string m_sBayMessage;
	[RplProp()]
	protected int m_iBayMessageSeq;

	protected float m_fNextBuildTime;   // server
	protected IEntity m_pPendingMech;   // server: saved build waiting to be fitted
	protected ref RW_SavedBuild m_pPendingBuild;
	protected int m_iPendingTries;

	protected static ref array<RW_MechBayComponent> s_aBays;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aBays)
			s_aBays = {};
		s_aBays.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ApplyPendingBuild);
		if (s_aBays)
			s_aBays.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetBayName()
	{
		return m_sBayName;
	}

	float GetRadius()
	{
		return m_fRadius;
	}

	bool NoToolNeeded()
	{
		return m_bNoToolNeeded;
	}

	//------------------------------------------------------------------------------------------------
	vector GetParkingSpot()
	{
		return GetOwner().CoordToParent(m_vParkingOffset);
	}

	//------------------------------------------------------------------------------------------------
	//! Is this mech parked in this bay?
	bool Contains(IEntity mech)
	{
		if (!mech || !GetOwner())
			return false;
		return vector.Distance(mech.GetOrigin(), GetParkingSpot()) <= m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Is this player close enough to work the bay (at the bay or at the parked mech)?
	bool CanOperate(IEntity user, IEntity mech)
	{
		if (!user || !GetOwner())
			return false;
		if (vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) <= m_fRadius + 10)
			return true;
		return mech && vector.Distance(user.GetOrigin(), mech.GetOrigin()) <= 25;
	}

	//------------------------------------------------------------------------------------------------
	//! The bay this mech is parked in, or null.
	static RW_MechBayComponent FindBayFor(IEntity mech)
	{
		if (!mech || !s_aBays)
			return null;
		foreach (RW_MechBayComponent bay : s_aBays)
		{
			if (bay && bay.Contains(mech))
				return bay;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAtBay(IEntity mech)
	{
		return FindBayFor(mech) != null;
	}

	//------------------------------------------------------------------------------------------------
	int GetFrameCount()
	{
		if (!m_aFrames)
			return 0;
		return m_aFrames.Count();
	}

	RW_BayFrameEntry GetFrameEntry(int index)
	{
		if (!m_aFrames || index < 0 || index >= m_aFrames.Count())
			return null;
		return m_aFrames[index];
	}

	string GetBayMessage()
	{
		return m_sBayMessage;
	}

	int GetBayMessageSeq()
	{
		return m_iBayMessageSeq;
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerBayMessage(string message)
	{
		m_sBayMessage = message;
		m_iBayMessageSeq++;
		Replication.BumpMe();
		Print("[RW MECH BAY] " + m_sBayName + ": " + message);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: build frame 'index' on the pad for this player.
	bool ServerOrderFrame(int index, IEntity user, RW_SavedBuild build = null)
	{
		if (!Replication.IsServer())
			return false;

		RW_BayFrameEntry entry = GetFrameEntry(index);
		if (!entry || entry.m_sPrefab.IsEmpty())
		{
			ServerBayMessage("REFUSED: no such frame");
			return false;
		}
		if (!user || vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) > m_fRadius + 10)
		{
			ServerBayMessage("REFUSED: too far from the bay");
			return false;
		}
		if (m_pPendingMech)
		{
			ServerBayMessage("REFUSED: bay is still fitting the previous order");
			return false;
		}
		if (FindParkedMech())
		{
			ServerBayMessage("REFUSED: pad occupied - move the mech off the pad first");
			return false;
		}
		float now = GetGame().GetWorld().GetWorldTime() * 0.001;
		if (now < m_fNextBuildTime)
		{
			ServerBayMessage(string.Format("REFUSED: crew busy, ready in %1 s", Math.Ceil(m_fNextBuildTime - now)));
			return false;
		}

		Resource resource = Resource.Load(entry.m_sPrefab);
		if (!resource || !resource.IsValid())
		{
			ServerBayMessage("REFUSED: frame prefab missing");
			return false;
		}

		// On the ground at the parking spot, facing the way the bay faces.
		BaseWorld world = GetOwner().GetWorld();
		vector spot = GetParkingSpot();
		// Preserve the authored pad elevation for raised or indoor mech bays.
		// Ground-level bays use the terrain surface when the offset is lower.
		spot[1] = Math.Max(spot[1], world.GetSurfaceY(spot[0], spot[2])) + m_fSpawnHeight;
		vector angles = GetOwner().GetYawPitchRoll();
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(angles[0], 0, 0), mat);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = mat[0];
		params.Transform[1] = mat[1];
		params.Transform[2] = mat[2];
		params.Transform[3] = spot;

		IEntity mech = GetGame().SpawnEntityPrefab(resource, world, params);
		if (!mech)
		{
			ServerBayMessage("REFUSED: could not build the frame");
			return false;
		}

		m_fNextBuildTime = now + Math.Max(m_fBuildCooldown, 0);
		string name = entry.m_sName;
		RW_MechFrameComponent frame = RW_MechFrameComponent.Find(mech);
		if (name.IsEmpty() && frame)
			name = frame.GetFrameName();
		if (!build)
		{
			ServerBayMessage(name + " built - fit it out");
			return true;
		}

		// Saved build: fit it once the new mech has settled.
		ServerBayMessage(name + " built - fitting \"" + build.m_sName + "\"...");
		m_pPendingMech = mech;
		m_pPendingBuild = build;
		m_iPendingTries = 0;
		GetGame().GetCallqueue().Remove(ApplyPendingBuild);
		GetGame().GetCallqueue().CallLater(ApplyPendingBuild, 1500, false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyPendingBuild()
	{
		if (!m_pPendingBuild)
			return;
		if (!m_pPendingMech)
		{
			ServerBayMessage("REFUSED: mech was removed before its saved build could be fitted");
			m_pPendingBuild = null;
			return;
		}
		RW_MechFrameComponent frame = RW_MechFrameComponent.Find(m_pPendingMech);
		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(m_pPendingMech.FindComponent(BFA_MechHardpointComponent));
		if (!frame || !hardpoints)
		{
			ServerBayMessage("REFUSED: spawned frame is missing mech components");
			m_pPendingMech = null;
			m_pPendingBuild = null;
			return;
		}
		if (m_pPendingBuild.m_aModules.Count() != hardpoints.GetHardpointCount())
		{
			ServerBayMessage("REFUSED: saved build hardpoint count does not match this frame");
			m_pPendingMech = null;
			m_pPendingBuild = null;
			return;
		}

		// One entry per hardpoint (older saves may be shorter).
		array<int> modules = {};
		for (int i = 0; i < hardpoints.GetHardpointCount(); i++)
		{
			int id = 0;
			if (i < m_pPendingBuild.m_aModules.Count())
				id = m_pPendingBuild.m_aModules[i];
			modules.Insert(id);
		}

		string reason;
		if (frame.ServerApplyBuild(modules, m_pPendingBuild.m_iHeatsinks, reason))
		{
			ServerBayMessage("\"" + m_pPendingBuild.m_sName + "\" fitted - crew at work");
			m_pPendingMech = null;
			m_pPendingBuild = null;
			return;
		}

		// Still settling / busy: try again a few times. Rules broken: tell the player.
		m_iPendingTries++;
		if (m_iPendingTries < 6 && (reason.Contains("still") || reason.Contains("already")))
		{
			GetGame().GetCallqueue().CallLater(ApplyPendingBuild, 1000, false);
			return;
		}
		ServerBayMessage("REFUSED: saved build doesn't fit any more - " + reason + " (fix it in the Mech Lab)");
		m_pPendingMech = null;
		m_pPendingBuild = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The mech parked closest to the parking spot, or null. Walks the frame list (a handful of
	//! mechs) - no area scan, so it is cheap enough for the action prompt.
	IEntity FindParkedMech()
	{
		IEntity best;
		float bestDist = m_fRadius;
		vector spot = GetParkingSpot();
		foreach (RW_MechFrameComponent frame : RW_MechFrameComponent.GetAll())
		{
			if (!frame)
				continue;
			IEntity mech = frame.GetOwner();
			if (!mech || BFA_MechExtension.IsMechDestroyed(mech))
				continue;
			float dist = vector.Distance(mech.GetOrigin(), spot);
			if (dist <= bestDist)
			{
				bestDist = dist;
				best = mech;
			}
		}
		return best;
	}
}

//------------------------------------------------------------------------------------------------
//! On the bay: opens the Mech Lab for the mech parked in it.
class RW_MechBayOpenAction : ScriptedUserAction
{
	protected RW_MechBayComponent m_Bay;
	protected IEntity m_pCachedMech;
	protected float m_fNextLook;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Bay = RW_MechBayComponent.Cast(pOwnerEntity.FindComponent(RW_MechBayComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity ParkedMech()
	{
		if (!m_Bay || !GetGame().GetWorld())
			return null;
		float now = GetGame().GetWorld().GetWorldTime() * 0.001;
		if (now >= m_fNextLook)
		{
			m_fNextLook = now + 0.5;
			m_pCachedMech = m_Bay.FindParkedMech();
		}
		return m_pCachedMech;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return m_Bay != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!ParkedMech() && m_Bay.GetFrameCount() == 0)
		{
			SetCannotPerformReason("No mech parked in the bay");
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		IEntity mech = ParkedMech();
		RW_MechFrameComponent frame = RW_MechFrameComponent.Find(mech);
		if (frame)
			outName = "Open Mech Bay (" + frame.GetFrameName() + ")";
		else if (m_Bay && m_Bay.GetFrameCount() > 0)
			outName = "Mech Bay: Order Frame";
		else
			outName = "Open Mech Bay";
		return true;
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
		m_fNextLook = 0;
		IEntity mech = ParkedMech();
		if (mech)
			RW_MechLabMenu.OpenFor(mech);
		else if (m_Bay.GetFrameCount() > 0)
			RW_MechLabMenu.OpenOrder(m_Bay);
	}
}
