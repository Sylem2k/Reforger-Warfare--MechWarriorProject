//------------------------------------------------------------------------------------------------
//! REFORGER WARFARE - frame info on the cockpit HUD (top left): frame name, weight class, tons
//! used / max, extra heatsinks, and the last loadout message (installing / refused and why)
//! for a few seconds.
modded class BFA_MechHUDDisplay
{
	protected TextWidget m_wRwFrame;
	protected TextWidget m_wRwMessage;
	protected int m_iRwSeenSeq = -1;
	protected float m_fRwMessageUntil;
	protected float m_fRwNextTons;
	protected float m_fRwTons;

	//------------------------------------------------------------------------------------------------
	override void Update(IEntity mech, string station)
	{
		super.Update(mech, station);

		RW_MechFrameComponent frame = RW_MechFrameComponent.Find(mech);
		if (!frame || !GetGame() || !GetGame().GetWorld())
		{
			RwShow(false, false);
			return;
		}

		if (!m_wRwFrame)
			m_wRwFrame = PanelText(520, 22, 16, new Color(0.95, 0.75, 0.35, 1));
		if (!m_wRwMessage)
			m_wRwMessage = PanelText(520, 22, 15, new Color(1, 1, 1, 1));

		float now = GetGame().GetWorld().GetWorldTime() * 0.001;

		// Tons: re-added twice a second (cheap, but no need every frame).
		if (now >= m_fRwNextTons)
		{
			m_fRwNextTons = now + 0.5;
			m_fRwTons = Math.Round(frame.CurrentTons() * 10) / 10;
		}

		FrameSlot.SetPos(m_wRwFrame, MARGIN, MARGIN);
		m_wRwFrame.SetText(string.Format("%1 (%2)   %3 / %4 t   heatsinks +%5",
			frame.GetFrameName(),
			typename.EnumToString(RW_EWeightClass, frame.GetWeightClass()),
			m_fRwTons,
			frame.GetMaxTons(),
			frame.GetExtraHeatsinks()));

		// New message from the server: show it for 5 seconds.
		int seq = frame.GetCrewMessageSeq();
		if (seq != m_iRwSeenSeq)
		{
			if (m_iRwSeenSeq >= 0 || seq > 0)
				m_fRwMessageUntil = now + 5;
			m_iRwSeenSeq = seq;
		}

		bool showMessage = now < m_fRwMessageUntil && !frame.GetCrewMessage().IsEmpty();
		if (showMessage)
		{
			string message = frame.GetCrewMessage();
			FrameSlot.SetPos(m_wRwMessage, MARGIN, MARGIN + 24);
			m_wRwMessage.SetText(message);
			if (message.StartsWith("REFUSED"))
				m_wRwMessage.SetColor(new Color(1, 0.3, 0.25, 1));
			else
				m_wRwMessage.SetColor(new Color(0.4, 1, 0.5, 1));
		}

		RwShow(true, showMessage);
	}

	//------------------------------------------------------------------------------------------------
	protected void RwShow(bool frameLine, bool message)
	{
		if (m_wRwFrame)
			m_wRwFrame.SetVisible(frameLine);
		if (m_wRwMessage)
			m_wRwMessage.SetVisible(message);
	}

	//------------------------------------------------------------------------------------------------
	override void Destroy()
	{
		super.Destroy(); // removes every widget made with PanelText
		m_wRwFrame = null;
		m_wRwMessage = null;
		m_iRwSeenSeq = -1;
	}
}
