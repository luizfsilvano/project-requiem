"""Configure the player sword hand/back presentation without touching combat rules.

Run through UnrealEditor-Cmd after rebuilding the native ProjectRequiemEditor target.
The existing hand socket is treated as user-authored input and is only inspected.
"""

import math

import unreal


LOG_PREFIX = "[ProjectRequiem.SwordAttachment]"
PLAYER_MESH = (
    "/Game/ProjectRequiem/Characters/Player/Meshes/Temporary/"
    "SKM_Temp_SuperheroFemale"
)
PLAYER_BLUEPRINT = (
    "/Game/ProjectRequiem/Characters/Player/Blueprints/BP_CH_Player"
)

HAND_SOCKET_NAME = "Socket_Weapon_Hand_R"
HAND_BONE_NAME = "hand_r"
BACK_SOCKET_NAME = "Socket_Weapon_Back"
BACK_BONE_NAME = "spine_03"

# Component-space target: grip over the right upper back, sword angled down
# toward the left hip and with its thin axis facing away from the body.
BACK_LOCATION = (-16.0, -14.844128, 19.228205)
BACK_ROTATION = (39.070796, 9.353976, -165.353354)  # pitch, yaw, roll


def require(value, message):
    if not value:
        raise RuntimeError(f"{LOG_PREFIX} {message}")
    return value


def vector_tuple(value):
    return (float(value.x), float(value.y), float(value.z))


def rotator_tuple(value):
    return (float(value.pitch), float(value.yaw), float(value.roll))


def nearly_equal_tuple(left, right, tolerance=1.0e-6):
    return len(left) == len(right) and all(
        math.isclose(a, b, rel_tol=0.0, abs_tol=tolerance)
        for a, b in zip(left, right)
    )


def snapshot_socket(socket):
    return {
        "outer": socket.get_outer().get_path_name(),
        "socket_name": str(socket.get_editor_property("socket_name")),
        "bone_name": str(socket.get_editor_property("bone_name")),
        "location": vector_tuple(socket.get_editor_property("relative_location")),
        "rotation": rotator_tuple(socket.get_editor_property("relative_rotation")),
        "scale": vector_tuple(socket.get_editor_property("relative_scale")),
        "force_always_animated": bool(
            socket.get_editor_property("force_always_animated")
        ),
    }


def require_snapshot_unchanged(before, after, description):
    for key in ("outer", "socket_name", "bone_name", "force_always_animated"):
        require(
            before[key] == after[key],
            f"{description} changed {key}: {before[key]} -> {after[key]}",
        )
    for key in ("location", "rotation", "scale"):
        require(
            nearly_equal_tuple(before[key], after[key]),
            f"{description} changed {key}: {before[key]} -> {after[key]}",
        )


def find_socket(mesh, socket_name):
    return mesh.find_socket(unreal.Name(socket_name))


def configure_back_socket(mesh, asset_subsystem):
    hand_socket = require(
        find_socket(mesh, HAND_SOCKET_NAME),
        f"Missing user-authored socket {HAND_SOCKET_NAME}",
    )
    hand_before = snapshot_socket(hand_socket)
    require(
        hand_before["bone_name"] == HAND_BONE_NAME,
        f"{HAND_SOCKET_NAME} must remain on {HAND_BONE_NAME}",
    )

    back_socket = find_socket(mesh, BACK_SOCKET_NAME)
    if back_socket is None:
        mesh.modify()
        back_socket = unreal.SkeletalMeshSocket(outer=mesh)
        back_socket.set_socket_parent(mesh, unreal.Name(BACK_BONE_NAME))
        back_socket.set_editor_property(
            "relative_location",
            unreal.Vector(*BACK_LOCATION),
        )
        back_socket.set_editor_property(
            "relative_rotation",
            unreal.Rotator(
                pitch=BACK_ROTATION[0],
                yaw=BACK_ROTATION[1],
                roll=BACK_ROTATION[2],
            ),
        )
        back_socket.set_editor_property(
            "relative_scale",
            unreal.Vector(1.0, 1.0, 1.0),
        )
        mesh.add_socket(back_socket, False)
        temporary_name = back_socket.get_editor_property("socket_name")
        skeletal_mesh_subsystem = require(
            unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem),
            "SkeletalMeshEditorSubsystem is unavailable",
        )
        require(
            skeletal_mesh_subsystem.rename_socket(
                mesh,
                temporary_name,
                unreal.Name(BACK_SOCKET_NAME),
            ),
            f"Failed to rename temporary socket {temporary_name} to {BACK_SOCKET_NAME}",
        )
        require(
            asset_subsystem.save_loaded_asset(mesh),
            f"Failed to save {PLAYER_MESH}",
        )
        back_socket = require(
            find_socket(mesh, BACK_SOCKET_NAME),
            f"{BACK_SOCKET_NAME} disappeared after save",
        )
        unreal.log(
            f"{LOG_PREFIX} Created mesh-only {BACK_SOCKET_NAME} on {BACK_BONE_NAME}"
        )
    else:
        unreal.log(
            f"{LOG_PREFIX} Preserving existing {BACK_SOCKET_NAME} transform for manual tuning"
        )

    back_snapshot = snapshot_socket(back_socket)
    require(
        back_snapshot["outer"] == mesh.get_path_name(),
        f"{BACK_SOCKET_NAME} must be mesh-only on {PLAYER_MESH}",
    )
    require(
        back_snapshot["bone_name"] == BACK_BONE_NAME,
        f"{BACK_SOCKET_NAME} must remain on {BACK_BONE_NAME}",
    )

    hand_after = snapshot_socket(
        require(find_socket(mesh, HAND_SOCKET_NAME), f"Lost {HAND_SOCKET_NAME}")
    )
    require_snapshot_unchanged(
        hand_before,
        hand_after,
        f"User-authored {HAND_SOCKET_NAME}",
    )
    return hand_after, back_snapshot


