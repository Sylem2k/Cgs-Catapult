// CGS Catapult - Phase 1 launch controller, CallQueue-driven.
// Arma Reforger 1.8.0.13; designed for STATIC SCR_DestructibleEntity prefabs.
// Based on the working GetGame().GetCallqueue().CallLater timer pattern in Mech Core.
// Does not depend on EOnFrame to advance a firing sequence.
// Test-only: no arm animation, safe landing, roof clearance, or vehicle launching yet.

[ComponentEditorProps(category: "CGS Catapult", description: "Experimental server-side catapult player launch and empty fire test")]
class CGS_CatapultComponentClass : ScriptComponentClass
{
}

class CGS_CatapultComponent : ScriptComponent
{
	[Attribute("true", UIWidgets.CheckBox, "Enable the catapult's test actions.", category: "Catapult Test")]
	protected bool m_bEnabled;

	[Attribute("15", UIWidgets.Auto, "Maximum distance from catapult origin to operator (metres).", category: "Catapult Test")]
	protected float m_fUseRadius;

	[Attribute("3", UIWidgets.Auto, "Forward speed (m/s), conservative first test.", category: "Catapult Test")]
	protected float m_fForwardSpeed;

	[Attribute("6", UIWidgets.Auto, "Upward speed (m/s), conservative first test.", category: "Catapult Test")]
	protected float m_fUpSpeed;

	[Attribute("8", UIWidgets.Auto, "Minimum seconds between firing cycles.", category: "Catapult Test")]
	protected float m_fCooldownSeconds;

	[Attribute("0.5", UIWidgets.Auto, "Seconds between ARMING and release or empty-fire cue.", category: "Catapult Test")]
	protected float m_fWindupSeconds;

	[Attribute("1.2", UIWidgets.Auto, "One-time upward player lift before physics launch (metres).", category: "Catapult Test")]
	protected float m_fReleaseLift;

	[Attribute("0.08", UIWidgets.Auto, "Seconds after lift before assigning native launch velocity.", category: "Catapult Test")]
	protected float m_fReleaseDelay;

	[Attribute("1.5", UIWidgets.Auto, "Time between EMPTY FIRE CUE and EMPTY RESET, seconds.", category: "Catapult Test")]
	protected float m_fEmptyResetSeconds;

	[Attribute("false", UIWidgets.CheckBox, "Reverse the launch direction.", category: "Catapult Test")]
	protected bool m_bReverseDirection;

	[Attribute(desc: "Optional launch direction. Blue local Z points out of the open tray. If unset, use entity local X.", category: "Catapult Test")]
	protected ref PointInfo m_LaunchPoint;

	[Attribute("false", UIWidgets.CheckBox, "TEST ONLY: run one empty fire automatically after spawning.", category: "Catapult Test Diagnostics")]
	protected bool m_bAutoEmptyTestOnSpawn;

	[Attribute("3", UIWidgets.Auto, "Delay in seconds before the one-shot automatic empty fire.", category: "Catapult Test Diagnostics")]
	protected float m_fAutoEmptyTestDelaySeconds;

	// 0=ready, 1=player windup, 2=player lifted, 3=player physics probe,
	// 4=empty windup, 5=empty reset.
	protected int m_iStage;
	protected IEntity m_pTestPlayer;
	protected vector m_vLaunchVelocity;
	protected vector m_vProbeStart;
	protected float m_fNextReadyTime;
	protected bool m_bWasDeleted;

	// A static destructible prefab may not deliver EntityEvent.FRAME.
	// These timed callbacks run through the game call queue instead.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer())
			return;

		ScriptCallQueue queue = GetQueue();
		Print(string.Format("[CGS CATAPULT] INIT callQueue=%1 autoEmpty=%2 delay=%3; no EOnFrame required", queue != null, m_bAutoEmptyTestOnSpawn, m_fAutoEmptyTestDelaySeconds));
		if (!queue)
		{
			Print("[CGS CATAPULT] ERROR: game call queue unavailable; cannot schedule launch stages", LogLevel.ERROR);
			return;
		}

