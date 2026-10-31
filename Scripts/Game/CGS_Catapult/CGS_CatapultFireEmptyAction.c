// Test the catapult's fire sequence without a character or vehicle payload.
// This is a diagnostic action: actual LaunchArm visual animation is not yet bound.
class CGS_CatapultFireEmptyAction : ScriptedUserAction
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
		outName = "CATAPULT: FIRE EMPTY (TEST)";
		return true;
	}

	// Show this diagnostic button whenever the player's interaction context is
	// in range, even if the controller component cannot be found. In that case
	// the button will be disabled with a reason instead of silently disappearing.
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
		Print("[CGS CATAPULT] FIRE EMPTY action invoked on server");
		CGS_CatapultComponent catapult = GetCatapult();
		if (!catapult)
		{
			Print("[CGS CATAPULT] FIRE EMPTY refused: controller not found", LogLevel.WARNING);
			return;
		}
		if (!catapult.ServerFireEmpty(pUserEntity))
			Print("[CGS CATAPULT] FIRE EMPTY refused: range, player, busy state or cooldown", LogLevel.WARNING);
	}
}
