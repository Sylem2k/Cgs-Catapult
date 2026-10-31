// CGS Catapult - Phase 1: Native player launch test (Enfusion / Arma Reforger 1.8)
// Stand on or beside the tray and use the CGS_CatapultLaunchAction.
// Uses the proven one-time character release followed by physics SetVelocity.
// This is not yet safe for occupied vehicles, crowded servers, roofs or gameplay use.

[ComponentEditorProps(category: "CGS Catapult", description: "Phase 1: server-authoritative native player launch; experimental")]
class CGS_CatapultComponentClass : ScriptComponentClass
{
}

class CGS_CatapultComponent : ScriptComponent
{
	[Attribute("true", UIWidgets.CheckBox, "Enable catapult player launch test.", category: "Catapult Test")]
	protected bool m_bEnabled;

	[Attribute("12", UIWidgets.Auto, "How close to the catapult origin a player must be to trigger it, in meters.", category: "Catapult Test")]
	protected float m_fUseRadius;

	[Attribute("3", UIWidgets.Auto, "Horizontal speed in m/s. Start at 3 (proven Portal_Test value).", category: "Catapult Test")]
	protected float m_fForwardSpeed;

	[Attribute("6", UIWidgets.Auto, "Upward speed in m/s. Start at 6 (proven Portal_Test value).", category: "Catapult Test")]
	protected float m_fUpSpeed;

	[Attribute("8", UIWidgets.Auto, "Time in seconds between launches.", category: "Catapult Test")]
	protected float m_fCooldownSeconds;

	[Attribute("0", UIWidgets.Auto, "Pre-launch windup in seconds; increase once the arm animation is wired.", category: "Catapult Test")]
	protected float m_fWindupSeconds;

	[Attribute("1.2", UIWidgets.Auto, "One-time lift to release the grounded character from the tray, in meters.", category: "Catapult Test")]
	protected float m_fReleaseLift;

	[Attribute("0.08", UIWidgets.Auto, "Delay after one-time lift before assigning physics velocity.", category: "Catapult Test")]
	protected float m_fReleaseDelay;

	[Attribute("1.5", UIWidgets.Auto, "Empty-fire reset timer in seconds; diagnostic only until animation playback is connected.", category: "Catapult Test")]
	protected float m_fEmptyResetSeconds;

	[Attribute("false", UIWidgets.CheckBox, "Flip the computed horizontal launch direction.", category: "Catapult Test")]
	protected bool m_bReverseDirection;

	[Attribute(desc: "Optional launch-direction gizmo. Place on the open tray nose; local blue Z points OUT of the catapult. Overrides fallback owner local X direction.", category: "Catapult Test")]
	protected ref PointInfo m_LaunchPoint;

