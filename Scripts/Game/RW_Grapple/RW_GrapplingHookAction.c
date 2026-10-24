//------------------------------------------------------------------------------------------------
//! User action for the ETool-based Grappling_Hook.et.
//! This is a test interaction, NOT a firearm trigger binding.
//! Add to the tool's ActionsManagerComponent default context.
class RW_GrapplingHookAction : ScriptedUserAction
{
	protected RW_GrapplingHookComponent GetHook()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;
		return RW_GrapplingHookComponent.Cast(owner.FindComponent(RW_GrapplingHookComponent));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "GRAPPLE: FIRE / RELEASE (TEST)";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		RW_GrapplingHookComponent hook = GetHook();
		return hook && hook.IsEquippedBy(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		RW_GrapplingHookComponent hook = GetHook();
		if (hook && hook.CanUse(user))
			return true;
		SetCannotPerformReason("Equip hook in right hand or wait for cooldown");
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		RW_GrapplingHookComponent hook = GetHook();
		if (hook)
			hook.ServerToggle(pUserEntity);
	}
}