def compile_blueprint_no_warnings(blueprint):
    require(
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint),
        "Failed to compile BP_CH_Player; rebuild ProjectRequiemEditor first",
    )
    require(
        blueprint.get_editor_property("status")
        == unreal.BlueprintStatus.BS_UP_TO_DATE,
        f"BP_CH_Player compiled with status {blueprint.get_editor_property('status')}",
    )


def get_blueprint_cdo(blueprint):
    generated_class = require(
        blueprint.generated_class(),
        "BP_CH_Player has no generated class",
    )
    return require(
        unreal.get_default_object(generated_class),
        "BP_CH_Player has no class default object",
    )


def configure_character_blueprint(blueprint, asset_subsystem):
    compile_blueprint_no_warnings(blueprint)
    character_cdo = get_blueprint_cdo(blueprint)
    character_cdo.modify()
    character_cdo.set_editor_property(
        "sword_hand_socket_name",
        unreal.Name(HAND_SOCKET_NAME),
    )
    character_cdo.set_editor_property(
        "sword_back_socket_name",
        unreal.Name(BACK_SOCKET_NAME),
    )
    character_cdo.set_sword_equipped_presentation(False)

    sword_component = require(
        character_cdo.get_editor_property("sword_mesh"),
        "BP_CH_Player CDO.sword_mesh is unavailable",
    )
    sword_component.modify()
    sword_component.set_editor_property("visible", True)
    sword_component.set_editor_property("hidden_in_game", False)

    compile_blueprint_no_warnings(blueprint)
    require(
        asset_subsystem.save_loaded_asset(blueprint),
        "Failed to save BP_CH_Player",
    )

    character_cdo = get_blueprint_cdo(blueprint)
    require(
        str(character_cdo.get_editor_property("sword_hand_socket_name"))
        == HAND_SOCKET_NAME,
        "BP_CH_Player did not retain the hand socket name",
    )
    require(
        str(character_cdo.get_editor_property("sword_back_socket_name"))
        == BACK_SOCKET_NAME,
        "BP_CH_Player did not retain the back socket name",
    )
    sword_component = require(
        character_cdo.get_editor_property("sword_mesh"),
        "BP_CH_Player sword component disappeared after compilation",
    )
    require(
        str(sword_component.get_attach_socket_name()) == BACK_SOCKET_NAME,
        f"Stored sword is attached to {sword_component.get_attach_socket_name()} instead of {BACK_SOCKET_NAME}",
    )
    require(
        sword_component.is_visible()
        and not sword_component.get_editor_property("hidden_in_game"),
        "Stored sword must remain visible on the back",
    )


def main():
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    mesh = require(unreal.load_asset(PLAYER_MESH), f"Missing {PLAYER_MESH}")
    blueprint = require(
        unreal.load_asset(PLAYER_BLUEPRINT),
        f"Missing {PLAYER_BLUEPRINT}",
    )

    hand_snapshot, back_snapshot = configure_back_socket(mesh, asset_subsystem)
    configure_character_blueprint(blueprint, asset_subsystem)
    unreal.log(
        f"{LOG_PREFIX} Configured hand={hand_snapshot} back={back_snapshot}"
    )


if __name__ == "__main__":
    main()
