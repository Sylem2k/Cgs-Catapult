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

	override bool CanBeShownScript(IEntity user)
	{
		CGS_CatapultComponent catapult = GetCatapult();
		return catapult && catapult.CanOffer(user);
	}

	override bool CanBePerformedScript(IEntity user)
	{
		if (CanBeShownScript(user))
			return true;
		SetCannotPerformReason("Move closer to the catapult");
		return false;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		CGS_CatapultComponent catapult = GetCatapult();
		if (catapult)
			catapult.ServerFireEmpty(pUserEntity);
	}
}
