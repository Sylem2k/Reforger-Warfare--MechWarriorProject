//------------------------------------------------------------------------------------------------
//! CGS / RW COMBAT WEAPON HUD - CLIENT-ONLY PRESENTATION LAYER.
//! Integrates with Mech Core's existing cockpit HUD and aiming/crosshair.
//! It reads real hardpoint modules, ammunition, reload status, weapon groups and mech heat.
//! There are no firing, RPC, input, targeting or damage changes in this file.
//! This addon owns the weapon display; Core still owns the actual aiming reticle.
//! Keep this script in MW Gameplay, NOT in the visual Assets addon.
//------------------------------------------------------------------------------------------------
modded class BFA_MechHUDDisplay
{
	static const int RW_COMBAT_MAX_ROWS = 8;
	static const float RW_COMBAT_PANEL_W = 404;
	static const float RW_COMBAT_ROW_H = 45;

	protected bool m_bRwCombatBuilt;

	protected ImageWidget m_wRwCombatPanel;
	protected ImageWidget m_wRwCombatHeaderBar;
	protected TextWidget m_wRwCombatTitle;
	protected TextWidget m_wRwCombatMode;
	protected TextWidget m_wRwCombatGroups;
	protected TextWidget m_wRwCombatHeat;

	protected ref array<ImageWidget> m_aRwCombatRowBg = {};
	protected ref array<ImageWidget> m_aRwCombatRowStripe = {};
	protected ref array<TextWidget> m_aRwCombatRowName = {};
	protected ref array<TextWidget> m_aRwCombatRowStatus = {};
	protected ref array<TextWidget> m_aRwCombatRowDetail = {};
	protected ref array<ImageWidget> m_aRwCombatRowBarBg = {};
	protected ref array<ImageWidget> m_aRwCombatRowBar = {};

	protected ref array<ImageWidget> m_aRwAimDecor = {};
	protected TextWidget m_wRwAimMode;
	protected TextWidget m_wRwAimDistance;
	protected TextWidget m_wRwAimContext; // weapon-type targeting info, not an unverified lock state

	//------------------------------------------------------------------------------------------------
	protected Color RW_CombatTypeColor(RW_EquipmentEntry entry)
	{
		if (entry)
		{
			if (entry.m_eType == RW_EHardpointType.ENERGY)
				return new Color(0.23, 0.88, 0.87, 0.98);
			if (entry.m_eType == RW_EHardpointType.BALLISTIC)
				return new Color(1, 0.73, 0.35, 0.98);
			if (entry.m_eType == RW_EHardpointType.MISSILE)
				return new Color(0.72, 0.62, 1, 0.98);
			if (entry.m_eType == RW_EHardpointType.SUPPORT)
				return new Color(0.45, 0.9, 0.61, 0.98);
		}
		return new Color(0.8, 0.87, 0.9, 0.98);
	}

	//------------------------------------------------------------------------------------------------
	protected string RW_CombatTypeName(RW_EquipmentEntry entry)
	{
		if (entry)
		{
			if (entry.m_eType == RW_EHardpointType.ENERGY)
				return "ENERGY";
			if (entry.m_eType == RW_EHardpointType.BALLISTIC)
				return "BALLISTIC";
			if (entry.m_eType == RW_EHardpointType.MISSILE)
				return "MISSILE";
			if (entry.m_eType == RW_EHardpointType.SUPPORT)
				return "SUPPORT";
			if (entry.m_eType == RW_EHardpointType.MELEE)
				return "MELEE";
		}
		return "WEAPON";
	}

	//------------------------------------------------------------------------------------------------
	protected void RW_BuildCombatHUD()
	{
		// PanelRect / PanelText are Mech Core helpers. Their widgets are tracked by
		// the base HUD and destroyed when the player exits the crew station.
		m_bRwCombatBuilt = true;
		Color background = new Color(0.025, 0.05, 0.063, 0.94);
		Color title = new Color(0.27, 0.91, 0.92, 1);
		Color white = new Color(0.94, 0.98, 1, 1);
		Color muted = new Color(0.62, 0.73, 0.77, 1);

		m_wRwCombatPanel = PanelRect(RW_COMBAT_PANEL_W, 160, background);
		m_wRwCombatHeaderBar = PanelRect(RW_COMBAT_PANEL_W, 3, title);
		m_wRwCombatTitle = PanelText(RW_COMBAT_PANEL_W - 20, 26, 18, white);
		m_wRwCombatMode = PanelText(RW_COMBAT_PANEL_W - 20, 22, 14, title);
		m_wRwCombatGroups = PanelText(RW_COMBAT_PANEL_W - 20, 23, 13, muted);
		m_wRwCombatHeat = PanelText(RW_COMBAT_PANEL_W - 20, 24, 13, white);

		for (int i = 0; i < RW_COMBAT_MAX_ROWS; i++)
		{
			m_aRwCombatRowBg.Insert(PanelRect(RW_COMBAT_PANEL_W - 16, RW_COMBAT_ROW_H - 3, new Color(0.07, 0.13, 0.16, 0.94)));
			m_aRwCombatRowStripe.Insert(PanelRect(4, RW_COMBAT_ROW_H - 3, title));
			m_aRwCombatRowName.Insert(PanelText(215, 20, 16, white));
			m_aRwCombatRowStatus.Insert(PanelText(134, 20, 15, title));
			m_aRwCombatRowDetail.Insert(PanelText(RW_COMBAT_PANEL_W - 35, 18, 12, muted));
			m_aRwCombatRowBarBg.Insert(PanelRect(RW_COMBAT_PANEL_W - 33, 3, new Color(0.11, 0.24, 0.27, 1)));
			m_aRwCombatRowBar.Insert(PanelRect(RW_COMBAT_PANEL_W - 33, 3, title));
		}

		// Corner brackets sit BEHIND Core's real moving crosshair (Core uses SORT 1000).
		// No fake missile locks, fake targets or duplicate crosshair centre dot.
		for (int k = 0; k < 8; k++)
		{
			if (k < 4)
				m_aRwAimDecor.Insert(PanelRect(16, 2, title));
			else
				m_aRwAimDecor.Insert(PanelRect(2, 16, title));
		}
		m_wRwAimMode = PanelText(220, 22, 14, title);
		m_wRwAimDistance = PanelText(220, 20, 12, muted);
		m_wRwAimContext = PanelText(270, 20, 12, muted);
	}

	//------------------------------------------------------------------------------------------------
	protected void RW_HideCoreWeaponsPanel()
	{
		// Replaces only the OLD weapon list. Core's hull/leg/damage/AMS panels remain.
		Show(m_wWeaponsBg, false);
		Show(m_wWeaponsTitle, false);
		Show(m_wOtherGroups, false);
		for (int i = 0; i < m_aRowName.Count(); i++)
		{
			Show(m_aRowName[i], false);
			Show(m_aRowAmmo[i], false);
			Show(m_aRowBarBg[i], false);
			Show(m_aRowBar[i], false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RW_ShowCombat(bool visible)
	{
		Show(m_wRwCombatPanel, visible);
		Show(m_wRwCombatHeaderBar, visible);
		Show(m_wRwCombatTitle, visible);
		Show(m_wRwCombatMode, visible);
		Show(m_wRwCombatGroups, visible);
		Show(m_wRwCombatHeat, visible);
		for (int i = 0; i < RW_COMBAT_MAX_ROWS; i++)
		{
			Show(m_aRwCombatRowBg[i], false);
			Show(m_aRwCombatRowStripe[i], false);
			Show(m_aRwCombatRowName[i], false);
			Show(m_aRwCombatRowStatus[i], false);
			Show(m_aRwCombatRowDetail[i], false);
			Show(m_aRwCombatRowBarBg[i], false);
			Show(m_aRwCombatRowBar[i], false);
		}
		RW_ShowAim(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void RW_ShowAim(bool visible)
	{
		foreach (ImageWidget corner : m_aRwAimDecor)
			Show(corner, visible);
		Show(m_wRwAimMode, visible);
		Show(m_wRwAimDistance, visible);
		Show(m_wRwAimContext, visible);
	}

	//------------------------------------------------------------------------------------------------
	protected void RW_UpdateAim(IEntity mech, string station, WorkspaceWidget ws, BFA_MechWeaponModuleComponent primary, RW_EquipmentEntry category, Color accent)
	{
		if (!primary || !GetGame().GetWorld())
		{
			RW_ShowAim(false);
			return;
		}

		BFA_MechAimComponent aim = BFA_MechAimComponent.Cast(mech.FindComponent(BFA_MechAimComponent));
		if (!aim)
		{
			RW_ShowAim(false);
			return;
		}

		// Use exactly the same station convergence point as Mech Core's original crosshair.
		vector worldPoint = mech.CoordToParent(aim.GetAimLocalPointForStation(station));
		vector camera[4];
		GetGame().GetWorld().GetCurrentCamera(camera);
		if (vector.Dot(worldPoint - camera[3], camera[2]) < 1)
		{
			RW_ShowAim(false);
			return;
		}

		vector pixel = ws.ProjWorldToScreen(worldPoint, GetGame().GetWorld());
		float x = pixel[0];
		float y = pixel[1];
		float screenW = ws.DPIUnscale(ws.GetWidth());
		float screenH = ws.DPIUnscale(ws.GetHeight());
		if (x < 92 || y < 90 || x > screenW - 300 || y > screenH - 145)
		{
			RW_ShowAim(false);
			return;
		}

		RW_ShowAim(true);
		// Different bank types change the surrounding brackets, not Core's true aim point.
		// ENERGY: compact precision frame, BALLISTIC: mid-size lead frame,
		// MISSILE: wide acquisition frame. None implies an actual target lock.
		float halfSize = 45;
		if (category)
		{
			if (category.m_eType == RW_EHardpointType.ENERGY)
				halfSize = 36;
			else if (category.m_eType == RW_EHardpointType.BALLISTIC)
				halfSize = 49;
			else if (category.m_eType == RW_EHardpointType.MISSILE)
				halfSize = 58;
		}
		float left = x - halfSize;
		float right = x + halfSize - 16;
		float top = y - halfSize;
		float bottom = y + halfSize - 2;
		Place(m_aRwAimDecor[0], left, top);
		Place(m_aRwAimDecor[1], right, top);
		Place(m_aRwAimDecor[2], left, bottom);
		Place(m_aRwAimDecor[3], right, bottom);
		Place(m_aRwAimDecor[4], left, top);
		Place(m_aRwAimDecor[5], x + halfSize - 2, top);
		Place(m_aRwAimDecor[6], left, y + halfSize - 16);
		Place(m_aRwAimDecor[7], x + halfSize - 2, y + halfSize - 16);
		foreach (ImageWidget segment : m_aRwAimDecor)
			segment.SetColor(accent);

		Place(m_wRwAimMode, x + 67, y - 27);
		Place(m_wRwAimDistance, x + 67, y - 6);
		Place(m_wRwAimContext, x + 67, y + 13);
		m_wRwAimMode.SetText(RW_CombatTypeName(category) + " / " + ShortName(primary.GetDisplayName(), 16));
		m_wRwAimMode.SetColor(accent);
		int distance = Math.Round(vector.Distance(camera[3], worldPoint));
		m_wRwAimDistance.SetText(string.Format("AIM POINT %1 m", distance));

		// Report only Core/gameplay values that are actually available on this client.
		// LRM lock authority is server-side in RW_LRMWeapon; do not invent a progress bar.
		string context = "DIRECT FIRE";
		if (RW_LRMWeapon.Cast(primary))
		{
			context = "LRM / SERVER LOCK REQUIRED";
		}
		else if (RW_LaserWeapon.Cast(primary))
		{
			context = string.Format("LASER / RANGE %1 m", Math.Round(primary.GetAimRange()));
		}
		else if (category && category.m_eType == RW_EHardpointType.MISSILE)
		{
			context = "MISSILE / TRAJECTORY";
		}
		else if (primary.GetLeadSpeed() > 0)
		{
			context = string.Format("BALLISTIC / %1 m/s LEAD", Math.Round(primary.GetLeadSpeed()));
		}
		m_wRwAimContext.SetText(context);
		m_wRwAimContext.SetColor(accent);
	}

	//------------------------------------------------------------------------------------------------
	override void Update(IEntity mech, string station)
	{
		super.Update(mech, station);
		if (!mech || !GetGame() || !GetGame().GetWorkspace())
			return;

		if (!m_bRwCombatBuilt)
			RW_BuildCombatHUD();
		RW_HideCoreWeaponsPanel();

		BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(mech.FindComponent(BFA_MechCoreComponent));
		bool canAim = core && core.CanStationAim(station);
		if (!canAim)
		{
			RW_ShowCombat(false);
			return;
		}

		WorkspaceWidget ws = GetGame().GetWorkspace();
		float sw = ws.DPIUnscale(ws.GetWidth());
		float sh = ws.DPIUnscale(ws.GetHeight());
		int count = Math.Min(m_aWeapons.Count(), RW_COMBAT_MAX_ROWS);
		float panelH = 102 + Math.Max(count, 1) * RW_COMBAT_ROW_H;
		float px = sw - RW_COMBAT_PANEL_W - MARGIN;
		float py = Math.Max(60, sh - panelH - MARGIN);
		int group = core.GetSelectedWeaponGroup(station);

		RW_ShowCombat(true);
		FrameSlot.SetSize(m_wRwCombatPanel, RW_COMBAT_PANEL_W, panelH);
		Place(m_wRwCombatPanel, px, py);
		Place(m_wRwCombatHeaderBar, px, py);
		Place(m_wRwCombatTitle, px + 12, py + 9);
		Place(m_wRwCombatMode, px + 12, py + 34);
		Place(m_wRwCombatGroups, px + 12, py + panelH - 44);
		Place(m_wRwCombatHeat, px + 12, py + panelH - 23);
		m_wRwCombatTitle.SetText("CGS  //  WEAPON SYSTEMS");
		m_wRwCombatMode.SetText(string.Format("%1  /  GROUP %2", station, group));

		string fireMode = "GROUP FIRE";
		BFA_MechWeaponComponent router = BFA_MechWeaponComponent.Cast(mech.FindComponent(BFA_MechWeaponComponent));
		if (router && router.IsChainFire(group))
			fireMode = "CHAIN FIRE";
		m_wRwCombatMode.SetText(string.Format("%1  /  GROUP %2  /  %3", station, group, fireMode));

		string others = "OTHER BANKS:";
		foreach (int g : m_aGroups)
		{
			if (g != group)
				others = others + string.Format("   [%1]", g);
		}
		m_wRwCombatGroups.SetText(others);

		CGS_MechHeatComponent heat = CGS_MechHeatComponent.Find(mech);
		string heatInfo = "REACTOR HEAT  --";
		bool shutDown = false;
		bool hot = false;
		if (heat)
		{
			int pct = Math.Round(heat.GetHeat01() * 100);
			heatInfo = string.Format("REACTOR HEAT %1", pct) + "%";
			shutDown = heat.IsShutDown();
			hot = heat.IsOverheated();
			if (shutDown)
				heatInfo = heatInfo + "  /  SHUTDOWN";
			else if (hot)
				heatInfo = heatInfo + "  /  OVERHEAT";
		}
		m_wRwCombatHeat.SetText(heatInfo);
		if (hot)
			m_wRwCombatHeat.SetColor(new Color(1, 0.35, 0.35, 1));
		else
			m_wRwCombatHeat.SetColor(new Color(0.67, 0.82, 0.84, 1));

		RW_EquipmentCatalog catalog = RW_EquipmentCatalog.Get();
		RW_EquipmentEntry firstType;
		BFA_MechWeaponModuleComponent firstWeapon;
		for (int i = 0; i < count; i++)
		{
			BFA_MechWeaponModuleComponent weapon = m_aWeapons[i];
			if (!weapon)
				continue;
			RW_EquipmentEntry type;
			if (catalog)
				type = catalog.Find(weapon.GetModuleId());
			Color color = RW_CombatTypeColor(type);
			if (!firstWeapon)
			{
				firstWeapon = weapon;
				firstType = type;
			}

			float ry = py + 58 + i * RW_COMBAT_ROW_H;
			Show(m_aRwCombatRowBg[i], true);
			Show(m_aRwCombatRowStripe[i], true);
			Show(m_aRwCombatRowName[i], true);
			Show(m_aRwCombatRowStatus[i], true);
			Show(m_aRwCombatRowDetail[i], true);
			Show(m_aRwCombatRowBarBg[i], true);
			Show(m_aRwCombatRowBar[i], true);
			Place(m_aRwCombatRowBg[i], px + 8, ry);
			Place(m_aRwCombatRowStripe[i], px + 8, ry);
			Place(m_aRwCombatRowName[i], px + 19, ry + 3);
			Place(m_aRwCombatRowStatus[i], px + 257, ry + 3);
			Place(m_aRwCombatRowDetail[i], px + 19, ry + 22);
			Place(m_aRwCombatRowBarBg[i], px + 19, ry + 40);
			Place(m_aRwCombatRowBar[i], px + 19, ry + 40);
			m_aRwCombatRowStripe[i].SetColor(color);
			m_aRwCombatRowName[i].SetText(ShortName(weapon.GetDisplayName(), 23));

			string status = "READY";
			if (shutDown)
				status = "SHUTDOWN";
			else if (weapon.IsReloading())
				status = "RELOADING";
			else if (!weapon.HasAmmo())
				status = "EMPTY";
			else if (hot)
				status = "OVERHEAT";
			m_aRwCombatRowStatus[i].SetText(status);
			if (status == "READY")
				m_aRwCombatRowStatus[i].SetColor(new Color(0.52, 0.98, 0.65, 1));
			else
				m_aRwCombatRowStatus[i].SetColor(new Color(1, 0.65, 0.43, 1));

			string hp = ShortName(weapon.GetHardpointName(), 16);
			if (hp.IsEmpty())
				hp = "MODULE";
			string detail = RW_CombatTypeName(type) + "  /  " + hp;
			if (weapon.UsesAmmo())
				detail = detail + string.Format("  /  AMMO %1", AmmoText(weapon));
			else
				detail = detail + string.Format("  /  HEAT +%1", Math.Round(weapon.GetHeatPerShot() * 10) / 10.0);
			m_aRwCombatRowDetail[i].SetText(detail);

			float fill = 1;
			if (weapon.IsReloading())
				fill = weapon.GetReloadProgress();
			else if (weapon.UsesAmmo() && weapon.GetMagazineSize() > 0)
			{
				float magazineSize = weapon.GetMagazineSize();
				fill = weapon.GetRoundsInMagazine() / magazineSize;
			}
			FrameSlot.SetSize(m_aRwCombatRowBar[i], Math.Max(2, (RW_COMBAT_PANEL_W - 33) * Math.Clamp(fill, 0, 1)), 3);
			m_aRwCombatRowBar[i].SetColor(color);
		}

		if (count == 0)
		{
			Show(m_aRwCombatRowName[0], true);
			Show(m_aRwCombatRowStatus[0], true);
			Place(m_aRwCombatRowName[0], px + 18, py + 70);
			Place(m_aRwCombatRowStatus[0], px + 255, py + 70);
			m_aRwCombatRowName[0].SetText("NO FITTED WEAPONS");
			m_aRwCombatRowStatus[0].SetText("UNARMED");
		}
		RW_UpdateAim(mech, station, ws, firstWeapon, firstType, RW_CombatTypeColor(firstType));
	}

	//------------------------------------------------------------------------------------------------
	override void Destroy()
	{
		// Core cleans PanelRect/PanelText widgets. Clear references so a later crew
		// entry rebuilds from scratch after a weapon/loadout change.
		super.Destroy();
		m_bRwCombatBuilt = false;
		m_wRwCombatPanel = null;
		m_wRwCombatHeaderBar = null;
		m_wRwCombatTitle = null;
		m_wRwCombatMode = null;
		m_wRwCombatGroups = null;
		m_wRwCombatHeat = null;
		m_wRwAimMode = null;
		m_wRwAimDistance = null;
		m_wRwAimContext = null;
		m_aRwCombatRowBg.Clear();
		m_aRwCombatRowStripe.Clear();
		m_aRwCombatRowName.Clear();
		m_aRwCombatRowStatus.Clear();
		m_aRwCombatRowDetail.Clear();
		m_aRwCombatRowBarBg.Clear();
		m_aRwCombatRowBar.Clear();
		m_aRwAimDecor.Clear();
	}
}
