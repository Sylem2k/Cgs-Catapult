# CGS Catapult - Phase 1 handoff (2026-10-30)

I pushed the Phase 1 native-player-launch test scripts to this repository:
- `Scripts/Game/CGS_Catapult/CGS_CatapultComponent.c`
- `Scripts/Game/CGS_Catapult/CGS_CatapultLaunchAction.c`

The model is imported as `Common/Models/capapult.fbx` / `capapult.xob`.
The current prefab is `Prefabs/Props/Core/Destructible_Cata_Base.et`, which has a static RigidBody.

## My Workbench setup

1. Pull `main` in GitHub Desktop.
2. Open `Destructible_Cata_Base.et`.
3. Add `CGS_CatapultComponent` to the root entity.
4. Add `ActionsManagerComponent`, with `CGS_CatapultLaunchAction` in its actions list.
5. Configure the optional `LaunchPoint` gizmo at the open nose of the throwing tray, orienting local blue +Z outward. Otherwise the component uses the entity local +X axis.
6. Spawn on a clear test map, approach the tray, and activate `CATAPULT: LAUNCH PLAYER (TEST)`.
7. Look for `[CGS CATAPULT] ARMING`, `RELEASE`, `FIRED`, and `PROBE` in the log.

Initial speeds are 3 m/s forward, 6 m/s upward, single 1.2 m grounded-character release lift, 0.08 second delay, 8 second cooldown. The component applies one native physics velocity change and probes displacement 0.4 seconds later.

**Status:** committed source scripts; not yet compiled in Enfusion Workbench or tested in multiplayer. This only launches the player activating the action, not vehicles or others on the tray. It does not yet synchronize the arm animation or provide safe landing. Test only in a disposable scenario.

## Next
- Fix any Workbench compile errors.
- Confirm action visibility and flight direction.
- Confirm server/client flight replication and camera behavior.
- Add vehicle-specific detection/physics test.
- Connect the launch event to the firing animation.
