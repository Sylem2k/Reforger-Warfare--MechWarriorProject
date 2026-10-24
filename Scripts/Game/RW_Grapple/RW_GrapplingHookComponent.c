//------------------------------------------------------------------------------------------------
//! Reforger Warfare - handheld Grappling Hook test prototype.
//! Also supports Handgun_Grapling.et (M9): firing the normal pistol trigger
//! invokes grapple. ADS stays vanilla. OnProjectileShot event from the player
//! is used with an optional immediate MuzzleEffect event where available.
//! Server is authoritative; camera/weapon aim direction is used when available.
//! Direct CharacterFire input works even with an empty pistol / no magazine.
//! The old OnProjectileShot path is retained only for non-direct-fire setups.
//! There is NO rope rendering, no fall-damage immunity and no wall-climbing yet.
//! With physics pull the player controller may reject imposed velocity on some builds;
//! an explicitly opt-in scripted movement test is exposed in Workbench.
[ComponentEditorProps(category: "Reforger Warfare/Grappling", description: "Experimental handheld grapple: server raycast and controlled character pull.")]
class RW_GrapplingHookComponentClass : ScriptComponentClass
{
}

class RW_GrapplingHookComponent : ScriptComponent
{
	[Attribute("50", UIWidgets.Auto, "Maximum hook distance in metres.", category: "Grapple")]
	protected float m_fMaxRange;

	[Attribute("18", UIWidgets.Auto, "Pull speed, metres per second.", category: "Grapple")]
	protected float m_fPullSpeed;

	[Attribute("2.0", UIWidgets.Auto, "Stop pulling when the player's chest is this close to the anchor (metres).", category: "Grapple")]
	protected float m_fStopDistance;

	[Attribute("3", UIWidgets.Auto, "Cooldown after activation, seconds.", category: "Grapple")]
	protected float m_fCooldown;

	[Attribute("6", UIWidgets.Auto, "Maximum grapple pull duration before controlled descent, seconds.", category: "Grapple")]
	protected float m_fMaxDuration;

	// SAFETY: The hook ray is a point, but a moving soldier occupies a capsule.
	// Stay several metres away from the anchor and brake before approaching it.
	[Attribute("4.5", UIWidgets.Auto, "Minimum body clearance from grapple impact before stopping horizontal pull (metres). 4+ recommended for walls.", category: "Safe Arrival")]
	protected float m_fSafeArrivalDistance;

	[Attribute("14", UIWidgets.Auto, "Distance at which the grapple starts slowing for a safe arrival (metres).", category: "Safe Arrival")]
	protected float m_fBrakeStartDistance;

	[Attribute("2.5", UIWidgets.Auto, "Controlled downward speed when the grapple ends in the air (metres/second).", category: "Safe Arrival")]
	protected float m_fSafeDescentSpeed;

	[Attribute("90", UIWidgets.Auto, "Maximum controlled descent time in seconds before giving control back to the character. Manual cancel always releases.", category: "Safe Arrival")]
	protected float m_fSafeDescentTimeout;

	[Attribute("1.5", UIWidgets.Auto, "Approximate eye height for the server-side facing raycast.", category: "Grapple")]
	protected float m_fEyeHeight;

	[Attribute("0.65", UIWidgets.Auto, "One-time upwards release from the grounded character controller, metres. 0 disables the lift.", category: "Grapple Test")]
	protected float m_fGroundReleaseLift;

	[Attribute("false", UIWidgets.CheckBox, "EXPERIMENT ONLY: server moves the character with SetOrigin instead of velocity. Use only if native physics cannot pull a player; may cause camera snapping and movement collisions.", category: "Grapple Test")]
	protected bool m_bScriptedPositionFallback;

	[Attribute("false", UIWidgets.CheckBox, "Log start, stop and failed motion diagnostics.", category: "Grapple Test")]
	protected bool m_bDebug;

	[Attribute("false", UIWidgets.CheckBox, "M9 test: listen for actual pistol shots and activate grapple on left-click. ADS remains normal.", category: "Pistol Trigger")]
	protected bool m_bUseWeaponTrigger;

	[Attribute("true", UIWidgets.CheckBox, "M9 test: try to remove the normal fired bullet on the server. This is NOT a substitute for using a proper zero-damage test cartridge.", category: "Pistol Trigger")]
	protected bool m_bDeleteProjectile;

