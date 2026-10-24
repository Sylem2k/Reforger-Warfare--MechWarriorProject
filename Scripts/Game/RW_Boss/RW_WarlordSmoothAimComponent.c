// Warlord-only smooth aim. Never changes the shared MechCore aim component.
// Eight replicated station aims are slewed on the server; the existing MechCore
// replication continues to drive visible weapon mounts on every client.
// Manual/player aim remains untouched if someone takes a real station.
[ComponentEditorProps(category: "Reforger Warfare/Boss", description: "Rate-limited AI gunner aim for the Warlord world boss.")]
class RW_WarlordSmoothAimComponentClass : BFA_MechAimComponentClass
{
}

class RW_WarlordSmoothAimComponent : BFA_MechAimComponent
{
    [Attribute("38", UIWidgets.Auto, "Maximum AI traverse speed, degrees per second.", category: "Warlord AI Aim")]
    protected float m_fAIYawSpeed;

    [Attribute("27", UIWidgets.Auto, "Maximum AI elevation speed, degrees per second.", category: "Warlord AI Aim")]
    protected float m_fAIPitchSpeed;

    protected ref array<int> m_aAimTimes = {};

    protected float WrapAngle(float angle)
    {
        while (angle > 180)
            angle -= 360;
        while (angle < -180)
            angle += 360;
        return angle;
    }

    override void ServerSetStationAim(string stationId, vector mechLocalPoint)
    {
        if (!Replication.IsServer())
            return;

        BFA_MechCoreComponent core = BFA_MechCoreComponent.Cast(GetOwner().FindComponent(BFA_MechCoreComponent));
        if (!core)
        {
            super.ServerSetStationAim(stationId, mechLocalPoint);
            return;
        }

        // Never restrict the live player's camera or weapon aim.
        IEntity occupant = core.GetStationOccupant(stationId);
        if (occupant && !BFA_MechMovementComponent.IsAIControlled(occupant))
        {
            super.ServerSetStationAim(stationId, mechLocalPoint);
            return;
        }

        int index = core.GetStationIndex(stationId);
        if (index < 0 || index >= 8 || mechLocalPoint.LengthSq() < 0.001)
        {
            super.ServerSetStationAim(stationId, mechLocalPoint);
            return;
        }

        while (m_aAimTimes.Count() <= index)
            m_aAimTimes.Insert(0);

        int nowMs = GetGame().GetWorld().GetWorldTime();
        int lastMs = m_aAimTimes[index];
        float dt = 0.1;
        if (lastMs > 0)
            dt = Math.Clamp((nowMs - lastMs) * 0.001, 0.01, 0.25);
        m_aAimTimes[index] = nowMs;

        vector current = GetAimLocalPointForStation(stationId);
        if (current.LengthSq() < 0.01)
            current = vector.Forward * 1000;

        float currentYaw = Math.Atan2(current[0], current[2]) * Math.RAD2DEG;
        float targetYaw = Math.Atan2(mechLocalPoint[0], mechLocalPoint[2]) * Math.RAD2DEG;

        float currentFlat = Math.Sqrt(current[0] * current[0] + current[2] * current[2]);
        float targetFlat = Math.Sqrt(mechLocalPoint[0] * mechLocalPoint[0] + mechLocalPoint[2] * mechLocalPoint[2]);
        float currentPitch = Math.Atan2(current[1], Math.Max(currentFlat, 0.001)) * Math.RAD2DEG;
        float targetPitch = Math.Atan2(mechLocalPoint[1], Math.Max(targetFlat, 0.001)) * Math.RAD2DEG;

        float yawDelta = WrapAngle(targetYaw - currentYaw);
        float pitchDelta = targetPitch - currentPitch;
        float newYaw = currentYaw + Math.Clamp(yawDelta, -m_fAIYawSpeed * dt, m_fAIYawSpeed * dt);
        float newPitch = currentPitch + Math.Clamp(pitchDelta, -m_fAIPitchSpeed * dt, m_fAIPitchSpeed * dt);

        float yawRad = newYaw * Math.DEG2RAD;
        float pitchRad = newPitch * Math.DEG2RAD;
        float cp = Math.Cos(pitchRad);

        vector direction = Vector(
            Math.Sin(yawRad) * cp,
            Math.Sin(pitchRad),
            Math.Cos(yawRad) * cp
        );

        float targetDistance = Math.Max(mechLocalPoint.Length(), 10.0);
        super.ServerSetStationAim(stationId, direction * targetDistance);
    }
}
