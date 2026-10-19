//------------------------------------------------------------------------------------------------
//! REFORGER WARFARE - MECH LAB. Full-screen build screen opened with the Mech Wrench.
//!
//!   left   : frame, tonnage bar, cooling, speed, and the PARTS list
//!   centre : open - the real mech stands right there and changes when the build is applied
//!   right  : HARDPOINTS (what each takes and what is planned on it) and HEATSINKS - / +
//!   bottom : RESET  APPLY  CLOSE and a status line
//!
//! Plan freely, then APPLY sends the whole build; the server checks it with the frame rules and
//! the weapons swap with the normal timer.
//!   - Click a hardpoint to select it, click a part to put it there.
//!   - Or drag a part onto a hardpoint.
//!   - Right-click a hardpoint to empty it.
//! Built in code on a blank layout (UI/RW/RW_MechLab.layout); restyle the colours below.

modded enum ChimeraMenuPreset
{
	RW_MechLab
}

//------------------------------------------------------------------------------------------------
class RW_LabRow
{
	int m_iKind;      // see RW_MechLabMenu.ROW_*
	int m_iValue;     // module id / hardpoint index
	float m_fX;
	float m_fY;
	float m_fW;
	float m_fH;
	ImageWidget m_wBg;
	ImageWidget m_wAccent;
	TextWidget m_wTitle;
	TextWidget m_wSub;
	TextWidget m_wRight;

	bool Contains(float x, float y)
	{
		return x >= m_fX && x <= m_fX + m_fW && y >= m_fY && y <= m_fY + m_fH;
	}
}

//------------------------------------------------------------------------------------------------
class RW_MechLabInput : ScriptedWidgetEventHandler
{
	RW_MechLabMenu m_Menu;

	override bool OnMouseButtonDown(Widget w, int x, int y, int button)
	{
		if (m_Menu)
			m_Menu.OnPress(button);
		return true;
	}

