// CGS Catapult - Phase 1 action for testing the native player launch.
// Add this action through an ActionsManagerComponent on the catapult prefab.
class CGS_CatapultLaunchAction : ScriptedUserAction
{
	protected CGS_CatapultComponent GetCatapult()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;

		CGS_CatapultComponent catapult = CGS_CatapultComponent.Cast(owner.FindComponent(CGS_CatapultComponent));
		if (catapult)
			return catapult;

		IEntity root = owner.GetRootParent();
		if (root && root != owner)
			return CGS_CatapultComponent.Cast(root.FindComponent(CGS_CatapultComponent));
		return null;
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "CATAPULT: LAUNCH PLAYER (TEST)";
		return true;
	}

	// Keep the action visible when the context is in range, so a missing
	// controller shows a disabled action rather than hiding all diagnostics.
	override bool CanBeShownScript(IEntity user)
	{
		return ChimeraCharacter.Cast(user) != null;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		CGS_CatapultComponent catapult = GetCatapult();
		if (!catapult)
		{
			SetCannotPerformReason("Catapult component missing");
			return false;
		}
		if (!catapult.CanOffer(user))
		{
			SetCannotPerformReason("Catapult disabled or too far away");
			return false;
		}
		return true;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		Print("[CGS CATAPULT] LAUNCH PLAYER action invoked on server");
		CGS_CatapultComponent catapult = GetCatapult();
		if (!catapult)
		{
			Print("[CGS CATAPULT] LAUNCH PLAYER refused: component missing", LogLevel.WARNING);
			return;
		}
		if (!catapult.ServerLaunchPlayer(pUserEntity))
			Print("[CGS CATAPULT] LAUNCH PLAYER refused: range, body, busy state or cooldown", LogLevel.WARNING);
	}
}
