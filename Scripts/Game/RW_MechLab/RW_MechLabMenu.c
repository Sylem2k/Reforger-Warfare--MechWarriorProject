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
//!   - Markers on the mech show where each hardpoint is; click one to select it, drop parts on it.
//!   - VIEW < > turns the showroom camera around the mech.
//! Built in code on a blank layout (UI/RW/RW_MechLab.layout); restyle the colours below.
//! Everything is laid out on a 1080p-high design screen and scaled by UI_SCALE.

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
	TextWidget m_wStats;
	bool m_bHidden;

	bool Contains(float x, float y)
	{
		if (m_bHidden)
			return false;
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

	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		if (m_Menu)
			m_Menu.OnWheel(wheel);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Showroom camera: looks at the mech from the front, VIEW < > turns it around the mech.
//! If the camera can't be made, the lab still works with the normal player view.
class RW_MechLabCamera
{
	protected CameraBase m_Cam;
	protected CameraBase m_Prev;
	protected IEntity m_pMech;
	protected float m_fYaw;          // degrees around the mech, 0 = in front
	protected float m_fTargetYaw;
	protected vector m_vCentre;      // mech local
	protected float m_fDist;
	protected float m_fHeight;

	//------------------------------------------------------------------------------------------------
	bool Start(IEntity mech)
	{
		CameraManager cameras = GetGame().GetCameraManager();
		if (!mech || !cameras)
			return false;

		m_pMech = mech;
		vector mins, maxs;
		mech.GetBounds(mins, maxs);
		vector size = maxs - mins;
		m_fHeight = Math.Max(size[1], 2);
		float wide = Math.Max(size[0], size[2]);
		m_vCentre = (mins + maxs) * 0.5;
		m_fDist = Math.Max(m_fHeight, wide) * 1.35 + 2;

		vector mat[4];
		Pose(mat);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = mat[0];
		params.Transform[1] = mat[1];
		params.Transform[2] = mat[2];
		params.Transform[3] = mat[3];

		m_Cam = CameraBase.Cast(GetGame().SpawnEntity(CameraBase, mech.GetWorld(), params));
		if (!m_Cam)
			return false;

		m_Prev = cameras.CurrentCamera();
		cameras.SetCamera(m_Cam);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Turn(float degrees)
	{
		m_fTargetYaw = m_fTargetYaw + degrees;
	}

	//------------------------------------------------------------------------------------------------
	void Update(float timeSlice)
	{
		if (!m_Cam || !m_pMech)
			return;
		float blend = Math.Clamp(timeSlice * 6, 0, 1);
		m_fYaw = m_fYaw + (m_fTargetYaw - m_fYaw) * blend;
		vector mat[4];
		Pose(mat);
		m_Cam.SetWorldTransform(mat);
	}

	//------------------------------------------------------------------------------------------------
	protected void Pose(out vector mat[4])
	{
		float rad = m_fYaw * Math.DEG2RAD;
		vector local = m_vCentre + Vector(Math.Sin(rad) * m_fDist, m_fHeight * 0.15, Math.Cos(rad) * m_fDist);
		vector pos = m_pMech.CoordToParent(local);
		vector target = m_pMech.CoordToParent(m_vCentre);
		vector dir = (target - pos).Normalized();
		Math3D.DirectionAndUpMatrix(dir, vector.Up, mat);
		mat[3] = pos;
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras && m_Cam && m_Prev && cameras.CurrentCamera() == m_Cam)
			cameras.SetCamera(m_Prev);
		if (m_Cam)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Cam);
		m_Cam = null;
		m_Prev = null;
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
	static const int ROW_MARKER = 5;
	static const int ROW_ORDER = 6;     // value = frame index * 100 + saved slot (0 = bare)
	static const int ROW_DELETE = 7;
	static const int ROW_FILTER = 9;    // filter parts by family
	static const int ROW_SCROLL = 8;    // value -1 = up, 1 = down (parts list)    // value = frame index * 100 + saved slot

	static const int BTN_RESET = 0;
	static const int BTN_APPLY = 1;
	static const int BTN_CLOSE = 2;
	static const int BTN_VIEW_LEFT = 3;
	static const int BTN_VIEW_RIGHT = 4;
	static const int BTN_SAVE = 5;

	static const float UI_SCALE = 1.3;      // bigger = everything bigger
	static const float PANEL_W = 410;
	static const float EDGE = 30;
	static const float ROW_H = 66;
	static const float MAX_DISTANCE = 25;
	static const bool SHOWROOM_CAMERA = true;

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
	protected static RW_MechBayComponent s_pNextBay;

	// Order Frame mode (bay with an empty pad): pick a frame, the bay builds it, the lab opens.
	protected RW_MechBayComponent m_OrderBay;
	protected float m_fNextPadLook;
	protected int m_iOrderSeenSeq = -1;
	protected float m_fOrderMsgUntil;

	// Saved builds (this player's, from the server).
	protected static string s_sBuildsData;
	protected static int s_iBuildsSeq;
	protected int m_iBuildsSeenSeq = -1;
	protected bool m_bBuildsRequested;
	protected ref array<ref RW_SavedBuild> m_aBuilds = {};

	protected IEntity m_pMech;
	protected RW_MechFrameComponent m_Frame;
	protected BFA_MechHardpointComponent m_Hardpoints;
	protected CGS_MechHeatComponent m_Heat;

	protected Widget m_wCanvas;
	protected float m_fS = 1;               // design units -> screen units
	protected float m_fLeftEdge;            // inner edges of the side panels (design units)
	protected float m_fRightEdge;
	protected float m_fBottomEdge;
	protected float m_fBarW;
	protected ref RW_MechLabCamera m_Camera;
	protected ref RW_MechLabInput m_Input;
	protected ref array<ref RW_LabRow> m_aRows = {};
	protected bool m_bBuilt;
	protected int m_iBuildTries;
	protected int m_iPartFilter;        // 0 All, 1 Energy, 2 Ballistic, 3 Missile, 4 Support/Melee
	protected int m_iPartScroll;        // first part shown (after Empty)
	protected int m_iPartCount;         // parts besides Empty
	protected int m_iPartsShown;

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
	protected TextWidget m_wFireHeat;
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
	//! The server sent this player's saved builds.
	static void ReceiveBuilds(string data)
	{
		s_sBuildsData = data;
		s_iBuildsSeq++;
	}

	//------------------------------------------------------------------------------------------------
	//! Open the bay's Order Frame screen (local player).
	static void OpenOrder(RW_MechBayComponent bay)
	{
		if (!bay || !GetGame() || !GetGame().GetMenuManager())
			return;
		s_pNextMech = null;
		s_pNextBay = bay;
		MenuBase menu = GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.RW_MechLab);
		RW_MechLabMenu lab = RW_MechLabMenu.Cast(menu);
		if (lab)
			lab.m_OrderBay = bay;
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
		else if (s_pNextBay)
			m_OrderBay = s_pNextBay;

		if (SHOWROOM_CAMERA && m_pMech)
		{
			m_Camera = new RW_MechLabCamera();
			if (!m_Camera.Start(m_pMech))
				m_Camera = null;
		}

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
		if (m_Camera)
			m_Camera.Stop();
		m_Camera = null;
		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();
		m_wCanvas = null;
		m_aRows.Clear();
		s_pNextMech = null;
		s_pNextBay = null;
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

		if (!m_pMech && m_OrderBay)
		{
			OrderUpdate();
			return;
		}

		if (!m_pMech || !m_Frame || !m_Hardpoints || BFA_MechExtension.IsMechDestroyed(m_pMech))
		{
			Close();
			return;
		}

		// Walked away: close.
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		RW_MechBayComponent bay = RW_MechBayComponent.FindBayFor(m_pMech);
		bool atBay = bay && bay.CanOperate(player, m_pMech);
		if (player && !atBay && vector.Distance(player.GetOrigin(), m_pMech.GetOrigin()) > MAX_DISTANCE)
		{
			Close();
			return;
		}

		if (!m_bBuilt && !Build())
			return;

		WorkspaceWidget ws = GetGame().GetWorkspace();
		int mx, my;
		WidgetManager.GetMousePos(mx, my);
		m_fMouseX = ws.DPIUnscale(mx) / m_fS;
		m_fMouseY = ws.DPIUnscale(my) / m_fS;

		if (m_Camera)
			m_Camera.Update(tDelta);

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
			if (r.m_iKind == ROW_HARDPOINT || r.m_iKind == ROW_MARKER)
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
			if (overIndex >= 0 && (m_aRows[overIndex].m_iKind == ROW_HARDPOINT || m_aRows[overIndex].m_iKind == ROW_MARKER))
				Assign(m_aRows[overIndex].m_iValue, pressed.m_iValue);
			return;
		}

		if (overIndex < 0 || m_aRows[overIndex] != pressed)
			return;

		if (pressed.m_iKind == ROW_FILTER)
		{
			m_iPartFilter = pressed.m_iValue;
			m_iPartScroll = 0;
			ScrollParts(0, true);
		}
		else if (pressed.m_iKind == ROW_SCROLL)
		{
			ScrollParts(pressed.m_iValue);
		}
		else if (pressed.m_iKind == ROW_ORDER)
		{
			OrderFrame(pressed.m_iValue / 100, pressed.m_iValue % 100);
		}
		else if (pressed.m_iKind == ROW_DELETE)
		{
			DeleteBuild(pressed.m_iValue / 100, pressed.m_iValue % 100);
		}
		else if (pressed.m_iKind == ROW_PART)
		{
			Assign(m_iSelectedHp, pressed.m_iValue);
		}
		else if (pressed.m_iKind == ROW_HARDPOINT || pressed.m_iKind == ROW_MARKER)
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
			else if (pressed.m_iValue == BTN_VIEW_LEFT)
			{
				if (m_Camera)
					m_Camera.Turn(-45);
			}
			else if (pressed.m_iValue == BTN_SAVE)
			{
				SaveBuild();
			}
			else if (pressed.m_iValue == BTN_VIEW_RIGHT)
			{
				if (m_Camera)
					m_Camera.Turn(45);
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
		FrameSlot.SetPos(img, x * m_fS, y * m_fS);
		FrameSlot.SetSize(img, w * m_fS, h * m_fS);
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
		txt.SetExactFontSize(Math.Round(size * m_fS));
		FrameSlot.SetAnchorMin(txt, 0, 0);
		FrameSlot.SetAnchorMax(txt, 0, 0);
		FrameSlot.SetAlignment(txt, 0, 0);
		FrameSlot.SetPos(txt, x * m_fS, y * m_fS);
		FrameSlot.SetSize(txt, w * m_fS, h * m_fS);
		txt.SetText(text);
		return txt;
	}

	//------------------------------------------------------------------------------------------------
	//! Move / resize a widget in design units.
	protected void PlaceS(Widget w, float x, float y)
	{
		FrameSlot.SetPos(w, x * m_fS, y * m_fS);
	}

	protected void SizeS(Widget w, float width, float height)
	{
		FrameSlot.SetSize(w, width * m_fS, height * m_fS);
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
	//! Builds the screen once the canvas knows its real size (the frame after it is made).
	//! The canvas size is used instead of the workspace size, because in a Workbench window the
	//! workspace can be wider than the game view (that pushed the right panel off screen).
	protected bool PrepareCanvas(out float sw, out float sh)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		Widget root = GetRootWidget();

		if (!m_wCanvas)
		{
			// Full-screen canvas that takes the mouse; everything else ignores the cursor.
			m_wCanvas = ws.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE, new Color(1, 1, 1, 1), 50, root);
			FrameSlot.SetAnchorMin(m_wCanvas, 0, 0);
			FrameSlot.SetAnchorMax(m_wCanvas, 1, 1);
			FrameSlot.SetOffsets(m_wCanvas, 0, 0, 0, 0);
			m_Input = new RW_MechLabInput();
			m_Input.m_Menu = this;
			m_wCanvas.AddHandler(m_Input);
			return false;
		}

		float canvasW, canvasH;
		m_wCanvas.GetScreenSize(canvasW, canvasH);
		m_iBuildTries++;
		if ((canvasW < 50 || canvasH < 50) && m_iBuildTries < 10)
			return false;
		if (canvasW < 50 || canvasH < 50)
		{
			canvasW = ws.GetWidth();
			canvasH = ws.GetHeight();
		}

		// Design screen: 1080 / UI_SCALE units high, scaled up to the real screen.
		float realW = ws.DPIUnscale(canvasW);
		float realH = ws.DPIUnscale(canvasH);
		m_fS = Math.Clamp(realH / 1080 * UI_SCALE, 0.75, 3);
		sw = realW / m_fS;
		sh = realH / m_fS;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Build()
	{
		float sw, sh;
		if (!PrepareCanvas(sw, sh))
			return false;
		m_bBuilt = true;

		// Narrow screens: shrink the panels so the mech stays visible in the middle.
		float panelW = Math.Min(PANEL_W, (sw - EDGE * 2) * 0.32);
		float top = EDGE;
		float bottom = sh - EDGE - 70;

		// ---------------- Left panel: frame + parts ----------------
		float lx = EDGE;
		Rect(lx, top, panelW, bottom - top, m_cPanel, 0);
		Rect(lx, top, panelW, 3, m_cAccent, 1);
		string header = "MECH LAB  |  FIELD REFIT";
		RW_MechBayComponent bayHere = RW_MechBayComponent.FindBayFor(m_pMech);
		if (bayHere)
			header = "MECH LAB  |  " + bayHere.GetBayName();
		Text(lx + 18, top + 12, panelW - 36, 20, 14, m_cAccent, header);
		m_wFrameName = Text(lx + 18, top + 32, panelW - 36, 34, 28, m_cText);
		m_wFrameClass = Text(lx + 18, top + 66, panelW - 36, 20, 14, m_cDim);

		float sy = top + 96;
		Text(lx + 18, sy, 140, 18, 14, m_cDim, "TONNAGE");
		m_wTonsText = Text(lx + 160, sy, panelW - 178, 18, 15, m_cText);
		m_wTonsBg = Rect(lx + 18, sy + 24, panelW - 36, 12, new Color(1, 1, 1, 0.1), 1);
		m_wTonsFill = Rect(lx + 18, sy + 24, panelW - 36, 12, m_cGood, 2);
		m_fBarW = panelW - 36;
		m_wCooling = Text(lx + 18, sy + 46, panelW - 36, 18, 14, m_cText);
		m_wFireHeat = Text(lx + 18, sy + 66, panelW - 36, 18, 14, m_cText);
		m_wSpeed = Text(lx + 18, sy + 86, panelW - 36, 18, 14, m_cText);

		float py = sy + 118;
		TextWidget partsHeader = Text(lx + 18, py, panelW - 160, 18, 14, m_cAccent, "PARTS");
		py += 26;

		// Each family has its own page; no need to scroll through every weapon.
		array<string> tabs = {"ALL", "ENERGY", "BALLISTIC", "MISSILE", "OTHER"};
		float tabW = (panelW - 24) / tabs.Count();
		for (int filter = 0; filter < tabs.Count(); filter++)
		{
			float tx = lx + 12 + filter * tabW;
			RW_LabRow tab = AddRow(ROW_FILTER, filter, tx, py, tabW - 2, 28);
			Color tabColor = m_cDim;
			if (filter == m_iPartFilter)
				tabColor = m_cAccent;
			tab.m_wTitle = Text(tx + 2, py + 4, tabW - 4, 20, 12, tabColor, tabs[filter]);
			tab.m_wTitle.SetFlags(WidgetFlags.CENTER);
		}
		py += 34;

		// Empty remains available under every filter.
		array<int> parts = {0};
		for (int id = 1; id <= 1023; id++)
		{
			BFA_MechModuleDefinition definition = m_Hardpoints.GetModuleDefinition(id);
			if (!definition)
				continue;
			RW_EquipmentEntry equipment = RW_EquipmentCatalog.Get().Find(definition.m_sWeaponModuleId);
			int family = 4;
			if (equipment)
			{
				if (equipment.m_eType == RW_EHardpointType.ENERGY)
					family = 1;
				else if (equipment.m_eType == RW_EHardpointType.BALLISTIC)
					family = 2;
				else if (equipment.m_eType == RW_EHardpointType.MISSILE)
					family = 3;
			}
			if (m_iPartFilter == 0 || m_iPartFilter == family)
				parts.Insert(id);
		}
		m_iPartCount = parts.Count() - 1;
		int fit = Math.Max((int)((bottom - 10 - py - 46) / ROW_H), 1);
		m_iPartScroll = Math.Clamp(m_iPartScroll, 0, Math.Max(m_iPartCount - fit, 0));
		m_iPartsShown = 0;
		for (int k = parts.Count() - 1; k >= 1; k--)
		{
			if (k - 1 < m_iPartScroll || k - 1 >= m_iPartScroll + fit)
				parts.Remove(k);
		}
		if (m_iPartCount > fit)
		{
			partsHeader.SetText(string.Format("PARTS  %1-%2 of %3", m_iPartScroll + 1, Math.Min(m_iPartScroll + fit, m_iPartCount), m_iPartCount));
			// Navigation is below the family tabs, in its own row.
			RW_LabRow up = AddRow(ROW_SCROLL, -1, lx + panelW - 132, py, 54, 26);
			up.m_wTitle = Text(lx + panelW - 132, py + 2, 54, 22, 14, m_cText, "UP");
			up.m_wTitle.SetFlags(WidgetFlags.CENTER);
			RW_LabRow down = AddRow(ROW_SCROLL, 1, lx + panelW - 72, py, 54, 26);
			down.m_wTitle = Text(lx + panelW - 72, py + 2, 54, 22, 14, m_cText, "DOWN");
			down.m_wTitle.SetFlags(WidgetFlags.CENTER);
			py += 32;
		}
		else
		{
			partsHeader.SetText("PARTS   click = fit | drag onto a hardpoint");
		}

		foreach (int partId : parts)
		{
			float rowH = ROW_H;
			if (partId == 0)
				rowH = 46;
			if (py + rowH > bottom - 10)
				break;
			if (partId != 0)
				m_iPartsShown++;
			RW_LabRow part = AddRow(ROW_PART, partId, lx + 12, py, panelW - 24, rowH - 6);
			part.m_wTitle = Text(lx + 26, py + 4, panelW - 130, 22, 18, m_cText);
			part.m_wSub = Text(lx + 26, py + 23, panelW - 50, 20, 13, m_cDim);
			if (partId != 0)
				part.m_wStats = Text(lx + 26, py + 40, panelW - 50, 20, 13, new Color(0.6, 0.85, 1, 1));
			part.m_wRight = Text(lx + panelW - 100, py + 6, 80, 20, 17, m_cText);
			py += rowH;
		}

		// ---------------- Right panel: hardpoints + heatsinks ----------------
		float rx = sw - EDGE - panelW;
		Rect(rx, top, panelW, bottom - top, m_cPanel, 0);
		Rect(rx, top, panelW, 3, m_cAccent, 1);
		Text(rx + 18, top + 12, panelW - 36, 20, 14, m_cAccent, "HARDPOINTS   click = select | right-click = empty");

		float hy = top + 42;
		for (int hp = 0; hp < m_Hardpoints.GetHardpointCount(); hp++)
		{
			if (hy + 64 > bottom - 140)
				break;
			RW_LabRow slot = AddRow(ROW_HARDPOINT, hp, rx + 12, hy, panelW - 24, 58);
			slot.m_wTitle = Text(rx + 26, hy + 6, 200, 20, 16, m_cAccent);
			slot.m_wSub = Text(rx + 26, hy + 30, panelW - 50, 22, 18, m_cText);
			slot.m_wRight = Text(rx + panelW - 190, hy + 8, 166, 18, 13, m_cDim);
			hy += 64;
		}

		// Heatsinks.
		float hsY = bottom - 120;
		Text(rx + 18, hsY, panelW - 36, 18, 14, m_cAccent, "HEATSINKS");
		m_wHsText = Text(rx + 18, hsY + 26, panelW - 150, 44, 16, m_cText);
		RW_LabRow minus = AddRow(ROW_HS_MINUS, 0, rx + panelW - 120, hsY + 22, 48, 44);
		minus.m_wTitle = Text(rx + panelW - 120, hsY + 28, 48, 30, 26, m_cText, "-");
		minus.m_wTitle.SetFlags(WidgetFlags.CENTER);
		RW_LabRow plus = AddRow(ROW_HS_PLUS, 0, rx + panelW - 64, hsY + 22, 48, 44);
		plus.m_wTitle = Text(rx + panelW - 64, hsY + 28, 48, 30, 26, m_cText, "+");
		plus.m_wTitle.SetFlags(WidgetFlags.CENTER);

		// ---------------- Bottom bar ----------------
		float by = sh - EDGE - 56;
		float bw = 150;
		float vw = 60;
		float groupW = vw * 2 + bw * 4 + 12 * 5;
		float bx = sw * 0.5 - groupW * 0.5;
		Rect(lx, by - 8, sw - EDGE * 2, 64, m_cPanel, 0);
		AddButton(BTN_VIEW_LEFT, "<", bx, by, vw);
		AddButton(BTN_RESET, "RESET", bx + vw + 12, by, bw);
		AddButton(BTN_APPLY, "APPLY", bx + vw + bw + 24, by, bw);
		AddButton(BTN_SAVE, "SAVE", bx + vw + bw * 2 + 36, by, bw);
		AddButton(BTN_CLOSE, "CLOSE", bx + vw + bw * 3 + 48, by, bw);
		AddButton(BTN_VIEW_RIGHT, ">", bx + vw + bw * 4 + 60, by, vw);
		m_wStatus = Text(lx + 18, by + 14, bx - lx - 36, 24, 16, m_cText);
		m_wSwapping = Text(bx + groupW + 20, by + 14, sw - EDGE - bx - groupW - 30, 24, 16, m_cAccent);

		// ---------------- Markers on the mech (one per hardpoint) ----------------
		m_fLeftEdge = lx + panelW + 10;
		m_fRightEdge = rx - 10;
		m_fBottomEdge = by - 16;
		for (int m = 0; m < m_Hardpoints.GetHardpointCount(); m++)
		{
			RW_LabRow marker = new RW_LabRow();
			marker.m_iKind = ROW_MARKER;
			marker.m_iValue = m;
			marker.m_fW = 30;
			marker.m_fH = 30;
			marker.m_bHidden = true;
			marker.m_wBg = Rect(0, 0, 22, 22, m_cAccent, 8);
			marker.m_wAccent = Rect(0, 0, 10, 10, new Color(0, 0, 0, 0.8), 9);
			marker.m_wTitle = Text(0, 0, 320, 22, 16, m_cText, "", 9);
			marker.m_wSub = Text(0, 0, 320, 20, 14, m_cDim, "", 9);
			m_aRows.Insert(marker);
		}

		m_wDragGhost = Text(0, 0, 300, 26, 18, m_cAccent, "", 20);
		m_wDragGhost.SetVisible(false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a hardpoint is on the mech, in the world. False if it can't be found.
	protected bool HardpointWorldPos(int hp, out vector pos)
	{
		IEntity module = m_Hardpoints.GetModuleEntityAt(hp);
		if (module)
		{
			pos = module.GetOrigin();
			return true;
		}

		if (!m_Hardpoints.m_aHardpoints || hp < 0 || hp >= m_Hardpoints.m_aHardpoints.Count())
			return false;
		BFA_MechHardpointDefinition def = m_Hardpoints.m_aHardpoints[hp];
		if (!def)
			return false;

		if (def.m_Point)
		{
			vector mat[4];
			def.m_Point.Init(m_pMech);
			def.m_Point.GetModelTransform(mat);
			pos = m_pMech.CoordToParent(mat[3]);
			return true;
		}

		if (!def.m_sAimMountId.IsEmpty())
		{
			BFA_MechWeaponMountComponent mounts = BFA_MechWeaponMountComponent.Cast(m_pMech.FindComponent(BFA_MechWeaponMountComponent));
			if (mounts)
			{
				IEntity mount = mounts.GetMountEntity(def.m_sAimMountId);
				if (mount)
				{
					pos = mount.GetOrigin();
					return true;
				}
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshMarker(RW_LabRow row, bool hovered, vector camPos, vector camForward)
	{
		int hp = row.m_iValue;
		vector world;
		bool show = HardpointWorldPos(hp, world) && vector.Dot(world - camPos, camForward) > 0.5;

		float x, y;
		if (show)
		{
			vector screen = GetGame().GetWorkspace().ProjWorldToScreen(world, GetGame().GetWorld());
			x = screen[0] / m_fS;
			y = screen[1] / m_fS;
			// Hidden behind a panel: don't steal clicks from it.
			show = x > m_fLeftEdge && x < m_fRightEdge && y > EDGE && y < m_fBottomEdge;
		}

		row.m_bHidden = !show;
		row.m_wBg.SetVisible(show);
		row.m_wAccent.SetVisible(show);
		row.m_wTitle.SetVisible(show);
		row.m_wSub.SetVisible(show);
		if (!show)
			return;

		bool selected = hp == m_iSelectedHp;
		bool dropTarget = m_bDragging && hovered;
		float size = 16;
		if (selected || dropTarget)
			size = 24;
		if (hovered)
			size = size + 4;

		// Selected one pulses.
		if (selected)
			size = size + Math.Sin(Now() * 6) * 3;

		row.m_fX = x - 15;
		row.m_fY = y - 15;
		PlaceS(row.m_wBg, x - size * 0.5, y - size * 0.5);
		SizeS(row.m_wBg, size, size);
		PlaceS(row.m_wAccent, x - 4, y - 4);
		SizeS(row.m_wAccent, 8, 8);

		if (selected || dropTarget)
			row.m_wBg.SetColor(m_cAccent);
		else
			row.m_wBg.SetColor(new Color(1, 1, 1, 0.75));

		int planned = m_aPending[hp];
		PlaceS(row.m_wTitle, x + 20, y - 20);
		PlaceS(row.m_wSub, x + 20, y + 1);
		row.m_wTitle.SetText(m_Hardpoints.GetHardpointName(hp));
		if (selected)
			row.m_wTitle.SetColor(m_cAccent);
		else
			row.m_wTitle.SetColor(m_cText);
		row.m_wSub.SetText(PartName(planned));
		if (planned != m_Hardpoints.GetModuleAt(hp))
			row.m_wSub.SetColor(new Color(1, 0.8, 0.3, 1));
		else
			row.m_wSub.SetColor(m_cDim);
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
		m_wFrameClass.SetText(typename.EnumToString(RW_EWeightClass, m_Frame.GetWeightClass()) + " FRAME  |  empty " + Tons(m_Frame.GetFrameTons()));
		m_wTonsText.SetText(Tons(tons) + " / " + Tons(maxTons));
		float used = Math.Clamp(tons / Math.Max(maxTons, 1), 0, 1);
		SizeS(m_wTonsFill, Math.Max(m_fBarW * used, 1), 12);
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
		// Heat if every fitted weapon fires non-stop, against cooling.
		float fireHeat = 0;
		bool heatKnown = true;
		foreach (int plannedId : m_aPending)
		{
			if (plannedId <= 0)
				continue;
			float perSecond = RW_LabStats.For(m_Hardpoints, plannedId).HeatPerSecond();
			if (perSecond < 0)
				heatKnown = false;
			else
				fireHeat = fireHeat + perSecond;
		}
		float totalCooling = baseCooling + sinkCooling;
		string fireLine = "FIRING HEAT   " + (Math.Round(fireHeat * 10) / 10).ToString() + " heat/s";
		if (!heatKnown)
			fireLine = fireLine + " +?";
		if (fireHeat > totalCooling + 0.01)
		{
			fireLine = fireLine + "   RUNS HOT";
			m_wFireHeat.SetColor(new Color(1, 0.65, 0.3, 1));
		}
		else
		{
			fireLine = fireLine + "   cooling keeps up";
			m_wFireHeat.SetColor(m_cGood);
		}
		m_wFireHeat.SetText(fireLine);

		m_wSpeed.SetText("TOP SPEED   " + Math.Round(m_Frame.SpeedScaleForTons(tons) * 100).ToString() + "%");

		// Camera for the markers.
		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);

		// Parts, hardpoints, markers.
		for (int r = 0; r < m_aRows.Count(); r++)
		{
			RW_LabRow row = m_aRows[r];
			bool hovered = r == hover;
			if (row.m_iKind == ROW_MARKER)
				RefreshMarker(row, hovered, cam[3], cam[2]);
			else if (row.m_iKind == ROW_PART)
				RefreshPart(row, hovered);
			else if (row.m_iKind == ROW_HARDPOINT)
				RefreshHardpoint(row, hovered);
			else
				RefreshButton(row, hovered, over);
		}

		// Heatsinks.
		string sinkLine = string.Format("%1 extra  (max %2)", m_iPendingHS, m_Frame.GetMaxExtraHeatsinks());
		if (sink)
			sinkLine = sinkLine + string.Format("\n+%1 heat/s | %2", Math.Round(sinkCooling * 10) / 10, Tons(sink.m_fTons * m_iPendingHS));
		m_wHsText.SetText(sinkLine);

		// Drag ghost.
		bool dragging = m_bDragging && m_iPressRow >= 0;
		m_wDragGhost.SetVisible(dragging);
		if (dragging)
		{
			PlaceS(m_wDragGhost, m_fMouseX + 16, m_fMouseY + 8);
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
				sub = typename.EnumToString(RW_EHardpointType, item.m_eType) + " | " + typename.EnumToString(RW_ESize, item.m_eSize);
			if (!fits)
				sub = sub + "  -  " + reason;
		}
		row.m_wSub.SetText(sub);
		if (row.m_wStats)
			row.m_wStats.SetText(RW_LabStats.For(m_Hardpoints, id).Line());
		if (id == 0)
			row.m_wRight.SetText("");
		else
			row.m_wRight.SetText(Tons(PartTons(id)));

		Color title = m_cText;
		if (!fits)
			title = m_cDim;
		row.m_wTitle.SetColor(title);
		row.m_wRight.SetColor(title);
		if (row.m_wStats)
		{
			if (fits)
				row.m_wStats.SetColor(new Color(0.6, 0.85, 1, 1));
			else
				row.m_wStats.SetColor(m_cDim);
		}
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
			accepts = typename.EnumToString(RW_EHardpointType, rules.m_eType) + " | up to " + typename.EnumToString(RW_ESize, rules.m_eMaxSize);
			if (rules.m_eType == RW_EHardpointType.ANY)
				accepts = "ANY | up to " + typename.EnumToString(RW_ESize, rules.m_eMaxSize);
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

	//==================================================================================================
	// ORDER FRAME (bay with an empty pad)
	//==================================================================================================

	//------------------------------------------------------------------------------------------------
	protected void OrderUpdate()
	{
		IEntity bayEntity = m_OrderBay.GetOwner();
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!bayEntity || (player && vector.Distance(player.GetOrigin(), bayEntity.GetOrigin()) > m_OrderBay.GetRadius() + 12))
		{
			Close();
			return;
		}

		// The frame is on the pad: switch to the Mech Lab for it.
		float now = Now();
		if (now >= m_fNextPadLook)
		{
			m_fNextPadLook = now + 0.5;
			IEntity mech = m_OrderBay.FindParkedMech();
			if (mech && RW_MechFrameComponent.Find(mech))
			{
				SwitchToLab(mech);
				return;
			}
		}

		if (!m_bBuildsRequested)
		{
			m_bBuildsRequested = true;
			SCR_PlayerController requester = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (requester)
				requester.RW_RequestBuilds();
		}
		if (m_bBuilt && m_iBuildsSeenSeq != s_iBuildsSeq)
			RebuildOrder();

		if (!m_bBuilt && !BuildOrder())
			return;

		WorkspaceWidget ws = GetGame().GetWorkspace();
		int mx, my;
		WidgetManager.GetMousePos(mx, my);
		m_fMouseX = ws.DPIUnscale(mx) / m_fS;
		m_fMouseY = ws.DPIUnscale(my) / m_fS;

		RefreshOrder();
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Mouse wheel: scroll the parts list when the mouse is over the left panel.
	void OnWheel(int wheel)
	{
		if (!m_pMech || m_fMouseX > m_fLeftEdge || wheel == 0)
			return;
		if (wheel > 0)
			ScrollParts(-1);
		else
			ScrollParts(1);
	}

	//------------------------------------------------------------------------------------------------
	protected void ScrollParts(int step, bool force = false)
	{
		int maxScroll = Math.Max(m_iPartCount - m_iPartsShown, 0);
		int next = Math.Clamp(m_iPartScroll + step, 0, maxScroll);
		if (next == m_iPartScroll && !force)
			return;
		m_iPartScroll = next;
		// Build the screen again with the new window (the plan is kept).
		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();
		m_wCanvas = null;
		m_aRows.Clear();
		m_bBuilt = false;
		m_iBuildTries = 0;
		m_iPressRow = -1;
		m_bDragging = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void SwitchToLab(IEntity mech)
	{
		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();
		m_wCanvas = null;
		m_aRows.Clear();
		m_bBuilt = false;
		m_iBuildTries = 0;
		m_iPressRow = -1;
		m_pMech = null;
		SetMech(mech);
		if (SHOWROOM_CAMERA && !m_Camera)
		{
			m_Camera = new RW_MechLabCamera();
			if (!m_Camera.Start(mech))
				m_Camera = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Frame facts read from the prefab (name, class, tons) for the order list.
	protected string FrameLine(RW_BayFrameEntry entry, out string title)
	{
		title = entry.m_sName;
		string line = "";
		Resource resource = Resource.Load(entry.m_sPrefab);
		if (!resource || !resource.IsValid())
			return "prefab missing";

		IEntitySource source = resource.GetResource().ToEntitySource();
		if (!source)
			return "";
		for (int c = 0; c < source.GetComponentCount(); c++)
		{
			IEntityComponentSource component = source.GetComponent(c);
			if (!component || component.GetClassName() != "RW_MechFrameComponent")
				continue;
			string frameName;
			int weightClass;
			float maxTons, frameTons;
			component.Get("m_sFrameName", frameName);
			component.Get("m_eWeightClass", weightClass);
			component.Get("m_fMaxTons", maxTons);
			component.Get("m_fFrameTons", frameTons);
			if (title.IsEmpty())
				title = frameName;
			line = typename.EnumToString(RW_EWeightClass, weightClass) + "  |  max " + maxTons.ToString() + " t  |  bare frame " + frameTons.ToString() + " t";
			break;
		}
		if (title.IsEmpty())
			title = "Frame";
		return line;
	}

	//------------------------------------------------------------------------------------------------
	protected bool BuildOrder()
	{
		float sw, sh;
		if (!PrepareCanvas(sw, sh))
			return false;
		m_bBuilt = true;

		RW_SavedBuild.ParseAll(s_sBuildsData, m_aBuilds);
		m_iBuildsSeenSeq = s_iBuildsSeq;

		float w = 760;
		int count = m_OrderBay.GetFrameCount();
		float listH = 0;
		for (int f = 0; f < count; f++)
		{
			RW_BayFrameEntry counted = m_OrderBay.GetFrameEntry(f);
			listH += 76;
			if (counted)
				listH += 66 * RW_SavedBuild.CountFor(m_aBuilds, RW_SavedBuild.GuidOf(counted.m_sPrefab));
		}
		float h = 150 + listH + 100;
		float x = sw * 0.5 - w * 0.5;
		float y = Math.Max(sh * 0.5 - h * 0.5, EDGE);

		Rect(x, y, w, h, m_cPanel, 0);
		Rect(x, y, w, 3, m_cAccent, 1);
		Text(x + 20, y + 14, w - 40, 20, 14, m_cAccent, "MECH BAY  |  " + m_OrderBay.GetBayName());
		Text(x + 20, y + 36, w - 40, 34, 28, m_cText, "ORDER FRAME");
		Text(x + 20, y + 74, w - 40, 40, 14, m_cDim, "BUILD = bare frame, then fit it in the Mech Lab.\nYour saved builds come ready-made (save them with SAVE in the Mech Lab).");

		float ry = y + 126;
		for (int i = 0; i < count; i++)
		{
			RW_BayFrameEntry entry = m_OrderBay.GetFrameEntry(i);
			if (!entry)
				continue;
			string title;
			string line = FrameLine(entry, title);
			RW_LabRow row = AddRow(ROW_ORDER, i * 100, x + 14, ry, w - 28, 68);
			row.m_wTitle = Text(x + 30, ry + 8, w - 200, 26, 22, m_cText, title);
			row.m_wSub = Text(x + 30, ry + 38, w - 60, 20, 15, m_cDim, line);
			row.m_wRight = Text(x + w - 150, ry + 22, 120, 24, 18, m_cAccent, "BUILD >");
			ry += 76;

			// This player's saved builds for the frame.
			string frameGuid = RW_SavedBuild.GuidOf(entry.m_sPrefab);
			int slots = RW_SavedBuild.CountFor(m_aBuilds, frameGuid);
			for (int slot = 1; slot <= slots; slot++)
			{
				RW_SavedBuild build = RW_SavedBuild.FindSlot(m_aBuilds, frameGuid, slot);
				// Two lines (label + full name) and a separate READY button, so nothing overlaps.
				float rowW = w - 44 - 14 - 190;
				RW_LabRow saved = AddRow(ROW_ORDER, i * 100 + slot, x + 44, ry, rowW, 58);
				saved.m_wSub = Text(x + 60, ry + 6, rowW - 30, 18, 13, m_cGood, "SAVED BUILD " + slot.ToString());
				saved.m_wTitle = Text(x + 60, ry + 26, rowW - 30, 24, 17, m_cText, build.m_sName);

				RW_LabRow ready = AddRow(ROW_ORDER, i * 100 + slot, x + 44 + rowW + 8, ry, 120, 58);
				ready.m_wTitle = Text(x + 44 + rowW + 8, ry + 17, 120, 24, 17, m_cGood, "READY >");
				ready.m_wTitle.SetFlags(WidgetFlags.CENTER);

				RW_LabRow del = AddRow(ROW_DELETE, i * 100 + slot, x + 44 + rowW + 136, ry, 48, 58);
				del.m_wTitle = Text(x + 44 + rowW + 136, ry + 16, 48, 26, 20, m_cBad, "X");
				del.m_wTitle.SetFlags(WidgetFlags.CENTER);
				ry += 66;
			}
		}

		float by = y + h - 70;
		m_wStatus = Text(x + 20, by + 14, w - 240, 44, 16, m_cText);
		AddButton(BTN_CLOSE, "CLOSE", x + w - 170, by, 150);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! New list from the server: build the screen again.
	protected void RebuildOrder()
	{
		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();
		m_wCanvas = null;
		m_aRows.Clear();
		m_bBuilt = false;
		m_iBuildTries = 0;
		m_iPressRow = -1;
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshOrder()
	{
		int hover = RowAt(m_fMouseX, m_fMouseY);
		for (int r = 0; r < m_aRows.Count(); r++)
		{
			RW_LabRow row = m_aRows[r];
			row.m_wAccent.SetVisible(r == hover && row.m_iKind == ROW_ORDER);
			if (r == hover)
				row.m_wBg.SetColor(m_cRowHover);
			else
				row.m_wBg.SetColor(m_cRow);
		}

		// Bay answer (built / refused) for 6 seconds, else local message, else the hint.
		float now = Now();
		int seq = m_OrderBay.GetBayMessageSeq();
		if (m_iOrderSeenSeq < 0)
			m_iOrderSeenSeq = seq;
		if (seq != m_iOrderSeenSeq)
		{
			m_iOrderSeenSeq = seq;
			m_fOrderMsgUntil = now + 6;
			m_fLocalUntil = 0;
		}

		string text = "Pick a frame to build";
		Color color = m_cDim;
		if (now < m_fOrderMsgUntil)
		{
			text = m_OrderBay.GetBayMessage();
			color = m_cGood;
			if (text.StartsWith("REFUSED"))
				color = m_cBad;
		}
		else if (now < m_fLocalUntil)
		{
			text = m_sLocalMessage;
			color = m_cText;
		}
		m_wStatus.SetText(text);
		m_wStatus.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	protected void OrderFrame(int index, int savedSlot)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;
		pc.RW_RequestOrderFrame(m_OrderBay.GetOwner(), index, savedSlot);
		Say("Order sent to the bay crew...", false);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteBuild(int index, int slot)
	{
		RW_BayFrameEntry entry = m_OrderBay.GetFrameEntry(index);
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!entry || !pc || slot <= 0)
			return;
		pc.RW_RequestDeleteBuild(RW_SavedBuild.GuidOf(entry.m_sPrefab), slot);
		Say("Saved build deleted", false);
	}

	//------------------------------------------------------------------------------------------------
	//! Lab: save the planned build (weapons + heatsinks) for this frame.
	protected void SaveBuild()
	{
		string reason;
		if (!m_Frame.CheckBuild(m_Hardpoints, m_aPending, m_iPendingHS, reason))
		{
			Say("Can't save: " + reason, true);
			return;
		}
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.RW_RequestSaveBuild(m_pMech, m_aPending, m_iPendingHS);
		Say("Saving build...", false);
	}
}
