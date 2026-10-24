//------------------------------------------------------------------------------------------------
//! Streak SRM: server-side timed crosshair lock.
//! A pulse starts acquisition. One uninterrupted target lock fires one salvo.
//! Nothing consumes ammunition until Core accepts the completed lock.
//! Core continues to handle missile flight, heat, ammunition, reload and replication.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "Reforger Warfare/Weapons", description: "Streak SRM: timed lock on a target before firing a homing salvo.")]
class RW_StreakSRMWeaponClass : CGS_Titan_MissileClass
{
}

class RW_StreakSRMWeapon : CGS_Titan_Missile
{
	[Attribute("700", UIWidgets.Auto, "Milliseconds the same target must remain under the crosshair.", category: "MW Streak Lock")]
	protected int m_iLockTimeMs;

	[Attribute("4000", UIWidgets.Auto, "Maximum time for one lock attempt. Press fire again after timeout.", category: "MW Streak Lock")]
	protected int m_iAcquireTimeoutMs;

	protected bool m_bLockPending;
	protected int m_iAcquireStartedMs;
	protected int m_iStableSinceMs;
	protected IEntity m_pAcquiringTarget;
	protected IEntity m_pLockInstigator;

	// Frozen target for the whole salvo, even if the pilot moves the reticle mid-launch.
	protected IEntity m_pCommittedTarget;
	protected vector m_vCommittedLocalPoint;

	//------------------------------------------------------------------------------------------------
	//! Find exactly the target Core's launch trace would see, without consuming a tube.
	protected IEntity FindCrosshairTarget(out vector hitPoint)
	{
		vector direction;
		vector start = GetMuzzleRaw(direction, false);
		ApplyFireAtAimPoint(start, direction);

		IEntity hit;
		TraceSegment(start, start + direction * Math.Max(m_fRange, 10.0), m_pMechOwner, hit, hitPoint);
		if (!hit)
			return null;

		IEntity root = hit.GetRootParent();
		if (!root)
			root = hit;

		if (root == m_pMechOwner || root == GetOwner())
			return null;

		return root;
	}

	//------------------------------------------------------------------------------------------------
	//! Called for both normal trigger-down and fire-pulse routes.
	override protected void TryStartSalvo(IEntity instigator)
	{
		if (!Replication.IsServer() || !m_bBoundToHardpoint || m_bLockPending)
			return;

		int nowMs = GetWorldTimeMs();
		if (nowMs < m_iNextTriggerTimeMs || m_iSalvoRemaining > 0 || m_bReloading)
			return;

		if (m_iMagazineSize > 0 && m_iRoundsInMagazine <= 0)
			return;

		vector hitPoint;
		IEntity target = FindCrosshairTarget(hitPoint);
		if (!target)
			return;

		m_pCommittedTarget = null;
		m_pAcquiringTarget = target;
		m_pLockInstigator = instigator;
		m_iAcquireStartedMs = nowMs;
		m_iStableSinceMs = nowMs;
		m_bLockPending = true;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
		{
			StopLock();
			return;
		}
		queue.CallLater(StreakLockTick, 100, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StreakLockTick()
	{
		if (!Replication.IsServer() || !m_bLockPending || !m_bBoundToHardpoint || !m_pMechOwner)
		{
			StopLock();
			return;
		}

		int nowMs = GetWorldTimeMs();
		if (nowMs - m_iAcquireStartedMs >= Math.Max(m_iAcquireTimeoutMs, 100))
		{
			StopLock();
			return;
		}

		if (m_bReloading || m_iSalvoRemaining > 0)
		{
			StopLock();
			return;
		}

		vector hitPoint;
		IEntity target = FindCrosshairTarget(hitPoint);
		if (!target || target != m_pAcquiringTarget)
		{
			// Changing targets or losing line of sight resets lock progress.
			m_pAcquiringTarget = target;
			m_iStableSinceMs = nowMs;
			return;
		}

		if (nowMs - m_iStableSinceMs < Math.Max(m_iLockTimeMs, 100))
			return;

		m_pCommittedTarget = target;
		m_vCommittedLocalPoint = target.CoordToLocal(hitPoint);
		IEntity instigator = m_pLockInstigator;
		StopLock();

		// All Core cooldown, firing, heat and ammunition rules still apply.
		super.TryStartSalvo(instigator);
	}

	//------------------------------------------------------------------------------------------------
	//! Assign the acquired target to each missile in the completed salvo.
	override protected void LaunchRocket(IEntity instigator)
	{
		super.LaunchRocket(instigator);

		if (!m_pCommittedTarget || m_aRockets.IsEmpty())
			return;

		CGS_TitanMissileInFlight missile = CGS_TitanMissileInFlight.Cast(m_aRockets[m_aRockets.Count() - 1]);
		if (!missile)
			return;

		missile.m_pSpot = null;
		missile.m_pTarget = m_pCommittedTarget;
		missile.m_vTargetLocal = m_vCommittedLocalPoint;
		missile.m_vTargetPoint = m_pCommittedTarget.CoordToParent(m_vCommittedLocalPoint);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopLock()
	{
		if (GetGame())
		{
			ScriptCallQueue queue = GetGame().GetCallqueue();
			if (queue)
				queue.Remove(StreakLockTick);
		}
		m_bLockPending = false;
		m_pAcquiringTarget = null;
		m_pLockInstigator = null;
	}

	//------------------------------------------------------------------------------------------------
	override void UnbindFromHardpoint()
	{
		StopLock();
		m_pCommittedTarget = null;
		super.UnbindFromHardpoint();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		StopLock();
		m_pCommittedTarget = null;
		super.OnDelete(owner);
	}
}
