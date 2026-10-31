"""CGS Catapult: lower the RIGGED Blender source at rest and re-export FBX.

Use in Blender 4.x/4.5:
  1. Open CGS_Catapult_Rigged.blend created by the original rigging script.
  2. Scripting > Open this file > Run Script.
  3. A new 'capapult.fbx' is written next to the .blend file.
  4. Copy that FBX to Cgs-Catapult/Common/Models/capapult.fbx,
     then reimport it in Workbench and commit the resulting model resources.

IMPORTANT: This script is for the original Blender source scene, NOT a
converted Enfusion XOB or an FBX imported back into Blender with changed axes.
Refuses to run twice or when the expected pose is absent.
No external packages required; Blender's bpy only.
"""

import bpy
import math
from pathlib import Path
from mathutils import Vector, Quaternion

ARM_NAME = "CGS_Catapult_Arm"
BASE_NAME = "CGS_Catapult_Base"
RIG_NAME = "CGS_Catapult_Rig"
OLD_DEGREES = -12.0
LOWERED_DEGREES = 32.0
FPS = 30


def main():
    if not bpy.data.filepath:
        raise RuntimeError("Save your source .blend before running; the script exports beside it.")

    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    base = bpy.data.objects.get(BASE_NAME)
    arm = bpy.data.objects.get(ARM_NAME)
    rig = bpy.data.objects.get(RIG_NAME)
    if not base or base.type != 'MESH' or not arm or arm.type != 'MESH':
        raise RuntimeError("Missing base/arm meshes; open the original generated CGS Catapult .blend.")
    if not rig or rig.type != 'ARMATURE' or not rig.pose.bones.get('LaunchArm'):
        raise RuntimeError("Rig is missing. Run rig_cgs_catapult.py on the original Blender model first.")

    if arm.rotation_mode != 'XYZ':
        arm.rotation_mode = 'XYZ'

    actual_angle = math.degrees(arm.rotation_euler.y)
    if abs(actual_angle - OLD_DEGREES) > 1.0:
        raise RuntimeError(
            "Expected the ORIGINAL -12 degree rest pose, found %.1f. "
            "The scene was already modified or imported with different axes. "
            "Open the unmodified rigged .blend to avoid double-rotating." % actual_angle
        )

    # Rotate mesh OBJECT around its existing pivot; geometry stays rigid,
    # skinned to LaunchArm. The arm's origin must be the original pivot.
    arm.rotation_euler.y = math.radians(LOWERED_DEGREES)

    # Reset and rewrite the rig's firing keys relative to the newly lowered
    # mesh binding. Bone neutral pose (identity) is now the LOWERED position.
    rig.animation_data_clear()
    pb = rig.pose.bones['LaunchArm']
    pb.rotation_mode = 'QUATERNION'
    pb.location = (0.0, 0.0, 0.0)
    pb.scale = (1.0, 1.0, 1.0)

    rest_matrix = rig.data.bones['LaunchArm'].matrix_local
    local_y = (rest_matrix.to_3x3().inverted() @ Vector((0.0, 1.0, 0.0))).normalized()

    # Absolute arm angles in Blender Y rotation, NOT bone-relative values.
    # Start physically down at 32 deg (tray nose near ground), fire upward,
    # then lower again to reload.
    poses = [
        (1, LOWERED_DEGREES),
        (12, LOWERED_DEGREES),
        (26, LOWERED_DEGREES),
        (30, 12.0),
        (34, -26.0),
        (38, -57.0),
        (43, -70.0),
        (48, -70.0),
        (70, LOWERED_DEGREES),
    ]
    for frame, angle in poses:
        delta = math.radians(angle - LOWERED_DEGREES)
        pb.rotation_quaternion = Quaternion(local_y, delta)
        pb.keyframe_insert(data_path='rotation_quaternion', frame=frame, group='LaunchArm')

    if rig.animation_data and rig.animation_data.action:
        rig.animation_data.action.name = 'CGS_Catapult_Fire'

    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.frame_start = 1
    scene.frame_end = 70
    scene.frame_set(1)
    bpy.context.view_layer.update()

    # Export the same original filename expected by the existing Enfusion
    # project; do not change .xob/.anm resource paths or GUIDs.
    output_dir = Path(bpy.data.filepath).parent
    filepath = output_dir / "capapult.fbx"
    bpy.ops.object.select_all(action='DESELECT')
    for obj in (rig, base, arm):
        obj.hide_set(False)
        obj.select_set(True)
    bpy.context.view_layer.objects.active = rig

    bpy.ops.export_scene.fbx(
        filepath=str(filepath),
        use_selection=True,
        object_types={'ARMATURE', 'MESH'},
        global_scale=1.0,
        axis_forward='X',
        axis_up='Z',
        add_leaf_bones=False,
        primary_bone_axis='Y',
        secondary_bone_axis='X',
        use_armature_deform_only=False,
        bake_anim=True,
        bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False,
        bake_anim_use_all_actions=False,
        bake_anim_step=1,
        bake_anim_simplify_factor=0.0,
    )
    print("[CGS CATAPULT] Exported lowered rest-pose rigged FBX:", filepath)

    blend_copy = output_dir / "CGS_Catapult_Lowered_Rigged.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_copy), copy=True, compress=True)
    print("[CGS CATAPULT] Saved separate source copy:", blend_copy)
    print("[CGS CATAPULT] Default is lowered ~32 degrees; release animation cue frame 38.")
    print("[CGS CATAPULT] Copy capapult.fbx into Common/Models, reimport and commit in Workbench.")
    print("[CGS CATAPULT] Script does NOT change the original prefab until the new FBX is imported.")


if __name__ == '__main__':
    main()
