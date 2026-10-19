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

	//------------------------------------------------------------------------------------------------
	//! Client: ask a Mech Bay to build frame 'index' on its pad.
	void RW_RequestOrderFrame(IEntity bayEntity, int index, int savedSlot = 0)
	{
		if (!bayEntity)
			return;
		RplComponent rpl = RplComponent.Cast(bayEntity.FindComponent(RplComponent));
		if (!rpl)
			return;
		if (Replication.IsServer())
		{
			RW_ServerOrderFrame(rpl.Id(), index, savedSlot);
			return;
		}
		Rpc(RW_RpcAsk_OrderFrame, rpl.Id(), index, savedSlot);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RW_RpcAsk_OrderFrame(RplId bayId, int index, int savedSlot)
	{
		RW_ServerOrderFrame(bayId, index, savedSlot);
	}

	//------------------------------------------------------------------------------------------------
	protected void RW_ServerOrderFrame(RplId bayId, int index, int savedSlot)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(bayId));
		if (!rpl || !rpl.GetEntity())
			return;
		RW_MechBayComponent bay = RW_MechBayComponent.Cast(rpl.GetEntity().FindComponent(RW_MechBayComponent));
		if (!bay)
			return;

		// Saved build: look it up in this player's builds for that frame.
		RW_SavedBuild build;
		RW_BayFrameEntry entry = bay.GetFrameEntry(index);
		if (savedSlot > 0 && entry)
			build = RW_SavedBuild.FindSlot(RW_BuildStore.Get(RW_BuildStore.KeyFor(GetPlayerId())), RW_SavedBuild.GuidOf(entry.m_sPrefab), savedSlot);
		bay.ServerOrderFrame(index, GetControlledEntity(), build);
	}

	//==================================================================================================
	// SAVED BUILDS
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	//! Client: save this build of this mech's frame on the server.
	void RW_RequestSaveBuild(IEntity mech, notnull array<int> modules, int heatsinks)
	{
		if (!mech)
			return;
		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;
		string packed = "";
		for (int i = 0; i < modules.Count(); i++)
		{
			if (i > 0)
				packed = packed + ",";
			packed = packed + modules[i].ToString();
		}
		if (Replication.IsServer())
		{
			RW_ServerSaveBuild(rpl.Id(), packed, heatsinks);
			return;
		}
		Rpc(RW_RpcAsk_SaveBuild, rpl.Id(), packed, heatsinks);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RW_RpcAsk_SaveBuild(RplId mechId, string packed, int heatsinks)
	{
		RW_ServerSaveBuild(mechId, packed, heatsinks);
	}

	//------------------------------------------------------------------------------------------------
	protected void RW_ServerSaveBuild(RplId mechId, string packed, int heatsinks)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(mechId));
		if (!rpl || !rpl.GetEntity())
			return;
		IEntity mech = rpl.GetEntity();
		RW_MechFrameComponent frame = RW_MechFrameComponent.Find(mech);
		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(mech.FindComponent(BFA_MechHardpointComponent));
		if (!frame || !hardpoints)
			return;

		RW_SavedBuild build = RW_SavedBuild.FromLine("x|x|" + heatsinks.ToString() + "|" + packed);
		if (!build)
			return;
		string reason;
		if (!frame.CheckBuild(hardpoints, build.m_aModules, build.m_iHeatsinks, reason))
		{
			frame.ServerCrewMessage("REFUSED: can't save - " + reason);
			return;
		}

		// Name from the parts, e.g. "Laser Missiles + Target Designator, 2 HS".
		string name = "";
		foreach (int id : build.m_aModules)
		{
			if (id <= 0)
				continue;
			BFA_MechModuleDefinition module = hardpoints.GetModuleDefinition(id);
			if (!module)
				continue;
			if (!name.IsEmpty())
				name = name + " + ";
			name = name + module.m_sLabel;
		}
		if (name.IsEmpty())
			name = "Empty";
		if (build.m_iHeatsinks > 0)
			name = name + ", " + build.m_iHeatsinks.ToString() + " HS";

		build.m_sFrame = RW_SavedBuild.FrameOf(mech);
		build.m_sName = RW_SavedBuild.Clean(name);
		string key = RW_BuildStore.KeyFor(GetPlayerId());
		int slot = RW_BuildStore.Add(key, build);
		frame.ServerCrewMessage(string.Format("Saved build %1/%2: %3", slot, RW_BuildStore.MAX_PER_FRAME, build.m_sName));
		RW_ServerSendBuilds();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask for my saved builds (answer arrives in RW_RpcDo_Builds).
	void RW_RequestBuilds()
	{
		if (Replication.IsServer())
		{
			RW_ServerSendBuilds();
			return;
		}
		Rpc(RW_RpcAsk_Builds);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RW_RpcAsk_Builds()
	{
		RW_ServerSendBuilds();
	}

	//------------------------------------------------------------------------------------------------
	protected void RW_ServerSendBuilds()
	{
		string data = RW_BuildStore.Pack(RW_BuildStore.KeyFor(GetPlayerId()));
		if (GetGame().GetPlayerController() == this)
		{
			RW_MechLabMenu.ReceiveBuilds(data);
			return;
		}
		Rpc(RW_RpcDo_Builds, data);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RW_RpcDo_Builds(string data)
	{
		RW_MechLabMenu.ReceiveBuilds(data);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: delete saved build 'slot' of a frame.
	void RW_RequestDeleteBuild(string frame, int slot)
	{
		if (Replication.IsServer())
		{
			RW_ServerDeleteBuild(frame, slot);
			return;
		}
		Rpc(RW_RpcAsk_DeleteBuild, frame, slot);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RW_RpcAsk_DeleteBuild(string frame, int slot)
	{
		RW_ServerDeleteBuild(frame, slot);
	}

	protected void RW_ServerDeleteBuild(string frame, int slot)
	{
		RW_BuildStore.Remove(RW_BuildStore.KeyFor(GetPlayerId()), frame, slot);
		RW_ServerSendBuilds();
	}
}