	[Attribute("false", UIWidgets.CheckBox, "M9: fire/release using the player CharacterFire input, even with ZERO ammo. Reads local input and RPCs the authoritative weapon owner. No bullet event needed.", category: "Pistol Trigger")]
	protected bool m_bUseDirectFireInput;

	[Attribute("false", UIWidgets.CheckBox, "Preferred M9 mode: local SCR_PlayerController reads CharacterFire and sends a player-owned server RPC. Works without ammunition and avoids firearm entity network ownership.", category: "Pistol Trigger")]
	protected bool m_bUseControllerFireInput;

	protected IEntity m_pUser;
	protected vector m_vAnchor;
	protected vector m_vLastPosition;
	protected float m_fNoMotionSeconds;
	protected int m_iStartMs;
	protected int m_iNextUseMs;
	protected float m_fReleaseDelay;

	// The equipped player's OnProjectileShot is a vanilla Reforger weapon event.
	protected IEntity m_pFireEventUser;
	protected bool m_bRegisteredMuzzle;
	protected int m_iLastPistolShotMs;
	protected bool m_bWasFireDown;

	// Test-only diagnostics: prove the ACTUAL grapple pistol prefab is running.
	protected bool m_bRWInstanceFrameLogged;
	protected bool m_bRWEquippedFrameLogged;
	protected bool m_bRWPullStartLogged;
	protected bool m_bRWSafeDescending;
	protected int m_iRWSafeDescentStartMs;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// Instance-level trace is independent of PlayerController.OnUpdate.
		// If this message is absent after spawning the test M9, either the
		// wrong prefab is spawned or the installed Workbench script is stale.
		Print(string.Format("[RW GRAPPLE ITEM] OnPostInit weapon=%1 server=%2 controllerMode=%3", owner, Replication.IsServer(), m_bUseControllerFireInput));

		// Input polling runs on the owning local client, not only on the server.
		// The replicated component then requests a validated server-side pull.
		SetEventMask(owner, EntityEvent.FRAME);
		if (!Replication.IsServer())
			return;