	override bool OnMouseButtonUp(Widget w, int x, int y, int button)
	{
		if (m_Menu)
			m_Menu.OnRelease(button);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class RW_MechLabMenu : ChimeraMenuBase
{
	static const int ROW_PART = 0;
	static const int ROW_HARDPOINT = 1;
	static const int ROW_BUTTON = 2;
	static const int ROW_HS_MINUS = 3;
	static const int ROW_HS_PLUS = 4;

	static const int BTN_RESET = 0;
	static const int BTN_APPLY = 1;
	static const int BTN_CLOSE = 2;

	static const float PANEL_W = 430;
	static const float EDGE = 36;
	static const float ROW_H = 46;
	static const float MAX_DISTANCE = 25;

	// Colours (restyle here).
	protected ref Color m_cPanel = new Color(0.03, 0.04, 0.05, 0.86);
	protected ref Color m_cRow = new Color(1, 1, 1, 0.05);
	protected ref Color m_cRowHover = new Color(1, 1, 1, 0.12);
	protected ref Color m_cRowSelected = new Color(0.95, 0.65, 0.2, 0.28);
	protected ref Color m_cAccent = new Color(0.95, 0.65, 0.2, 1);
	protected ref Color m_cText = new Color(0.93, 0.93, 0.93, 1);
	protected ref Color m_cDim = new Color(0.55, 0.55, 0.55, 1);
	protected ref Color m_cGood = new Color(0.45, 1, 0.55, 1);
	protected ref Color m_cBad = new Color(1, 0.35, 0.3, 1);

	protected static IEntity s_pNextMech;

	protected IEntity m_pMech;
	protected RW_MechFrameComponent m_Frame;
	protected BFA_MechHardpointComponent m_Hardpoints;
	protected CGS_MechHeatComponent m_Heat;

	protected Widget m_wCanvas;
	protected ref RW_MechLabInput m_Input;
	protected ref array<ref RW_LabRow> m_aRows = {};
	protected bool m_bBuilt;

	// Planned build.
	protected ref array<int> m_aPending = {};
	protected int m_iPendingHS;
	protected int m_iSelectedHp;

	// Mouse.
	protected float m_fMouseX;
	protected float m_fMouseY;
	protected int m_iPressRow = -1;
	protected float m_fPressX;
	protected float m_fPressY;
	protected bool m_bDragging;

	// Widgets that change.
	protected TextWidget m_wFrameName;
	protected TextWidget m_wFrameClass;
	protected ImageWidget m_wTonsBg;
	protected ImageWidget m_wTonsFill;
	protected TextWidget m_wTonsText;
	protected TextWidget m_wCooling;
	protected TextWidget m_wSpeed;
	protected TextWidget m_wHsText;
	protected TextWidget m_wStatus;
	protected TextWidget m_wDragGhost;
	protected TextWidget m_wSwapping;

	protected string m_sLocalMessage;
	protected bool m_bLocalBad;
	protected float m_fLocalUntil;
	protected int m_iSeenSeq = -1;
	protected float m_fServerUntil;

	//------------------------------------------------------------------------------------------------
	//! Open the Mech Lab for this mech (local player).
	static void OpenFor(IEntity mech)
	{
		if (!mech || !GetGame() || !GetGame().GetMenuManager())
			return;
		s_pNextMech = mech;
		MenuBase menu = GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.RW_MechLab);
		RW_MechLabMenu lab = RW_MechLabMenu.Cast(menu);
		if (lab)
			lab.SetMech(mech);
		else
			Print("[RW MECH LAB] could not open the menu - is RW_MechLab in Configs/System/chimeraMenus.conf?", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	void SetMech(IEntity mech)
	{
		if (m_pMech == mech && m_bBuilt)
			return;
		m_pMech = mech;
		m_Frame = RW_MechFrameComponent.Find(mech);
		m_Hardpoints = BFA_MechHardpointComponent.Cast(mech.FindComponent(BFA_MechHardpointComponent));
		m_Heat = CGS_MechHeatComponent.Cast(mech.FindComponent(CGS_MechHeatComponent));
		ResetPlan();
	}

	//==================================================================================================
	// MENU LIFETIME
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		if (s_pNextMech)
			SetMech(s_pNextMech);

		InputManager input = GetGame().GetInputManager();
		if (input)
			input.AddActionListener("MenuBack", EActionTrigger.DOWN, OnBack);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		InputManager input = GetGame().GetInputManager();
		if (input)
			input.RemoveActionListener("MenuBack", EActionTrigger.DOWN, OnBack);
		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();
		m_wCanvas = null;
		m_aRows.Clear();
		s_pNextMech = null;
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBack()
	{
		Close();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		if (!m_pMech || !m_Frame || !m_Hardpoints || BFA_MechExtension.IsMechDestroyed(m_pMech))
		{
			Close();
			return;
		}

		// Walked away: close.
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (player && vector.Distance(player.GetOrigin(), m_pMech.GetOrigin()) > MAX_DISTANCE)
		{
			Close();
			return;
		}

		if (!m_bBuilt)
			Build();

		WorkspaceWidget ws = GetGame().GetWorkspace();
		int mx, my;
		WidgetManager.GetMousePos(mx, my);
		m_fMouseX = ws.DPIUnscale(mx);
		m_fMouseY = ws.DPIUnscale(my);

		if (m_iPressRow >= 0 && !m_bDragging && m_aRows[m_iPressRow].m_iKind == ROW_PART)
		{
			if (Math.AbsFloat(m_fMouseX - m_fPressX) + Math.AbsFloat(m_fMouseY - m_fPressY) > 8)
				m_bDragging = true;
		}

		Refresh();
	}

	//==================================================================================================
	// PLAN
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected void ResetPlan()
	{
		m_aPending.Clear();
		if (m_Hardpoints)
		{
			for (int i = 0; i < m_Hardpoints.GetHardpointCount(); i++)
				m_aPending.Insert(m_Hardpoints.GetModuleAt(i));
		}
		m_iPendingHS = 0;
		if (m_Frame)
			m_iPendingHS = m_Frame.GetExtraHeatsinks();
		if (m_iSelectedHp >= m_aPending.Count())
			m_iSelectedHp = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected bool PlanChanged()
	{
		if (!m_Hardpoints || !m_Frame)
			return false;
		if (m_iPendingHS != m_Frame.GetExtraHeatsinks())
			return true;
		for (int i = 0; i < m_aPending.Count(); i++)
		{
			if (m_aPending[i] != m_Hardpoints.GetModuleAt(i))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void Assign(int hardpointIndex, int moduleId)
	{
		if (hardpointIndex < 0 || hardpointIndex >= m_aPending.Count())
			return;
		string reason;
		if (!m_Frame.CheckItem(m_Hardpoints, hardpointIndex, moduleId, reason))
		{
			Say(string.Format("%1 can't go on %2: %3", PartName(moduleId), m_Hardpoints.GetHardpointName(hardpointIndex), reason), true);
			return;
		}
		m_aPending[hardpointIndex] = moduleId;
		m_iSelectedHp = hardpointIndex;
		if (moduleId == 0)
			Say(string.Format("%1 will be emptied", m_Hardpoints.GetHardpointName(hardpointIndex)), false);
		else
			Say(string.Format("%1 planned on %2", PartName(moduleId), m_Hardpoints.GetHardpointName(hardpointIndex)), false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Apply()
	{
		if (m_Hardpoints.IsSwapInProgress())
		{
			Say("Still working on the last change", true);
			return;
		}
		string reason;
		if (!m_Frame.CheckBuild(m_Hardpoints, m_aPending, m_iPendingHS, reason))
		{
			Say("Can't apply: " + reason, true);
			return;
		}
		if (!PlanChanged())
		{
			Say("Nothing to change", false);
			return;
		}

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.RW_RequestBuild(m_pMech, m_aPending, m_iPendingHS);
		Say("Sent to the crew...", false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Say(string message, bool bad)
	{
		m_sLocalMessage = message;
		m_bLocalBad = bad;
		m_fLocalUntil = Now() + 4;
	}

	//------------------------------------------------------------------------------------------------
	protected float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;
		return GetGame().GetWorld().GetWorldTime() * 0.001;
	}

	//------------------------------------------------------------------------------------------------
	protected RW_EquipmentEntry Item(int moduleId)
	{
		BFA_MechModuleDefinition module = m_Hardpoints.GetModuleDefinition(moduleId);
		if (!module)
			return null;
		return RW_EquipmentCatalog.Get().Find(module.m_sWeaponModuleId);
	}

	//------------------------------------------------------------------------------------------------
	protected string PartName(int moduleId)
	{
		if (moduleId == 0)
			return "Empty";
		BFA_MechModuleDefinition module = m_Hardpoints.GetModuleDefinition(moduleId);
		if (!module)
			return "?";
		RW_EquipmentEntry item = Item(moduleId);
		if (item && !item.m_sDisplayName.IsEmpty())
			return item.m_sDisplayName;
		return module.m_sLabel;
	}

	//------------------------------------------------------------------------------------------------
	protected float PartTons(int moduleId)
	{
		return RW_MechFrameComponent.ModuleTons(m_Hardpoints.GetModuleDefinition(moduleId));
	}

	//------------------------------------------------------------------------------------------------
	protected string Tons(float tons)
	{
		return (Math.Round(tons * 10) / 10).ToString() + " t";
	}

	//==================================================================================================
	// INPUT
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected int RowAt(float x, float y)
	{
		for (int i = m_aRows.Count() - 1; i >= 0; i--)
		{
			if (m_aRows[i].Contains(x, y))
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	void OnPress(int button)
	{
		int row = RowAt(m_fMouseX, m_fMouseY);
		if (row < 0)
			return;
		RW_LabRow r = m_aRows[row];

		// Right-click a hardpoint: empty it.
		if (button == 1)
		{
			if (r.m_iKind == ROW_HARDPOINT)
				Assign(r.m_iValue, 0);
			return;
		}
		if (button != 0)
			return;

		m_iPressRow = row;
		m_fPressX = m_fMouseX;
		m_fPressY = m_fMouseY;
		m_bDragging = false;
	}

	//------------------------------------------------------------------------------------------------
	void OnRelease(int button)
	{
		if (button != 0 || m_iPressRow < 0 || m_iPressRow >= m_aRows.Count())
		{
			m_iPressRow = -1;
			m_bDragging = false;
			return;
		}

		RW_LabRow pressed = m_aRows[m_iPressRow];
		int overIndex = RowAt(m_fMouseX, m_fMouseY);
		bool wasDrag = m_bDragging;
		m_iPressRow = -1;
		m_bDragging = false;

		// Dropped a part on a hardpoint.
		if (wasDrag)
		{
			if (overIndex >= 0 && m_aRows[overIndex].m_iKind == ROW_HARDPOINT)
				Assign(m_aRows[overIndex].m_iValue, pressed.m_iValue);
			return;
		}

		if (overIndex < 0 || m_aRows[overIndex] != pressed)
			return;

		if (pressed.m_iKind == ROW_PART)
		{
			Assign(m_iSelectedHp, pressed.m_iValue);
		}
		else if (pressed.m_iKind == ROW_HARDPOINT)
		{
			m_iSelectedHp = pressed.m_iValue;
		}
		else if (pressed.m_iKind == ROW_HS_MINUS)
		{
			m_iPendingHS = Math.Max(m_iPendingHS - 1, 0);
		}
		else if (pressed.m_iKind == ROW_HS_PLUS)
		{
			m_iPendingHS = Math.Min(m_iPendingHS + 1, m_Frame.GetMaxExtraHeatsinks());
		}
		else if (pressed.m_iKind == ROW_BUTTON)
		{
			if (pressed.m_iValue == BTN_RESET)
			{
				ResetPlan();
				Say("Plan reset to what is fitted", false);
			}
			else if (pressed.m_iValue == BTN_APPLY)
			{
				Apply();
			}
			else
			{
				Close();
			}
		}
	}

	//==================================================================================================
	// BUILD THE SCREEN
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected ImageWidget Rect(float x, float y, float w, float h, Color color, int z = 0)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		ImageWidget img = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS | WidgetFlags.STRETCH, color, z, m_wCanvas));
		if (!img)
			return null;
		img.SetColor(color);
		FrameSlot.SetAnchorMin(img, 0, 0);
		FrameSlot.SetAnchorMax(img, 0, 0);
		FrameSlot.SetAlignment(img, 0, 0);
		FrameSlot.SetPos(img, x, y);
		FrameSlot.SetSize(img, w, h);
		return img;
	}

	//------------------------------------------------------------------------------------------------
	protected TextWidget Text(float x, float y, float w, float h, int size, Color color, string text = "", int z = 5)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		TextWidget txt = TextWidget.Cast(ws.CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, color, z, m_wCanvas));
		if (!txt)
			return null;
		txt.SetColor(color);
		txt.SetExactFontSize(size);
		FrameSlot.SetAnchorMin(txt, 0, 0);
		FrameSlot.SetAnchorMax(txt, 0, 0);
		FrameSlot.SetAlignment(txt, 0, 0);
		FrameSlot.SetPos(txt, x, y);
		FrameSlot.SetSize(txt, w, h);
		txt.SetText(text);
		return txt;
	}

	//------------------------------------------------------------------------------------------------
	protected RW_LabRow AddRow(int kind, int value, float x, float y, float w, float h)
	{
		RW_LabRow row = new RW_LabRow();
		row.m_iKind = kind;
		row.m_iValue = value;
		row.m_fX = x;
		row.m_fY = y;
		row.m_fW = w;
		row.m_fH = h;
		row.m_wBg = Rect(x, y, w, h, m_cRow, 1);
		row.m_wAccent = Rect(x, y, 4, h, m_cAccent, 2);
		m_aRows.Insert(row);
		return row;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddButton(int which, string label, float x, float y, float w)
	{
		RW_LabRow row = AddRow(ROW_BUTTON, which, x, y, w, 48);
		row.m_wTitle = Text(x, y + 13, w, 24, 18, m_cText, label);
		row.m_wTitle.SetFlags(WidgetFlags.CENTER);
	}

	//------------------------------------------------------------------------------------------------
	protected void Build()
	{
		m_bBuilt = true;
		WorkspaceWidget ws = GetGame().GetWorkspace();
		Widget root = GetRootWidget();

		// Full-screen canvas that takes the mouse; everything else ignores the cursor.
		m_wCanvas = ws.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE, new Color(1, 1, 1, 1), 50, root);
		FrameSlot.SetAnchorMin(m_wCanvas, 0, 0);
		FrameSlot.SetAnchorMax(m_wCanvas, 1, 1);
		FrameSlot.SetOffsets(m_wCanvas, 0, 0, 0, 0);
		m_Input = new RW_MechLabInput();
		m_Input.m_Menu = this;
		m_wCanvas.AddHandler(m_Input);

		float sw = ws.DPIUnscale(ws.GetWidth());
		float sh = ws.DPIUnscale(ws.GetHeight());
		float top = EDGE;
		float bottom = sh - EDGE - 70;

		// ---------------- Left panel: frame + parts ----------------
		float lx = EDGE;
		Rect(lx, top, PANEL_W, bottom - top, m_cPanel, 0);
		Rect(lx, top, PANEL_W, 3, m_cAccent, 1);
		Text(lx + 20, top + 14, PANEL_W - 40, 20, 13, m_cAccent, "MECH LAB");
		m_wFrameName = Text(lx + 20, top + 34, PANEL_W - 40, 34, 28, m_cText);
		m_wFrameClass = Text(lx + 20, top + 70, PANEL_W - 40, 20, 14, m_cDim);

		float sy = top + 104;
		Text(lx + 20, sy, 200, 18, 13, m_cDim, "TONNAGE");
		m_wTonsText = Text(lx + 220, sy, PANEL_W - 240, 18, 14, m_cText);
		m_wTonsBg = Rect(lx + 20, sy + 24, PANEL_W - 40, 12, new Color(1, 1, 1, 0.1), 1);
		m_wTonsFill = Rect(lx + 20, sy + 24, PANEL_W - 40, 12, m_cGood, 2);
		m_wCooling = Text(lx + 20, sy + 48, PANEL_W - 40, 18, 14, m_cText);
		m_wSpeed = Text(lx + 20, sy + 70, PANEL_W - 40, 18, 14, m_cText);

		float py = sy + 108;
		Text(lx + 20, py, 300, 18, 13, m_cAccent, "PARTS  (click = put on selected · drag onto a hardpoint)");
		py += 26;

		array<int> parts = {0};
		for (int id = 1; id <= 31; id++)
		{
			if (m_Hardpoints.GetModuleDefinition(id))
				parts.Insert(id);
		}
		foreach (int partId : parts)
		{
			if (py + ROW_H > bottom - 10)
				break;
			RW_LabRow part = AddRow(ROW_PART, partId, lx + 14, py, PANEL_W - 28, ROW_H - 6);
			part.m_wTitle = Text(lx + 28, py + 4, 260, 22, 17, m_cText);
			part.m_wSub = Text(lx + 28, py + 24, 300, 16, 12, m_cDim);
			part.m_wRight = Text(lx + PANEL_W - 128, py + 10, 100, 20, 16, m_cText);
			py += ROW_H;
		}

		// ---------------- Right panel: hardpoints + heatsinks ----------------
		float rx = sw - EDGE - PANEL_W;
		Rect(rx, top, PANEL_W, bottom - top, m_cPanel, 0);
		Rect(rx, top, PANEL_W, 3, m_cAccent, 1);
		Text(rx + 20, top + 14, PANEL_W - 40, 20, 13, m_cAccent, "HARDPOINTS  (click = select · right-click = empty)");

		float hy = top + 44;
		for (int hp = 0; hp < m_Hardpoints.GetHardpointCount(); hp++)
		{
			if (hy + 64 > bottom - 140)
				break;
			RW_LabRow slot = AddRow(ROW_HARDPOINT, hp, rx + 14, hy, PANEL_W - 28, 58);
			slot.m_wTitle = Text(rx + 28, hy + 6, 260, 20, 15, m_cAccent);
			slot.m_wSub = Text(rx + 28, hy + 30, 380, 22, 18, m_cText);
			slot.m_wRight = Text(rx + PANEL_W - 168, hy + 8, 140, 18, 12, m_cDim);
			hy += 64;
		}

		// Heatsinks.
		float hsY = bottom - 120;
		Text(rx + 20, hsY, PANEL_W - 40, 18, 13, m_cAccent, "HEATSINKS");
		m_wHsText = Text(rx + 20, hsY + 26, PANEL_W - 150, 44, 16, m_cText);
		RW_LabRow minus = AddRow(ROW_HS_MINUS, 0, rx + PANEL_W - 124, hsY + 22, 48, 44);
		minus.m_wTitle = Text(rx + PANEL_W - 124, hsY + 30, 48, 28, 24, m_cText, "-");
		minus.m_wTitle.SetFlags(WidgetFlags.CENTER);
		RW_LabRow plus = AddRow(ROW_HS_PLUS, 0, rx + PANEL_W - 68, hsY + 22, 48, 44);
		plus.m_wTitle = Text(rx + PANEL_W - 68, hsY + 30, 48, 28, 24, m_cText, "+");
		plus.m_wTitle.SetFlags(WidgetFlags.CENTER);

		// ---------------- Bottom bar ----------------
		float by = sh - EDGE - 56;
		float bw = 180;
		float bx = sw * 0.5 - (bw * 3 + 24) * 0.5;
		Rect(lx, by - 8, sw - EDGE * 2, 64, m_cPanel, 0);
		AddButton(BTN_RESET, "RESET", bx, by, bw);
		AddButton(BTN_APPLY, "APPLY", bx + bw + 12, by, bw);
		AddButton(BTN_CLOSE, "CLOSE", bx + (bw + 12) * 2, by, bw);
		m_wStatus = Text(lx + 20, by + 14, bx - lx - 40, 24, 16, m_cText);
		m_wSwapping = Text(bx + (bw + 12) * 3 + 20, by + 14, 400, 24, 16, m_cAccent);

		m_wDragGhost = Text(0, 0, 300, 26, 18, m_cAccent, "", 20);
		m_wDragGhost.SetVisible(false);
	}

	//==================================================================================================
	// REFRESH (every frame)
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected void Refresh()
	{
		int hover = RowAt(m_fMouseX, m_fMouseY);
		float tons = m_Frame.BuildTons(m_Hardpoints, m_aPending, m_iPendingHS);
		float maxTons = m_Frame.GetMaxTons();
		bool over = tons > maxTons + 0.001;

		// Frame + stats.
		m_wFrameName.SetText(m_Frame.GetFrameName());
		m_wFrameClass.SetText(typename.EnumToString(RW_EWeightClass, m_Frame.GetWeightClass()) + " FRAME  ·  empty " + Tons(m_Frame.GetFrameTons()));
		m_wTonsText.SetText(Tons(tons) + " / " + Tons(maxTons));
		float used = Math.Clamp(tons / Math.Max(maxTons, 1), 0, 1);
		FrameSlot.SetSize(m_wTonsFill, Math.Max((PANEL_W - 40) * used, 1), 12);
		if (over)
			m_wTonsFill.SetColor(m_cBad);
		else if (used > 0.9)
			m_wTonsFill.SetColor(new Color(1, 0.8, 0.25, 1));
		else
			m_wTonsFill.SetColor(m_cGood);

		float baseCooling = 0;
		if (m_Heat)
			baseCooling = m_Heat.GetCoolingPerSecond();
		float sinkCooling = 0;
		RW_EquipmentEntry sink = RW_EquipmentCatalog.Get().GetHeatsink();
		if (sink)
			sinkCooling = sink.m_fCooling * m_iPendingHS;
		m_wCooling.SetText(string.Format("COOLING   %1 heat/s   (frame %2 + heatsinks %3)", Math.Round((baseCooling + sinkCooling) * 10) / 10, baseCooling, Math.Round(sinkCooling * 10) / 10));
		m_wSpeed.SetText("TOP SPEED   " + Math.Round(m_Frame.SpeedScaleForTons(tons) * 100).ToString() + "%");

		// Parts and hardpoints.
		for (int r = 0; r < m_aRows.Count(); r++)
		{
			RW_LabRow row = m_aRows[r];
			bool hovered = r == hover;
			if (row.m_iKind == ROW_PART)
				RefreshPart(row, hovered);
			else if (row.m_iKind == ROW_HARDPOINT)
				RefreshHardpoint(row, hovered);
			else
				RefreshButton(row, hovered, over);
		}

		// Heatsinks.
		string sinkLine = string.Format("%1 extra  (max %2)", m_iPendingHS, m_Frame.GetMaxExtraHeatsinks());
		if (sink)
			sinkLine = sinkLine + string.Format("\n+%1 heat/s · %2", Math.Round(sinkCooling * 10) / 10, Tons(sink.m_fTons * m_iPendingHS));
		m_wHsText.SetText(sinkLine);

		// Drag ghost.
		bool dragging = m_bDragging && m_iPressRow >= 0;
		m_wDragGhost.SetVisible(dragging);
		if (dragging)
		{
			FrameSlot.SetPos(m_wDragGhost, m_fMouseX + 16, m_fMouseY + 8);
			m_wDragGhost.SetText(PartName(m_aRows[m_iPressRow].m_iValue));
		}

		// Swap running.
		if (m_Hardpoints.IsSwapInProgress())
			m_wSwapping.SetText("CREW AT WORK...");
		else
			m_wSwapping.SetText("");

		RefreshStatus(over, tons, maxTons);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshPart(RW_LabRow row, bool hovered)
	{
		int id = row.m_iValue;
		string reason;
		bool fits = m_Frame.CheckItem(m_Hardpoints, m_iSelectedHp, id, reason);

		row.m_wTitle.SetText(PartName(id));
		RW_EquipmentEntry item = Item(id);
		string sub = "remove whatever is on the hardpoint";
		if (id != 0)
		{
			sub = "";
			if (item)
				sub = typename.EnumToString(RW_EHardpointType, item.m_eType) + " · " + typename.EnumToString(RW_ESize, item.m_eSize);
			if (!fits)
				sub = sub + "  —  " + reason;
		}
		row.m_wSub.SetText(sub);
		if (id == 0)
			row.m_wRight.SetText("");
		else
			row.m_wRight.SetText(Tons(PartTons(id)));

		Color title = m_cText;
		if (!fits)
			title = m_cDim;
		row.m_wTitle.SetColor(title);
		row.m_wRight.SetColor(title);
		if (!fits)
			row.m_wSub.SetColor(m_cBad);
		else
			row.m_wSub.SetColor(m_cDim);

		bool planned = m_iSelectedHp >= 0 && m_iSelectedHp < m_aPending.Count() && m_aPending[m_iSelectedHp] == id;
		row.m_wAccent.SetVisible(planned);
		if (hovered && fits)
			row.m_wBg.SetColor(m_cRowHover);
		else
			row.m_wBg.SetColor(m_cRow);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshHardpoint(RW_LabRow row, bool hovered)
	{
		int hp = row.m_iValue;
		string name = m_Hardpoints.GetHardpointName(hp);
		RW_FrameHardpoint rules = m_Frame.FindHardpoint(name);
		string accepts = "takes anything";
		if (rules)
		{
			accepts = typename.EnumToString(RW_EHardpointType, rules.m_eType) + " · up to " + typename.EnumToString(RW_ESize, rules.m_eMaxSize);
			if (rules.m_eType == RW_EHardpointType.ANY)
				accepts = "ANY · up to " + typename.EnumToString(RW_ESize, rules.m_eMaxSize);
		}

		int planned = m_aPending[hp];
		int fitted = m_Hardpoints.GetModuleAt(hp);
		row.m_wTitle.SetText(name);
		row.m_wRight.SetText(accepts);

		string content = PartName(planned);
		if (planned != fitted)
			content = content + "   (now: " + PartName(fitted) + ")";
		row.m_wSub.SetText(content);
		if (planned != fitted)
			row.m_wSub.SetColor(new Color(1, 0.8, 0.3, 1));
		else if (planned == 0)
			row.m_wSub.SetColor(m_cDim);
		else
			row.m_wSub.SetColor(m_cText);

		bool selected = hp == m_iSelectedHp;
		bool dropTarget = m_bDragging && hovered;
		row.m_wAccent.SetVisible(selected);
		if (selected || dropTarget)
			row.m_wBg.SetColor(m_cRowSelected);
		else if (hovered)
			row.m_wBg.SetColor(m_cRowHover);
		else
			row.m_wBg.SetColor(m_cRow);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshButton(RW_LabRow row, bool hovered, bool over)
	{
		row.m_wAccent.SetVisible(false);
		bool enabled = true;
		if (row.m_iKind == ROW_BUTTON && row.m_iValue == BTN_APPLY)
		{
			enabled = PlanChanged() && !over && !m_Hardpoints.IsSwapInProgress();
			if (enabled)
				row.m_wBg.SetColor(new Color(0.95, 0.65, 0.2, 0.55));
			else
				row.m_wBg.SetColor(m_cRow);
			if (row.m_wTitle)
			{
				if (enabled)
					row.m_wTitle.SetColor(m_cText);
				else
					row.m_wTitle.SetColor(m_cDim);
			}
			if (hovered && enabled)
				row.m_wBg.SetColor(new Color(0.95, 0.65, 0.2, 0.8));
			return;
		}
		if (hovered)
			row.m_wBg.SetColor(m_cRowHover);
		else
			row.m_wBg.SetColor(m_cRow);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshStatus(bool over, float tons, float maxTons)
	{
		float now = Now();

		// Answer from the server (accepted / refused).
		int seq = m_Frame.GetCrewMessageSeq();
		if (m_iSeenSeq < 0)
			m_iSeenSeq = seq;
		if (seq != m_iSeenSeq)
		{
			m_iSeenSeq = seq;
			m_fServerUntil = now + 5;
			m_fLocalUntil = 0;
		}

		string text;
		Color color = m_cText;
		if (now < m_fServerUntil)
		{
			text = m_Frame.GetCrewMessage();
			if (text.StartsWith("REFUSED"))
				color = m_cBad;
			else
				color = m_cGood;
		}
		else if (now < m_fLocalUntil)
		{
			text = m_sLocalMessage;
			if (m_bLocalBad)
				color = m_cBad;
		}
		else if (over)
		{
			text = string.Format("Too heavy by %1 - remove something", Tons(tons - maxTons));
			color = m_cBad;
		}
		else if (PlanChanged())
		{
			text = "Plan ready - APPLY to fit it";
			color = new Color(1, 0.8, 0.3, 1);
		}
		else
		{
			text = "Select a hardpoint, then pick a part";
			color = m_cDim;
		}
		m_wStatus.SetText(text);
		m_wStatus.SetColor(color);
	}
}
