//------------------------------------------------------------------------------------------------
//! REFORGER WARFARE - MECH BAY. Put this on a bay prefab. A mech parked within the radius can be
//! refitted with the Mech Lab from the bay's "Open Mech Bay" action, without the Mech Wrench.
//! Frames with "Swap Only At Mech Bay" can ONLY be refitted while parked in a bay.
//! Later: repair, rearm, cool-down and supply cost go here.
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

	protected static ref array<RW_MechBayComponent> s_aBays;

	// Sphere query results (FindParkedMech).
	protected IEntity m_pQueryBest;
	protected float m_fQueryBestDist;
	protected vector m_vQuerySpot;

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
	//! The mech parked closest to the parking spot, or null.
	IEntity FindParkedMech()
	{
		BaseWorld world = GetOwner().GetWorld();
		if (!world)
			return null;
		m_pQueryBest = null;
		m_fQueryBestDist = m_fRadius + 1;
		m_vQuerySpot = GetParkingSpot();
		world.QueryEntitiesBySphere(m_vQuerySpot, m_fRadius, QueryMech, null, EQueryEntitiesFlags.DYNAMIC);
		return m_pQueryBest;
	}

	//------------------------------------------------------------------------------------------------
	protected bool QueryMech(IEntity entity)
	{
		IEntity root = entity.GetRootParent();
		if (!root)
			root = entity;
		if (!RW_MechFrameComponent.Find(root) || BFA_MechExtension.IsMechDestroyed(root))
			return true;
		float dist = vector.Distance(root.GetOrigin(), m_vQuerySpot);
		if (dist < m_fQueryBestDist)
		{
			m_fQueryBestDist = dist;
			m_pQueryBest = root;
		}
		return true;
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
		if (!ParkedMech())
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
	}
}