		if (m_bAutoEmptyTestOnSpawn)
		{
			int ms = (int)(Math.Max(m_fAutoEmptyTestDelaySeconds, 0.2) * 1000.0);
			queue.CallLater(StartAutoEmptyTest, ms, false);
			Print(string.Format("[CGS CATAPULT] AUTO EMPTY scheduled in %1ms", ms));
		}
	}

	protected ScriptCallQueue GetQueue()
	{
		if (!GetGame())
			return null;
		return GetGame().GetCallqueue();
	}

	protected void QueueAdvance(float seconds)
	{
		ScriptCallQueue queue = GetQueue();
		if (!queue)
		{
			Print("[CGS CATAPULT] ERROR: scheduling stage failed (call queue unavailable)", LogLevel.ERROR);
			ResetStage();
			return;
		}
		int ms = (int)(Math.Max(seconds, 0.01) * 1000.0);
		queue.CallLater(AdvanceStage, ms, false);
	}

	protected void ResetStage()
	{
		m_iStage = 0;
		m_pTestPlayer = null;
	}

	protected float GetNowSeconds()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;
		return GetGame().GetWorld().GetWorldTime() * 0.001;
	}

	bool CanOffer(IEntity user)
	{
		if (!m_bEnabled || !user || !GetOwner() || !ChimeraCharacter.Cast(user))
			return false;

		float radius = Math.Max(m_fUseRadius, 1.0);
		return vector.DistanceSq(user.GetOrigin(), GetOwner().GetOrigin()) <= radius * radius;
	}

	protected bool ValidateOperator(IEntity user)
	{
		if (!Replication.IsServer() || !CanOffer(user) || !GetGame())
			return false;

		PlayerManager manager = GetGame().GetPlayerManager();
		if (!manager)
			return false;

		int id = manager.GetPlayerIdFromControlledEntity(user);
		return id > 0 && manager.GetPlayerControlledEntity(id) == user;
	}

	protected vector GetLaunchDirection()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return vector.Zero;

		// Original Blender model opens along local +X. The optional PointInfo
		// follows its local blue Z instead, to handle import-axis differences.
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

	// Diagnostic trigger. Can run without a payload or a player interaction.
	protected void StartAutoEmptyTest()
	{
		if (m_bWasDeleted || !Replication.IsServer() || !GetOwner())
			return;
		if (!m_bEnabled || m_iStage != 0)
		{
			Print("[CGS CATAPULT] AUTO EMPTY skipped (disabled or currently firing)", LogLevel.WARNING);
			return;
		}

		Print("[CGS CATAPULT] AUTO EMPTY callback reached; starting cycle");
		StartEmptyCycle(true);
	}

	protected void StartEmptyCycle(bool automatic)
	{
		if (m_bWasDeleted || m_iStage != 0)
			return;

		m_iStage = 4;
		m_fNextReadyTime = GetNowSeconds() + Math.Max(m_fCooldownSeconds, 1.0);
		float windup = Math.Max(m_fWindupSeconds, 0.1);
		if (automatic)
			Print(string.Format("[CGS CATAPULT] EMPTY ARMING AUTO diagnostic windup=%1", windup));
		else
			Print(string.Format("[CGS CATAPULT] EMPTY ARMING manual windup=%1", windup));
		QueueAdvance(windup);
	}

	bool ServerFireEmpty(IEntity user)
	{
		if (!ValidateOperator(user) || m_iStage != 0)
			return false;
		if (GetNowSeconds() < m_fNextReadyTime)
			return false;

		StartEmptyCycle(false);
		return true;
	}

	bool ServerLaunchPlayer(IEntity user)
	{
		if (!ValidateOperator(user) || m_iStage != 0)
			return false;
		if (GetNowSeconds() < m_fNextReadyTime)
			return false;

		Physics body = user.GetPhysics();
		if (!body)
		{
			Print("[CGS CATAPULT] Refused: character physics body missing", LogLevel.WARNING);
			return false;
		}

		vector direction = GetLaunchDirection();
		if (direction.LengthSq() < 0.01)
		{
			Print("[CGS CATAPULT] Refused: launch direction missing", LogLevel.WARNING);
			return false;
		}

		float forwardSpeed = Math.Clamp(m_fForwardSpeed, 0, 35);
		float upwardSpeed = Math.Clamp(m_fUpSpeed, 0, 25);
		if (forwardSpeed + upwardSpeed < 0.1)
			return false;

		m_vLaunchVelocity = direction * forwardSpeed + vector.Up * upwardSpeed;
		m_pTestPlayer = user;
		m_iStage = 1;
		m_fNextReadyTime = GetNowSeconds() + Math.Max(m_fCooldownSeconds, 1.0);
		Print(string.Format("[CGS CATAPULT] ARMING player=%1 velocity=%2 windup=%3",
			GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user), m_vLaunchVelocity, m_fWindupSeconds));
		QueueAdvance(Math.Max(m_fWindupSeconds, 0.1));
		return true;
	}

	// Server-only scheduled transitions. No EntityEvent.FRAME.
	protected void AdvanceStage()
	{
		if (m_bWasDeleted || !Replication.IsServer() || !GetOwner())
			return;

		if (m_iStage == 4)
		{
			Print("[CGS CATAPULT] EMPTY FIRE CUE (visual arm animation not connected)");
			m_iStage = 5;
			QueueAdvance(Math.Max(m_fEmptyResetSeconds, 0.1));
			return;
		}
		if (m_iStage == 5)
		{
			Print("[CGS CATAPULT] EMPTY RESET complete");
			ResetStage();
			return;
		}

		if (m_iStage < 1 || m_iStage > 3)
			return;
		if (!m_pTestPlayer || !m_pTestPlayer.GetPhysics())
		{
			Print("[CGS CATAPULT] Launch cancelled: target physics unavailable", LogLevel.WARNING);
			ResetStage();
			return;
		}

		if (m_iStage == 1)
		{
			vector before = m_pTestPlayer.GetOrigin();
			vector after = before + vector.Up * Math.Clamp(m_fReleaseLift, 0.4, 2.0);
			m_pTestPlayer.SetOrigin(after);
			m_iStage = 2;
			Print(string.Format("[CGS CATAPULT] RELEASE lifted once from=%1 to=%2", before, after));
			QueueAdvance(Math.Max(m_fReleaseDelay, 0.04));
			return;
		}
		if (m_iStage == 2)
		{
			Physics body = m_pTestPlayer.GetPhysics();
			m_vProbeStart = m_pTestPlayer.GetOrigin();
			body.SetVelocity(m_vLaunchVelocity);
			m_iStage = 3;
			Print(string.Format("[CGS CATAPULT] FIRED assigned=%1 actualPhysicsVelocity=%2",
				m_vLaunchVelocity, body.GetVelocity()));
			QueueAdvance(0.4);
			return;
		}
		if (m_iStage == 3)
		{
			vector nowPosition = m_pTestPlayer.GetOrigin();
			float moved = vector.Distance(nowPosition, m_vProbeStart);
			Print(string.Format("[CGS CATAPULT] PROBE after=0.4s moved=%1m pos=%2", moved, nowPosition));
			if (moved < 0.2)
				Print("[CGS CATAPULT] WARNING: controller blocked or cancelled native velocity", LogLevel.WARNING);
			ResetStage();
		}
	}

	override void OnDelete(IEntity owner)
	{
		m_bWasDeleted = true;
		ScriptCallQueue queue = GetQueue();
		if (queue)
		{
			queue.Remove(StartAutoEmptyTest);
			queue.Remove(AdvanceStage);
		}
		ResetStage();
		super.OnDelete(owner);
	}
}