		// Reforger's regular muzzle event is immediate. Register only if this
		// inherited pistol base has the scripted muzzle-effect component.
		if (m_bUseWeaponTrigger && !m_bUseControllerFireInput)
		{
			SCR_MuzzleEffectComponent muzzleEffects = SCR_MuzzleEffectComponent.Cast(owner.FindComponent(SCR_MuzzleEffectComponent));
			if (muzzleEffects)
			{
				muzzleEffects.GetOnWeaponFired().Insert(OnMuzzleFired);
				m_bRegisteredMuzzle = true;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		DetachShotListener();
		if (m_bRegisteredMuzzle)
		{
			SCR_MuzzleEffectComponent muzzleEffects = SCR_MuzzleEffectComponent.Cast(owner.FindComponent(SCR_MuzzleEffectComponent));
			if (muzzleEffects)
				muzzleEffects.GetOnWeaponFired().Remove(OnMuzzleFired);
		}
		StopPull(false);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Find the active user from the pistol's inventory/hand hierarchy.
	protected ChimeraCharacter FindEquippedPlayer()
	{
		IEntity weaponEntity = GetOwner();
		if (!weaponEntity)
			return null;

		IEntity ancestor = weaponEntity.GetParent();
		while (ancestor)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(ancestor);
			if (character && IsEquippedBy(character))
				return character;
			ancestor = ancestor.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void DetachShotListener()
	{
		if (!m_pFireEventUser)
			return;

		EventHandlerManagerComponent events = EventHandlerManagerComponent.Cast(m_pFireEventUser.FindComponent(EventHandlerManagerComponent));
		if (events)
			events.RemoveScriptHandler("OnProjectileShot", this, OnPlayerShot);

		m_pFireEventUser = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateShotListener()
	{
		// Controller input takes priority over legacy bullet listeners.
		// This prevents two firings for one click even if a prefab accidentally
		// has multiple legacy trigger options enabled.
		if (!m_bUseWeaponTrigger || m_bUseControllerFireInput)
			return;

		IEntity active = FindEquippedPlayer();
		if (active == m_pFireEventUser)
			return;

		DetachShotListener();
		if (!active)
			return;

		EventHandlerManagerComponent events = EventHandlerManagerComponent.Cast(active.FindComponent(EventHandlerManagerComponent));
		if (!events)
		{
			if (m_bDebug)
				Print("[RW GRAPPLE] Active player lacks EventHandlerManagerComponent", LogLevel.WARNING);
			return;
		}

		events.RegisterScriptHandler("OnProjectileShot", this, OnPlayerShot);
		m_pFireEventUser = active;
		if (m_bDebug)
			Print("[RW GRAPPLE] Pistol trigger listener attached to player");
	}

	//------------------------------------------------------------------------------------------------
	//! Scripted muzzle callback (immediate, if inherited pistol supplies it).
	protected void OnMuzzleFired(IEntity effectEntity, BaseMuzzleComponent muzzle, IEntity projectileEntity)
	{
		if (!Replication.IsServer())
			return;
		ProcessPistolShot(projectileEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla player event, delayed until the next frame: catches pistol
	//! variants that do not expose SCR_MuzzleEffectComponent.
	protected void OnPlayerShot(int playerId, BaseWeaponComponent weapon, IEntity projectileEntity)
	{
		if (!Replication.IsServer() || !weapon || weapon.GetOwner() != GetOwner())
			return;

		ProcessPistolShot(projectileEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteShotProjectile(IEntity projectileEntity)
	{
		if (!Replication.IsServer() || !projectileEntity)
			return;
		SCR_EntityHelper.DeleteEntityAndChildren(projectileEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void ProcessPistolShot(IEntity projectileEntity)
	{
		if (!m_bUseWeaponTrigger || m_bUseDirectFireInput || m_bUseControllerFireInput || !Replication.IsServer())
			return;

		ChimeraCharacter user = FindEquippedPlayer();
		if (!user)
			return;

		// Muzzle and player events may fire for the same round. Process it once
		// so we never schedule deletion of the same projectile twice.
		int now = System.GetTickCount();
		if (now - m_iLastPistolShotMs < 100)
			return;
		m_iLastPistolShotMs = now;

		// Never delete an engine projectile from inside its own OnFired
		// callback: other muzzle listeners may still be using the same entity.
		// Postpone the deletion until the callback has returned.
		// This is best effort ONLY. A zero-damage test round is still required
		// before allowing this weapon to shoot around players.
		if (m_bDeleteProjectile && projectileEntity && GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
				queue.CallLater(DeleteShotProjectile, 1, false, projectileEntity);
		}

		vector aimDirection = user.GetTransformAxis(2);
		AimingComponent aiming = AimingComponent.Cast(user.FindComponent(AimingComponent));
		if (!aiming)
			aiming = AimingComponent.Cast(GetOwner().FindComponent(AimingComponent));

		if (aiming)
		{
			vector weaponDirection = aiming.GetAimingDirectionWorld();
			if (weaponDirection.LengthSq() > 0.001)
				aimDirection = weaponDirection;
		}

		if (aimDirection.LengthSq() > 0.001)
			aimDirection.Normalize();

		if (m_bDebug)
			Print(string.Format("[RW GRAPPLE] Pistol trigger detected, aim=%1", aimDirection));

		ServerToggleWithDirection(user, aimDirection);
	}

	//------------------------------------------------------------------------------------------------
	//! Receive the LEFT MOUSE / CharacterFire press on the player owning the gun.
	//! Runs on each client but only reacts to its own equipped pistol.
	protected void PollLocalFireInput()
	{
		// Never poll the same left-click from both the pistol entity and the
		// player's controller. If controller mode is enabled it owns all input.
		if (!m_bUseDirectFireInput || m_bUseControllerFireInput || !GetGame())
			return;

		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return; // Dedicated server has no local shooter.

		IEntity user = controller.GetControlledEntity();
		if (!user || !IsEquippedBy(user))
		{
			m_bWasFireDown = false;
			return;
		}

		InputManager inputs = GetGame().GetInputManager();
		if (!inputs)
			return;

		// Works with 0 rounds; the game does not need to issue OnProjectileShot.
		bool down = inputs.GetActionValue("CharacterFire") > 0.5;
		bool pressed = down && !m_bWasFireDown;
		m_bWasFireDown = down;

		if (!pressed)
			return;

		vector direction = user.GetTransformAxis(2);
		AimingComponent aiming = AimingComponent.Cast(user.FindComponent(AimingComponent));
		if (!aiming)
			aiming = AimingComponent.Cast(GetOwner().FindComponent(AimingComponent));
		if (aiming && aiming.GetAimingDirectionWorld().LengthSq() > 0.001)
			direction = aiming.GetAimingDirectionWorld();

		if (direction.LengthSq() < 0.001)
			return;
		direction.Normalize();

		if (m_bDebug)
			Print(string.Format("[RW GRAPPLE INPUT] CharacterFire pressed, weapon=%1, server=%2", GetOwner(), Replication.IsServer()));

		if (Replication.IsServer())
		{
			ServerToggleWithDirection(user, direction);
		}
		else
		{
			// Pistol inherits M9's RplComponent. The server validates that
			// this exact weapon is equipped by a live player before moving them.
			Rpc(RW_RpcAskToggle, direction);
		}
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RW_RpcAskToggle(vector direction)
	{
		if (!Replication.IsServer() || !m_bUseDirectFireInput)
			return;
		ChimeraCharacter user = FindEquippedPlayer();
		if (!user)
		{
			if (m_bDebug)
				Print("[RW GRAPPLE INPUT] Server rejected grapple: pistol not actively equipped", LogLevel.WARNING);
			return;
		}
		if (m_bDebug)
			Print(string.Format("[RW GRAPPLE INPUT] Server received LEFT CLICK from pistol owner: %1", user));
		ServerToggleWithDirection(user, direction);
	}

	//------------------------------------------------------------------------------------------------
 	//------------------------------------------------------------------------------------------------
	//! Read by the local SCR_PlayerController without requiring a magazine.
	bool WantsControllerFireInput()
	{
		return m_bUseControllerFireInput;
	}

	//------------------------------------------------------------------------------------------------
	//! Validated on the server against the controller's CURRENT weapon and user.
	bool ServerPlayerControllerFire(IEntity user, vector direction)
	{
		if (!m_bUseControllerFireInput || !Replication.IsServer())
			return false;
		if (m_bDebug)
			Print(string.Format("[RW GRAPPLE CTRL] Component received player-controller fire user=%1 equipped=%2", user, IsEquippedBy(user)));
		return ServerToggleWithDirection(user, direction);
	}

	//------------------------------------------------------------------------------------------------
	bool IsEquippedBy(IEntity user)
	{
		if (!user || !GetOwner() || !ChimeraCharacter.Cast(user))
			return false;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(user.FindComponent(CharacterControllerComponent));
		if (!controller)
			return false;

		if (controller.GetRightHandItem() == GetOwner())
			return true;

		// Firearms use the weapon manager, not the gadget right-hand item.
		BaseWeaponComponent currentWeapon = SCR_WeaponLib.GetCurrentWeaponComponent(ChimeraCharacter.Cast(user));
		return currentWeapon && currentWeapon.GetOwner() == GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	bool IsActiveFor(IEntity user)
	{
		return m_pUser && m_pUser == user;
	}

	//------------------------------------------------------------------------------------------------
	bool CanUse(IEntity user)
	{
		if (!IsEquippedBy(user))
			return false;

		// Let the same player release at any time, even during the cooldown.
		if (IsActiveFor(user))
			return true;

		if (m_pUser)
			return false;

		return System.GetTickCount() >= m_iNextUseMs;
	}

	//------------------------------------------------------------------------------------------------
	//! One activation toggles fire/cancel. ScriptedUserAction is executed on the server.
	bool ServerToggle(IEntity user)
	{
		if (!user)
			return false;
		return ServerToggleWithDirection(user, user.GetTransformAxis(2));
	}

	//------------------------------------------------------------------------------------------------
	protected bool ServerToggleWithDirection(IEntity user, vector shotDirection)
	{
		if (!Replication.IsServer() || !GetGame() || !IsEquippedBy(user))
			return false;

		if (IsActiveFor(user))
		{
			StopPull(true);
			return true;
		}

		if (!CanUse(user))
			return false;

		// Never accept an arbitrary AI/remote entity in lieu of the player's character.
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return false;

		int playerId = players.GetPlayerIdFromControlledEntity(user);
		if (playerId <= 0 || players.GetPlayerControlledEntity(playerId) != user)
			return false;

		BaseWorld world = GetGame().GetWorld();
		if (!world || !user.GetPhysics())
			return false;

		vector direction = shotDirection;
		if (direction.LengthSq() <= 0.0001)
			return false;
		direction.Normalize();

		vector start = user.GetOrigin() + vector.Up * Math.Clamp(m_fEyeHeight, 0.8, 2.2);
		float range = Math.Clamp(m_fMaxRange, 3, 150);
		vector limit = start + direction * range;

		TraceParam ray = new TraceParam();
		ray.Start = start;
		ray.End = limit;
		ray.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		ray.LayerMask = EPhysicsLayerPresets.Projectile;
		array<IEntity> excluded = {};
		excluded.Insert(user);
		excluded.Insert(GetOwner());
		ray.ExcludeArray = excluded;

		float fraction = world.TraceMove(ray, null);
		if (fraction >= 1.0)
		{
			if (m_bDebug)
				Print("[RW GRAPPLE] No surface within range", LogLevel.WARNING);
			return false;
		}

		vector anchor = start + (limit - start) * fraction;
		if (vector.Distance(start, anchor) < Math.Max(m_fStopDistance + 0.75, 3.0))
			return false;

		// Lift only if there is empty room above the player's head; don't force
		// a player up through a roof. The native physics pull begins after release.
		float lift = Math.Clamp(m_fGroundReleaseLift, 0.0, 1.2);
		if (!m_bScriptedPositionFallback && lift > 0.01)
		{
			TraceParam liftTrace = new TraceParam();
			liftTrace.Start = start;
			liftTrace.End = start + vector.Up * lift;
			liftTrace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			liftTrace.LayerMask = EPhysicsLayerPresets.Projectile;
			liftTrace.ExcludeArray = excluded;
			if (world.TraceMove(liftTrace, null) >= 1.0)
			{
				user.SetOrigin(user.GetOrigin() + vector.Up * lift);
				if (m_bDebug)
					Print(string.Format("[RW GRAPPLE] Initial clear-space lift applied: %1 metres", lift));
			}
			else
			{
				if (m_bDebug)
					Print("[RW GRAPPLE] Initial lift blocked by overhead collision", LogLevel.WARNING);
			}
		}

		m_pUser = user;
		m_vAnchor = anchor;
		m_vLastPosition = user.GetOrigin();
		m_fNoMotionSeconds = 0;
		m_bRWPullStartLogged = false;
		m_bRWSafeDescending = false;
		m_iRWSafeDescentStartMs = 0;
		m_fReleaseDelay = 0.08;
		m_iStartMs = System.GetTickCount();
		m_iNextUseMs = m_iStartMs + (int)(Math.Max(m_fCooldown, 0.0) * 1000.0);

		if (m_bDebug)
			Print(string.Format("[RW GRAPPLE] player=%1 anchor=%2 range=%3", playerId, anchor, vector.Distance(start, anchor)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void StopPull(bool playerCancelled)
	{
		if (m_pUser && !m_bScriptedPositionFallback)
		{
			Physics body = m_pUser.GetPhysics();
			if (body)
				body.SetVelocity(vector.Zero);
		}
		if (m_bDebug && m_pUser)
			Print(string.Format("[RW GRAPPLE] stopped cancelled=%1 at=%2", playerCancelled, m_pUser.GetOrigin()));

		m_pUser = null;
		m_bRWPullStartLogged = false;
		m_bRWSafeDescending = false;
		m_iRWSafeDescentStartMs = 0;
		m_fReleaseDelay = 0;
		m_fNoMotionSeconds = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The final metres are where full pull speed causes fatal wall collisions.
	//! Switch from horizontal travel to a controlled descent, holding the
	//! player's horizontal velocity at zero instead of letting the 18m/s
	//! momentum send them into the collision mesh.
	protected void BeginSafeDescent(string reason)
	{
		if (!m_pUser || m_bRWSafeDescending)
			return;

		m_bRWSafeDescending = true;
		m_iRWSafeDescentStartMs = System.GetTickCount();

		Physics body = m_pUser.GetPhysics();
		if (body)
			body.SetVelocity(vector.Zero);

		if (m_bDebug)
			Print(string.Format("[RW GRAPPLE SAFE] Approach ended (%1). Controlled descent started.", reason));
	}

	//------------------------------------------------------------------------------------------------
	//! Move vertically at a low speed until feet are near walkable ground.
	//! Only use the server's character physics here; never teleport into a
	//! wall or forcibly adjust other players' health/damage state.
	protected void UpdateSafeDescent(float timeSlice)
	{
		if (!m_pUser || !GetGame())
			return;

		Physics body = m_pUser.GetPhysics();
		BaseWorld world = GetGame().GetWorld();
		if (!body || !world)
		{
			StopPull(false);
			return;
		}

		vector pos = m_pUser.GetOrigin();
		TraceParam down = new TraceParam();
		down.Start = pos + vector.Up * 0.45;
		down.End = pos - vector.Up * 2.2;
		down.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		down.LayerMask = EPhysicsLayerPresets.Projectile;
		array<IEntity> excluded = {};
		excluded.Insert(m_pUser);
		excluded.Insert(GetOwner());
		down.ExcludeArray = excluded;

		float fraction = world.TraceMove(down, null);
		if (fraction < 1.0 && down.TraceNorm[1] > 0.55)
		{
			vector surface = down.Start + (down.End - down.Start) * fraction;
			float footGap = pos[1] - surface[1];
			if (footGap <= 0.55 && footGap >= -0.25)
			{
				if (m_bDebug)
					Print(string.Format("[RW GRAPPLE SAFE] Landed, foot gap=%1", footGap));
				StopPull(false);
				return;
			}
		}

		int descentMs = System.GetTickCount() - m_iRWSafeDescentStartMs;
		if (descentMs > (int)(Math.Max(m_fSafeDescentTimeout, 10.0) * 1000.0))
		{
			if (m_bDebug)
				Print("[RW GRAPPLE SAFE] Controlled descent timed out; control released.", LogLevel.WARNING);
			StopPull(false);
			return;
		}

		float safeFallSpeed = Math.Clamp(m_fSafeDescentSpeed, 1.0, 4.0);
		if (m_bScriptedPositionFallback)
		{
			// Keep scripted-position fallback explicit and conservative.
			// Never move below the detected floor within this frame.
			float stepDown = safeFallSpeed * Math.Clamp(timeSlice, 0.0, 0.05);
			if (fraction < 1.0 && down.TraceNorm[1] > 0.55)
			{
				vector hit = down.Start + (down.End - down.Start) * fraction;
				stepDown = Math.Min(stepDown, Math.Max(0.0, pos[1] - hit[1] - 0.1));
			}
			m_pUser.SetOrigin(pos - vector.Up * stepDown);
		}
		else
		{
			body.SetVelocity(vector.Up * -safeFallSpeed);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Check near future capsule clearance with three parallel forward rays.
	//! Unlike the old single chest ray, these detect ledges at head/foot height.
	protected bool IsApproachBlocked(BaseWorld world, vector origin, vector travelDir, float ahead)
	{
		array<IEntity> ignored = {};
		ignored.Insert(m_pUser);
		ignored.Insert(GetOwner());

		for (int i = 0; i < 3; i++)
		{
			float h = 0.35;
			if (i == 1)
				h = 1.1;
			else if (i == 2)
				h = 1.75;

			TraceParam test = new TraceParam();
			test.Start = origin + vector.Up * h;
			test.End = test.Start + travelDir * ahead;
			test.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			test.LayerMask = EPhysicsLayerPresets.Projectile;
			test.ExcludeArray = ignored;
			if (world.TraceMove(test, null) < 1.0)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		// A separate proof that the entity FRAME event actually executes.
		// Printed only once per copy of this test item.
		if (!m_bRWInstanceFrameLogged)
		{
			m_bRWInstanceFrameLogged = true;
			Print(string.Format("[RW GRAPPLE ITEM] EOnFrame active weapon=%1 server=%2", owner, Replication.IsServer()));
		}

		// On the server, report the first time the pistol can actually be
		// resolved as the character's selected weapon. Does not change use.
		if (Replication.IsServer() && !m_bRWEquippedFrameLogged && m_bUseControllerFireInput)
		{
			ChimeraCharacter testOwner = FindEquippedPlayer();
			if (testOwner)
			{
				m_bRWEquippedFrameLogged = true;
				Print(string.Format("[RW GRAPPLE ITEM] Pistol currently equipped by %1", testOwner));
			}
		}

		// Direct input also runs for a client-only weapon replica.
		PollLocalFireInput();

		if (!Replication.IsServer() || !GetGame())
			return;

		// The old projectile listener still supports non-direct-fire tests.
		if (!m_bUseDirectFireInput && !m_bUseControllerFireInput)
			UpdateShotListener();

		if (!m_pUser)
			return;

		if (!IsEquippedBy(m_pUser))
		{
			StopPull(false);
			return;
		}

		// Once the grapple has finished advancing, continue applying gentle
		// downward speed until the character is on a walkable surface.
		if (m_bRWSafeDescending)
		{
			UpdateSafeDescent(timeSlice);
			return;
		}

		int elapsed = System.GetTickCount() - m_iStartMs;
		if (elapsed > (int)(Math.Max(m_fMaxDuration, 0.5) * 1000.0))
		{
			BeginSafeDescent("pull timeout");
			return;
		}

		if (m_fReleaseDelay > 0.0)
		{
			m_fReleaseDelay -= Math.Max(timeSlice, 0.0);
			return;
		}

		Physics body = m_pUser.GetPhysics();
		BaseWorld world = GetGame().GetWorld();
		if (!body || !world)
		{
			StopPull(false);
			return;
		}

		vector position = m_pUser.GetOrigin();
		vector chest = position + vector.Up * 1.1;
		vector toward = m_vAnchor - chest;
		float remaining = toward.Length();

		// The anchor is a WALL CONTACT POINT, not a safe place for a human
		// capsule. Hold off by several metres (including the inherited stop
		// distance) and avoid slamming into the object at full travel speed.
		float stopDistance = Math.Max(Math.Clamp(m_fSafeArrivalDistance, 3.0, 10.0), Math.Max(m_fStopDistance, 0.75));
		if (remaining <= stopDistance)
		{
			BeginSafeDescent("safe stand-off distance");
			return;
		}

		toward.Normalize();
		float speed = Math.Clamp(m_fPullSpeed, 3.0, 40.0);
		float brakeAt = Math.Max(Math.Clamp(m_fBrakeStartDistance, 5.0, 40.0), stopDistance + 2.0);
		if (remaining < brakeAt)
		{
			// Continuous slowdown prevents the engine character controller
			// from having to absorb an 18m/s collision impulse at the wall.
			float allowed = Math.Max(1.5, (remaining - stopDistance) * 1.6);
			speed = Math.Min(speed, allowed);
		}

		float dt = Math.Clamp(timeSlice, 0.0, 0.1);
		if (dt <= 0.0001)
			return;
		// Do not ask the body to cross the final stand-off zone this frame.
		speed = Math.Min(speed, Math.Max(0.25, (remaining - stopDistance) / dt));
		vector step = toward * speed * dt;

		// Keep an advance safety buffer and inspect feet, chest and head.
		// This intentionally cancels the high-speed approach before any one
		// of those points would meet the wall or an overhanging obstacle.
		float lookAhead = Math.Min(remaining, Math.Max(1.4, step.Length() + 1.1));
		if (IsApproachBlocked(world, position, toward, lookAhead))
		{
			BeginSafeDescent("wall/head/feet clearance");
			return;
		}

		if (!m_bRWPullStartLogged)
		{
			m_bRWPullStartLogged = true;
			if (m_bDebug)
				Print(string.Format("[RW GRAPPLE] Pull phase started: speed=%1 m/s, scriptedFallback=%2", speed, m_bScriptedPositionFallback));
		}

		if (m_bScriptedPositionFallback)
		{
			m_pUser.SetOrigin(position + step);
		}
		else
		{
			body.SetVelocity(toward * speed);
			float moved = vector.Distance(position, m_vLastPosition);
			if (moved < 0.10)
				m_fNoMotionSeconds += dt;
			else
				m_fNoMotionSeconds = 0;

			// Never silently teleport if the network/character controller rejects
			// imposed velocity. Let the tester opt into SetOrigin in Workbench.
			if (m_fNoMotionSeconds > 0.65)
			{
				if (m_bDebug)
					Print("[RW GRAPPLE] Native player motion blocked. Try Scripted Position Fallback in a disposable test world.", LogLevel.WARNING);
				StopPull(false);
				return;
			}
		}

		m_vLastPosition = position;
	}
}
