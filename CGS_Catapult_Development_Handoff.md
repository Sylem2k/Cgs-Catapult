# CGS Catapult — development handoff (2026-10-30)

## What I just changed

I found that my prefab already listed `CGS_CatapultLaunchAction`, but the required `CGS_CatapultComponent` was absent. As a result, CanBeShownScript always failed and the launch button was invisible.

I wired `CGS_CatapultComponent` into `Prefabs/Props/Core/Destructible_Cata_Base.et`, expanded the test radius to 12 m, and added `CGS_CatapultFireEmptyAction` alongside the original self-launch action. The empty action works with **nothing on the tray**: it logs ARMING -> FIRE CUE -> RESET and checks server-side action and cooldown execution. It does **not** animate the physical arm yet.

I also added `Tools/Blender/CGS_Catapult_Lowered_Rest_Export.py`. The catapult's FBX was originally exported with the tray raised (-12 degrees). This Blender script takes my original `CGS_Catapult_Rigged.blend` scene, lowers the default arm pose to +32 degrees, rewrites the firing action from lowered -> up -> lowered, and exports `capapult.fbx` again under its existing name. This requires me to run Blender and reimport the updated FBX through Workbench before the in-game catapult will actually appear lowered.

## Current files

- `Scripts/Game/CGS_Catapult/CGS_CatapultComponent.c`: Phase 1 player native-velocity launch + payload-free firing state.
- `Scripts/Game/CGS_Catapult/CGS_CatapultLaunchAction.c`: launches the player who interacts, not yet cars or other payloads.
- `Scripts/Game/CGS_Catapult/CGS_CatapultFireEmptyAction.c`: empty cycle, available without a target.
- `Prefabs/Props/Core/Destructible_Cata_Base.et`: component and both actions wired.
- `Tools/Blender/CGS_Catapult_Lowered_Rest_Export.py`: lowered pose re-export.
- `Common/Models/capapult.fbx`: original imported FBX; **not yet replaced with lowered export**.

## My next test

1. I pull `main` through GitHub Desktop.
2. I open Workbench and compile Game scripts. If errors show up, I capture their exact file and line number.
3. I spawn `Destructible_Cata_Base.et` and walk up to the existing action context on the catapult.
4. I activate **CATAPULT: FIRE EMPTY (TEST)**. I expect server logs for EMPTY ARMING, EMPTY FIRE CUE, EMPTY RESET; I do not expect the model to move.
5. I activate **CATAPULT: LAUNCH PLAYER (TEST)** on clear ground and check the native-physics probe output.
6. To fix the actual *raised visual rest pose*, I open the ORIGINAL generated `CGS_Catapult_Rigged.blend` in Blender, run the new Blender tool, copy the exported `capapult.fbx` into `Common/Models/`, reimport it, then commit/push the modified binary resource in GitHub Desktop.
7. After that, I configure a Workbench animation controller for the `CGS_Catapult_Fire` .anm and wire the firing action to it. This is **not implemented** yet; the static prefab's RigidBody collision also needs redesign before arm motion can be reliable.

## Safety / limits

This is uncompiled first-pass Enfusion code; don't treat a successful commit as a compile or multiplayer test. The empty fire has no physical payload and currently provides timing/log tests only. The player-launch test uses a one-time position lift then server physics SetVelocity and is unsafe near roofs, on obstacles, or for public gameplay. Vehicle physics, automatic tray target detection, occupied vehicle handling, and animation playback are future phases.

## Process

I keep the original `capapult.fbx` and Enfusion script filenames, push code updates to this repository, and maintain these notes in first person.

## Mech Throw action visibility port (2026-10-31)

I compared my actual in-use MechWarrior project prefabs `Prefabs/Vehicles/Mech/Frames/Light/Light_Mech_Frame_B.et` and `Prefabs/Vehicles/Mech/Frames/Medium/Med_Mech_Frame_B.et`, along with the corresponding `CGS_MechTacticalThrowAction.c` in `Mech-Core-CGS`.

**What I found:** Mech Throw uses a `UserActionContext` whose `ContextName` is `default`, and each `additionalActions` entry specifies `ParentContextList { "default" }` and `VisibilityRange 8`. My catapult had a context with no `ContextName`, action entries without `ParentContextList`, and an interaction point shifted 5.2 m to one end. That is why the scripts could be present but the interaction buttons were not properly registered for display.

**What I changed in `Destructible_Cata_Base.et`:**
- Restored `ContextName "default"` and moved the interaction point to `Offset 0 1.05 0`.
- Set context `Radius 50`, `Omnidirectional 1`, `VisibilityAngle 180`, and `FilterActionsUsingCache 0`.
- Set both action entries' `ParentContextList` to `default` and `VisibilityRange 15`.
- Explicitly enabled `CGS_CatapultComponent` and set its use radius to 15 m.
- Preserved original entity ID, imported mesh reference, static rigid body, component IDs, and original action class/file names.

I retained my catapult's own `CGS_CatapultComponent`, `CGS_CatapultFireEmptyAction`, and `CGS_CatapultLaunchAction` rather than copying `CGS_MechBrainComponent`. That mech brain has faction/companion-claim/order dependencies that a stationary catapult does not need. I also added a one-time component startup log: `[CGS CATAPULT] Component initialized on server; default-context launch controls ready for testing`.

