//------------------------------------------------------------------------------------------------
//! REFORGER WARFARE - MECH LAB networking: the client sends the whole planned build (weapon per
//! hardpoint + extra heatsinks) in one request; the server checks the player (near the mech,
//! has the tool) and the frame rules, then applies it with the normal swap timer.
modded class SCR_PlayerController
{
	protected static const int RW_BITS = 10;
	protected static const int RW_ID_MASK = 1023;
	protected static const int RW_PER_WORD = 3;
	protected static const float RW_MAX_DISTANCE = 25;

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server to build this mech this way.
	void RW_RequestBuild(IEntity mech, notnull array<int> modules, int heatsinks)
	{
		if (!mech)
			return;
		// The current wire format carries at most 18 module IDs.
		// Never send a partial build while reporting its original count.
		if (modules.Count() < 1 || modules.Count() > RW_PER_WORD * 6)
		{
			Print("[RW MECH LAB] Build refused: unsupported hardpoint count", LogLevel.WARNING);
			return;
		}
		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;

		// Serialize the 18 local module IDs in a single RPC string.  Each ID is
		// validated server-side against the authoritative mech definition.
		string serialized = "";
		for (int i = 0; i < modules.Count(); i++)
		{
			if (modules[i] < 0 || modules[i] > RW_ID_MASK)
				return;
			if (i > 0)
				serialized = serialized + ",";
			serialized = serialized + modules[i].ToString();
		}
		if (Replication.IsServer())
		{
			RW_ServerBuild(rpl.Id(), serialized, heatsinks);
			return;
		}
		Rpc(RW_RpcAsk_Build, rpl.Id(), serialized, heatsinks);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RW_RpcAsk_Build(RplId mechId, string serialized, int heatsinks)
	{
		RW_ServerBuild(mechId, serialized, heatsinks);
	}

	//------------------------------------------------------------------------------------------------
	protected void RW_ServerBuild(RplId mechId, string serialized, int heatsinks)
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
		BFA_MechHardpointComponent hardpoints = BFA_MechHardpointComponent.Cast(mech.FindComponent(BFA_MechHardpointComponent));
		if (!hardpoints)
		{
			frame.ServerCrewMessage("REFUSED: this mech has no hardpoints");
			return;
		}

		// 18 decimal IDs (up to four digits each) plus separators.
		if (serialized.IsEmpty() || serialized.Length() > 90)
		{
			frame.ServerCrewMessage("REFUSED: invalid loadout data");
			return;
		}
		ref array<string> tokens = {};
		serialized.Split(",", tokens, false);
		if (tokens.Count() != hardpoints.GetHardpointCount() || tokens.Count() < 1 || tokens.Count() > RW_PER_WORD * 6)
		{
			frame.ServerCrewMessage("REFUSED: invalid loadout hardpoint count");
			return;
		}

		array<int> modules = {};
		foreach (string token : tokens)
		{
			if (token.IsEmpty() || token.Length() > 4)
				return;
			for (int c = 0; c < token.Length(); c++)
			{
				int ch = token.ToAscii(c);
				if (ch < 48 || ch > 57)
					return;
			}
			int id = token.ToInt();
			if (id < 0 || id > RW_ID_MASK)
				return;
			modules.Insert(id);
		}

		// Parked in a Mech Bay: the bay does the work (no tool needed).
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

		string reason;
		if (frame.ServerApplyBuild(modules, heatsinks, reason))
			frame.ServerCrewMessage(string.Format("Build accepted: %1 / %2 t, %3 extra heatsinks", frame.BuildTons(hardpoints, modules, heatsinks), frame.GetMaxTons(), heatsinks));
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
		if (savedSlot > 0)
		{
			if (!entry)
				return;
			build = RW_SavedBuild.FindSlot(RW_BuildStore.Get(RW_BuildStore.KeyFor(GetPlayerId())), RW_SavedBuild.GuidOf(entry.m_sPrefab), savedSlot);
			// Never silently order an empty frame when a saved slot was requested.
			if (!build)
				return;
		}
		bay.ServerOrderFrame(index, GetControlledEntity(), GetPlayerId(), build);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: return the mech ordered at this Mech Bay (saved builds are not affected).
	void RW_RequestDeleteOrderedMech(IEntity mech)
	{
		if (!mech)
			return;
		RplComponent rpl = RplComponent.Cast(mech.FindComponent(RplComponent));
		if (!rpl)
			return;
		if (Replication.IsServer())
		{
			RW_ServerDeleteOrderedMech(rpl.Id());
			return;
		}
		Rpc(RW_RpcAsk_DeleteOrderedMech, rpl.Id());
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RW_RpcAsk_DeleteOrderedMech(RplId mechId)
	{
		RW_ServerDeleteOrderedMech(mechId);
	}

	protected void RW_ServerDeleteOrderedMech(RplId mechId)
	{
		if (!Replication.IsServer())
			return;
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(mechId));
		if (!rpl || !rpl.GetEntity())
			return;
		IEntity mech = rpl.GetEntity().GetRootParent();
		if (!mech)
			mech = rpl.GetEntity();
		RW_MechBayComponent bay = RW_MechBayComponent.FindBayFor(mech);
		if (bay)
			bay.ServerDeleteOrderedMech(mech, GetPlayerId(), GetControlledEntity());
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

		// Saving a build must obey the same proximity / tool rules as applying one.
		IEntity user = GetControlledEntity();
		RW_MechBayComponent bay = RW_MechBayComponent.FindBayFor(mech);
		bool bayWorks = bay && bay.NoToolNeeded() && bay.CanOperate(user, mech);
		if (!user || (!bayWorks && (vector.Distance(user.GetOrigin(), mech.GetOrigin()) > RW_MAX_DISTANCE || !RW_MechWrench.HasTool(user, RW_MechWrench.DEFAULT_TOOL))))
		{
			frame.ServerCrewMessage("REFUSED: Mech Bay or nearby Mech Wrench required to save");
			return;
		}
		if (packed.Length() > 128)
		{
			frame.ServerCrewMessage("REFUSED: saved build data too long");
			return;
		}

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