	// Stages: 0 idle, 1-3 player test, 4 empty windup, 5 empty reset.
	protected int m_iStage;
	protected IEntity m_pTestPlayer;
	protected float m_fStageTime;
	protected float m_fProbeElapsed;
	protected float m_fNextReadyTime;
	protected vector m_vLaunchVelocity;
	protected vector m_vProbeStart;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (Replication.IsServer())
			SetEventMask(owner, EntityEvent.FRAME);
	}

	bool CanOffer(IEntity user)
	{
		if (!m_bEnabled || !user || !GetOwner() || !ChimeraCharacter.Cast(user))
			return false;

		float radius = Math.Max(m_fUseRadius, 1.0);
		return vector.DistanceSq(user.GetOrigin(), GetOwner().GetOrigin()) <= radius * radius;
	}

	protected vector GetLaunchDirection()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return vector.Zero;

		// Blender source catapult: +X = out of tray. When a PointInfo is configured,
		// follow its blue +Z axis instead to avoid FBX/import-axis ambiguity.
		vector direction = owner.GetTransformAxis(0);
		if (m_LaunchPoint)
		{
			m_LaunchPoint.Init(owner);
			vector local[4];
			vector world[4];
			m_LaunchPoint.GetModelTransform(local);
			owner.GetWorldTransform(world);
			vector axis = local[2];
			direction = world[0] * axis[0] + world[1] * axis[1] + world[2] * axis[2];
		}

		direction[1] = 0;
		if (direction.LengthSq() < 0.01)
			return vector.Zero;
		direction.Normalize();
		if (m_bReverseDirection)
			direction = direction * -1.0;
		return direction;
	}


	// Empty test: the operator activates the catapult with no payload on the tray.
	// This verifies action wiring and launch timing. Visual bone animation is not
	// driven until an animation graph/controller is configured.
	bool ServerFireEmpty(IEntity user)
	{
		if (!Replication.IsServer() || !CanOffer(user) || !GetGame() || m_iStage != 0)
			return false;
		BaseWorld world = GetGame().GetWorld();
		PlayerManager players = GetGame().GetPlayerManager();
		if (!world || !players)
			return false;
		int id = players.GetPlayerIdFromControlledEntity(user);
		if (id <= 0 || players.GetPlayerControlledEntity(id) != user)
			return false;
		float now = world.GetWorldTime() * 0.001;
		if (now < m_fNextReadyTime)
			return false;

		m_fStageTime = Math.Max(m_fWindupSeconds, 0.1);
		m_iStage = 4;
		m_fNextReadyTime = now + Math.Max(m_fCooldownSeconds, 1.0);
		Print(string.Format("[CGS CATAPULT] EMPTY ARMING operator=%1 windup=%2 (no target required)", id, m_fStageTime));
		return true;
	}

	bool ServerLaunchPlayer(IEntity user)
	{
		if (!Replication.IsServer() || !CanOffer(user) || !GetGame())
			return false;
		if (m_iStage != 0)
			return false;

		BaseWorld world = GetGame().GetWorld();
		PlayerManager players = GetGame().GetPlayerManager();
		if (!world || !players)
			return false;

		int id = players.GetPlayerIdFromControlledEntity(user);
		if (id <= 0 || players.GetPlayerControlledEntity(id) != user)
			return false;

		float now = world.GetWorldTime() * 0.001;
		if (now < m_fNextReadyTime)
			return false;

		Physics body = user.GetPhysics();
		if (!body)
		{
			Print("[CGS CATAPULT] Refused: player has no physics body", LogLevel.WARNING);
			return false;
		}

		vector forward = GetLaunchDirection();
		if (forward.LengthSq() < 0.01)
		{
			Print("[CGS CATAPULT] Refused: launch direction missing", LogLevel.WARNING);
			return false;
		}

		float forwardSpeed = Math.Clamp(m_fForwardSpeed, 0, 35);
		float upSpeed = Math.Clamp(m_fUpSpeed, 0, 25);
		if (forwardSpeed + upSpeed < 0.1)
			return false;

		m_vLaunchVelocity = forward * forwardSpeed + vector.Up * upSpeed;
		m_pTestPlayer = user;
		m_fStageTime = Math.Max(m_fWindupSeconds, 0.0);
		m_iStage = 1;
		m_fNextReadyTime = now + Math.Max(m_fCooldownSeconds, 1.0);
		Print(string.Format("[CGS CATAPULT] ARMING player=%1 velocity=%2 windup=%3", id, m_vLaunchVelocity, m_fStageTime));
		return true;
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!Replication.IsServer() || m_iStage == 0)
			return;

		// Empty firing cycle runs independently of any character/vehicle entity.
		float dt = Math.Max(timeSlice, 0.0);
		if (m_iStage == 4)
		{
			m_fStageTime -= dt;
			if (m_fStageTime > 0)
				return;
			Print("[CGS CATAPULT] EMPTY FIRE CUE (animation not yet connected)");
			m_iStage = 5;
			m_fStageTime = Math.Max(m_fEmptyResetSeconds, 0.1);
			return;
		}
		if (m_iStage == 5)
		{
			m_fStageTime -= dt;
			if (m_fStageTime > 0)
				return;
			Print("[CGS CATAPULT] EMPTY RESET complete; ready after cooldown");
			m_iStage = 0;
			return;
		}
		if (!m_pTestPlayer || !m_pTestPlayer.GetPhysics())
		{
			Print("[CGS CATAPULT] Launch cancelled: target/player physics unavailable", LogLevel.WARNING);
			m_pTestPlayer = null;
			m_iStage = 0;
			return;
		}

		if (m_iStage == 1)
		{
			m_fStageTime -= dt;
			if (m_fStageTime > 0)
				return;

			// One release lift, NOT a per-frame scripted trajectory.
			// Needed even when standing on a raised catapult tray: the character
			// controller may be grounded against the tray rather than terrain.
			vector before = m_pTestPlayer.GetOrigin();
			vector after = before + vector.Up * Math.Clamp(m_fReleaseLift, 0.4, 2.0);
			m_pTestPlayer.SetOrigin(after);
			m_iStage = 2;
			m_fStageTime = Math.Max(m_fReleaseDelay, 0.04);
			Print(string.Format("[CGS CATAPULT] RELEASE lifted once from=%1 to=%2", before, after));
			return;
		}

		if (m_iStage == 2)
		{
			m_fStageTime -= dt;
			if (m_fStageTime > 0)
				return;

			Physics body = m_pTestPlayer.GetPhysics();
			m_vProbeStart = m_pTestPlayer.GetOrigin();
			m_fProbeElapsed = 0;
			body.SetVelocity(m_vLaunchVelocity);
			m_iStage = 3;
			Print(string.Format("[CGS CATAPULT] FIRED assigned=%1 actualPhysicsVelocity=%2", m_vLaunchVelocity, body.GetVelocity()));
			return;
		}

		if (m_iStage == 3)
		{
			m_fProbeElapsed += dt;
			if (m_fProbeElapsed < 0.40)
				return;

			vector nowPos = m_pTestPlayer.GetOrigin();
			float moved = vector.Distance(nowPos, m_vProbeStart);
			Print(string.Format("[CGS CATAPULT] PROBE after=%1s moved=%2m pos=%3", m_fProbeElapsed, moved, nowPos));
			if (moved < 0.2)
				Print("[CGS CATAPULT] Player appears blocked or controller cancelled velocity", LogLevel.WARNING);
			m_pTestPlayer = null;
			m_iStage = 0;
		}
	}

	override void OnDelete(IEntity owner)
	{
		m_pTestPlayer = null;
		m_iStage = 0;
		super.OnDelete(owner);
	}
}
