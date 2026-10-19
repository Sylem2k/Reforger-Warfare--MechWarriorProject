//------------------------------------------------------------------------------------------------
//! REFORGER WARFARE - MECH LAB networking: the client sends the whole planned build (weapon per
//! hardpoint + extra heatsinks) in one request; the server checks the player (near the mech,
//! has the tool) and the frame rules, then applies it with the normal swap timer.
modded class SCR_PlayerController
{
	protected static const int RW_BITS = 5;
	protected static const int RW_PER_WORD = 6;
	protected static const float RW_MAX_DISTANCE = 25;

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server to build this mech this way.
	void RW_RequestBuild(IEntity mech, notnull array<int> modules, int heatsinks)
	{
		if (!mech)
			return;
		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		int w0 = 0;
		int w1 = 0;
		int w2 = 0;
		for (int i = 0; i < modules.Count() && i < RW_PER_WORD * 3; i++)
		{
			int id = Math.Clamp(modules[i], 0, 31);
			int shift = Math.Mod(i, RW_PER_WORD) * RW_BITS;
			int word = i / RW_PER_WORD;
			if (word == 0)
				w0 = w0 | (id << shift);
			else if (word == 1)
				w1 = w1 | (id << shift);
			else
				w2 = w2 | (id << shift);
		}

		if (Replication.IsServer())
		{
			RW_ServerBuild(rpl.Id(), w0, w1, w2, modules.Count(), heatsinks);
			return;
		}
		Rpc(RW_RpcAsk_Build, rpl.Id(), w0, w1, w2, modules.Count(), heatsinks);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RW_RpcAsk_Build(RplId mechId, int w0, int w1, int w2, int count, int heatsinks)
	{
		RW_ServerBuild(mechId, w0, w1, w2, count, heatsinks);
	}

	//------------------------------------------------------------------------------------------------
	protected void RW_ServerBuild(RplId mechId, int w0, int w1, int w2, int count, int heatsinks)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(mechId));
		if (!rpl || !rpl.GetEntity())
			return;
		IEntity mech = rpl.GetEntity().GetRootParent();
		if (!mech)
			mech = rpl.GetEntity();

		RW_MechFrameComponent frame = RW_MechFrameComponent.Find(mech);
		if (!frame)
			return;

		// Parked in a Mech Bay: the bay does the work (no tool needed, the player can stand at the
		// bay). Anywhere else: the player must stand at the mech with the Mech Wrench.
		IEntity user = GetControlledEntity();
		RW_MechBayComponent bay = RW_MechBayComponent.FindBayFor(mech);
		bool bayWorks = bay && bay.NoToolNeeded() && bay.CanOperate(user, mech);
		if (!user || (!bayWorks && vector.Distance(user.GetOrigin(), mech.GetOrigin()) > RW_MAX_DISTANCE))
		{
			frame.ServerCrewMessage("REFUSED: too far from the mech");
			return;
		}
		if (!bayWorks && !RW_MechWrench.HasTool(user, RW_MechWrench.DEFAULT_TOOL))
		{
			frame.ServerCrewMessage("REFUSED: you need the Mech Wrench");
			return;
		}

		array<int> modules = {};
		for (int i = 0; i < count && i < RW_PER_WORD * 3; i++)
		{
			int shift = Math.Mod(i, RW_PER_WORD) * RW_BITS;
			int word = w0;
			if (i / RW_PER_WORD == 1)
				word = w1;
			else if (i / RW_PER_WORD == 2)
				word = w2;
			modules.Insert((word >> shift) & 31);
		}

		string reason;
		if (frame.ServerApplyBuild(modules, heatsinks, reason))
			frame.ServerCrewMessage(string.Format("Build accepted: %1 / %2 t, %3 extra heatsinks", frame.BuildTons(BFA_MechHardpointComponent.Cast(mech.FindComponent(BFA_MechHardpointComponent)), modules, heatsinks), frame.GetMaxTons(), heatsinks));
		else
			frame.ServerCrewMessage("REFUSED: " + reason);
	}
}
