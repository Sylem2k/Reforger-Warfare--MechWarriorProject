//------------------------------------------------------------------------------------------------
//! Reforger Warfare: ammo-independent grappling input from the actual local player controller.
//!
//! M9-derived guns keep their default ADS (right mouse) and equip/pickup behavior.
//! The normal CharacterFire input (left mouse) is read on the local player controller,
//! A press is a held-value rising edge; holding down left mouse cannot toggle
//! the grapple twice. Release, then click again to cancel.
//! regardless of whether the firearm successfully fires a projectile.
//! The controller has player-owned RPC authority and resolves the EQUIPPED
//! RW_GrapplingHookComponent again on the server. No weapon-entity RPC ownership
//! or inferred item parenting is required.
//
//! Test only with an EMPTY M9 magazine: the vanilla weapon can still shoot
//! ordinary damaging bullets if loaded. No general changes to other firearms.
modded class SCR_PlayerController
{
	protected bool m_bRWGrappleFireWasDown;
	protected bool m_bRWGrappleControllerStartupLogged;
	protected bool m_bRWGrappleUpdateLogged;
	protected int m_iRWGrappleLastCommandMs;

	//------------------------------------------------------------------------------------------------
	//! Logged at controller initialization, even before play input begins.
	override protected void OnInit(IEntity owner)
	{
		super.OnInit(owner);
		Print(string.Format("[RW GRAPPLE DIAG] PlayerController OnInit; server=%1", Replication.IsServer()));
	}

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);

		// Print once for every instance (local and server). If this does not
		// appear, the game has not executed our new controller script.
		if (!m_bRWGrappleUpdateLogged)
		{
			m_bRWGrappleUpdateLogged = true;
			bool isLocalController = false;
			if (GetGame())
				isLocalController = GetGame().GetPlayerController() == this;
			Print(string.Format("[RW GRAPPLE DIAG] PlayerController OnUpdate; server=%1 local=%2", Replication.IsServer(), isLocalController));
		}

		if (!GetGame() || GetGame().GetPlayerController() != this)
			return;

		if (!m_bRWGrappleControllerStartupLogged)
		{
			m_bRWGrappleControllerStartupLogged = true;
			Print("[RW GRAPPLE CTRL] Player-controller input hook loaded");
		}

		InputManager inputs = GetGame().GetInputManager();
		if (!inputs)
			return;

		// IMPORTANT: GetActionTriggered("CharacterFire") can report true on
		// successive update frames while a button is held. Combining it with
		// the edge test makes one click immediately FIRE then RELEASE.
		// Use the actual held input VALUE and send exactly one command per
		// down-edge. Another command requires a physical release first.
		bool fireDown = inputs.GetActionValue("CharacterFire") > 0.5;
		bool justPressed = fireDown && !m_bRWGrappleFireWasDown;
		m_bRWGrappleFireWasDown = fireDown;

		if (!justPressed)
			return;

		// Suppress bounce/rapid-fire action pulses in case an input context
		// briefly resets the held value across adjacent frames.
		int nowMs = System.GetTickCount();
		if (m_iRWGrappleLastCommandMs > 0 && nowMs - m_iRWGrappleLastCommandMs < 250)
			return;

		Print("[RW GRAPPLE DIAG] CharacterFire press observed before weapon checks");

		ChimeraCharacter user = ChimeraCharacter.Cast(GetControlledEntity());
		if (!user)
		{
			Print("[RW GRAPPLE DIAG] No character controlled", LogLevel.WARNING);
			return;
		}

		BaseWeaponComponent weapon = SCR_WeaponLib.GetCurrentWeaponComponent(user);
		if (!weapon)
		{
			Print("[RW GRAPPLE DIAG] Current weapon manager reports no selected gun", LogLevel.WARNING);
			return;
		}

		Print(string.Format("[RW GRAPPLE DIAG] Current weapon entity=%1", weapon.GetOwner()));

		RW_GrapplingHookComponent grapple = RW_GrapplingHookComponent.Cast(weapon.GetOwner().FindComponent(RW_GrapplingHookComponent));
		if (!grapple)
		{
			Print("[RW GRAPPLE DIAG] Equipped weapon has no RW_GrapplingHookComponent", LogLevel.WARNING);
			return;
		}
		if (!grapple.WantsControllerFireInput())
		{
			Print("[RW GRAPPLE DIAG] Grappling component found, but controller-fire mode is DISABLED", LogLevel.WARNING);
			return;
		}

		vector direction = user.GetTransformAxis(2);
		AimingComponent aim = AimingComponent.Cast(user.FindComponent(AimingComponent));
		if (!aim)
			aim = AimingComponent.Cast(weapon.GetOwner().FindComponent(AimingComponent));
		if (aim)
		{
			vector aimed = aim.GetAimingDirectionWorld();
			if (aimed.LengthSq() > 0.001)
				direction = aimed;
		}

		if (direction.LengthSq() < 0.001)
			return;
		direction.Normalize();

		Print(string.Format("[RW GRAPPLE CTRL] Local FIRE input detected. Equipped=%1", weapon.GetOwner()));
		m_iRWGrappleLastCommandMs = nowMs;

		if (Replication.IsServer())
		{
			grapple.ServerPlayerControllerFire(user, direction);
			return;
		}

		// This RPC is owned by the player's controller, not by their gun.
		Rpc(RW_RpcAskGrappleFire, direction);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RW_RpcAskGrappleFire(vector direction)
	{
		if (!Replication.IsServer())
			return;

		ChimeraCharacter user = ChimeraCharacter.Cast(GetControlledEntity());
		if (!user)
			return;

		BaseWeaponComponent weapon = SCR_WeaponLib.GetCurrentWeaponComponent(user);
		if (!weapon)
		{
			Print("[RW GRAPPLE CTRL] Server: no current weapon", LogLevel.WARNING);
			return;
		}

		RW_GrapplingHookComponent grapple = RW_GrapplingHookComponent.Cast(weapon.GetOwner().FindComponent(RW_GrapplingHookComponent));
		if (!grapple || !grapple.WantsControllerFireInput())
		{
			Print("[RW GRAPPLE CTRL] Server: current weapon lacks active grapple component", LogLevel.WARNING);
			return;
		}

		Print(string.Format("[RW GRAPPLE CTRL] Server received fire input. Player=%1 weapon=%2", user, weapon.GetOwner()));
		grapple.ServerPlayerControllerFire(user, direction);
	}
}