**My next manual validation:** Pull `main` and compile Game scripts. Spawn or reload `Destructible_Cata_Base.et` (not an old saved world instance), approach within 15 m, then use the standard Reforger interaction menu (usually hold F). I should see `CATAPULT: FIRE EMPTY (TEST)` and `CATAPULT: LAUNCH PLAYER (TEST)`. The first requires no payload and should log EMPTY ARMING, EMPTY FIRE CUE, EMPTY RESET. The second launches the **operator** only and is unsafe near obstacles. If the action remains missing, check the startup log first; an absent startup log suggests the prefab or Game script module did not load. These changes are not yet Workbench-compiled or runtime-verified.

**Separate animation status:** The imported `capapult.fbx` still has its originally raised baked rest pose. The lowered rest export script is in `Tools/Blender/` but I still need to run it in my Blender source project and reimport the FBX through Workbench. The FIRE EMPTY action is a logic/timer probe, not a visual arm animation or a car launcher.


## Standalone diagnostics after startup-only Workbench log (2026-10-31)

My Workbench session showed the catapult's initialization log, then only ordinary game startup messages. There was no FIRE or launch message. That does **not** establish that physics failed; it means no action execution was captured.

To isolate the problem without needing a car, a passenger, or even a visible F-menu button, I added an **automatic one-time empty firing cycle** to the existing test prefab:

- `CGS_CatapultComponent.c` has editor properties `m_bAutoEmptyTestOnSpawn` and `m_fAutoEmptyTestDelaySeconds`, implemented using the component's existing server `EOnFrame` event.
- Only `Destructible_Cata_Base.et` enables `m_bAutoEmptyTestOnSpawn 1` and sets a **3-second** delay.
- After the catapult exists in a running world for three seconds, I should see `[CGS CATAPULT] EMPTY ARMING AUTO diagnostic`, then `EMPTY FIRE CUE`, then `EMPTY RESET`. There is no projectile or character launch and no physical arm animation at this stage.
- In both `CGS_CatapultFireEmptyAction.c` and `CGS_CatapultLaunchAction.c`, I changed `CanBeShownScript` to depend only on the actor being a character. The normal action context's 15 m `VisibilityRange` still limits visibility. `CanBePerformedScript` provides disabled reasons if the catapult component is absent or the player is too far away. Clicking either action also writes a clear server log, including refusal reasons.
- This opt-in test is temporary. I will set the prefab flag back to 0 after button/physics/animation tests so production catapults do not auto-fire at spawn.

**My next test:** Pull `main`, compile Game scripts, and reload a fresh world instance. Without touching the controls, I watch the log for the three AUTO EMPTY phase messages. If they appear but the F-menu is missing, the failure is action visibility/selection rather than frame timing. If only the startup message appears, I must investigate whether `EOnFrame` is called on this static destructible entity. The FBX is still originally posed raised and the catapult arm visual animation is still not connected; the automatic test does not falsely claim either is working. I must send the Workbench compiler errors exactly if compilation fails.



## Firing timer fix — static prefab / no FRAME callbacks (2026-10-31)

My latest Workbench log showed `[CGS CATAPULT] Component initialized on server; emptyAutoTest=1 after=3 seconds` but never displayed `EMPTY ARMING AUTO diagnostic` or subsequent firing stages. I had also seen a manual `ARMING player=1` without the later `RELEASE` or `FIRED` lines. This strongly points to my static `SCR_DestructibleEntity` not getting the `EOnFrame` updates that my controller depended on, although the log alone doesn't establish the exact engine reason.

I inspected `CGS_MechEngineComponent.c` in `Sylem2k/Mech-Core-CGS`. That component already uses `GetGame().GetCallqueue().CallLater(FinishStartup, ms, false, ...)` for timed server events. The shield component also uses `ScriptCallQueue.Remove` for cleanup.

**I changed the entire existing `Scripts/Game/CGS_Catapult/CGS_CatapultComponent.c`** to use call-queue callbacks instead of `EOnFrame`. On server initialization it now schedules `StartAutoEmptyTest` after the configured three seconds. That starts a one-off empty cycle; `AdvanceStage` handles empty FIRE CUE and RESET with one-shot scheduled calls. The manual player action uses the same `AdvanceStage` to progress ARMING -> RELEASE -> FIRED -> PROBE. `OnDelete` removes pending callbacks so a destroyed catapult won't continue to fire.

Existing class names, action method signatures, editor configuration fields, and script filename are preserved. The test prefab still has `m_bAutoEmptyTestOnSpawn 1` and `m_fAutoEmptyTestDelaySeconds 3` **for diagnosis only**; I will disable that before normal use.

**My next test:** Pull `main`, reload Workbench and compile the Game module. I should get an INIT line including `callQueue=1` and a scheduled delay. Roughly three seconds into a running game, without pressing a button, I should get:
- `[CGS CATAPULT] AUTO EMPTY callback reached; starting cycle`
- `[CGS CATAPULT] EMPTY ARMING AUTO diagnostic`
- `[CGS CATAPULT] EMPTY FIRE CUE`
- `[CGS CATAPULT] EMPTY RESET complete`

If I only get the INIT line or a callQueue unavailable warning, I must inspect the new startup log and Workbench state. If the empty cycle completes, I then try the player launch after cooldown and inspect RELEASE/FIRED/PROBE. A successful callback chain still doesn't prove native character physics moves or the arm model animates.

**Status:** GitHub source commit completed; not compiled in Workbench by me. Actual lowered model and visual animation are separate, unfinished tasks.
