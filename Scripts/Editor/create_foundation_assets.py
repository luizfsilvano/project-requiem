"""Create the non-gameplay Project Requiem foundation assets in Unreal Editor 5.8."""

from __future__ import annotations

import unreal


LOG_PREFIX = "[ProjectRequiem.Foundation]"
GAME_ROOT = "/Game/ProjectRequiem"
INPUT_PATH = f"{GAME_ROOT}/Core/Input"
ACTION_PATH = f"{INPUT_PATH}/Actions"
LEGACY_SPRINT_ACTION = f"{ACTION_PATH}/IA_Sprint"
FRAMEWORK_PATH = f"{GAME_ROOT}/Core/Blueprints/GameFramework"
CHARACTER_PATH = f"{GAME_ROOT}/Characters/Player/Blueprints"
MAP_PATH = f"{GAME_ROOT}/World/Maps/Dev/L_Dev_Foundation"

CONTENT_FOLDERS = (
    f"{GAME_ROOT}/Core/Blueprints/GameFramework",
    f"{GAME_ROOT}/Core/Blueprints/Components",
    f"{GAME_ROOT}/Core/Blueprints/Interfaces",
    f"{GAME_ROOT}/Core/Input/Actions",
    f"{GAME_ROOT}/Core/Data",
    f"{GAME_ROOT}/Characters/Player/Blueprints",
    f"{GAME_ROOT}/Characters/Player/Animations",
    f"{GAME_ROOT}/Characters/Player/Meshes",
    f"{GAME_ROOT}/Characters/Player/Materials",
    f"{GAME_ROOT}/Characters/Shared",
    f"{GAME_ROOT}/NPCs/Shared",
    f"{GAME_ROOT}/NPCs/Family",
    f"{GAME_ROOT}/NPCs/Guild",
    f"{GAME_ROOT}/World/Maps/Dev",
    f"{GAME_ROOT}/World/Maps/Intro",
    f"{GAME_ROOT}/World/Maps/Tests",
    f"{GAME_ROOT}/World/Environment/Architecture",
    f"{GAME_ROOT}/World/Environment/Props",
    f"{GAME_ROOT}/World/LevelInstances",
    f"{GAME_ROOT}/UI/Common",
    f"{GAME_ROOT}/UI/Screens",
    f"{GAME_ROOT}/UI/HUD",
    f"{GAME_ROOT}/UI/Styles",
    f"{GAME_ROOT}/Dialogue/Blueprints",
    f"{GAME_ROOT}/Dialogue/Data",
    f"{GAME_ROOT}/PlayerData/Blueprints",
    f"{GAME_ROOT}/PlayerData/Data",
    f"{GAME_ROOT}/Interaction/Blueprints",
    f"{GAME_ROOT}/Interaction/Components",
    f"{GAME_ROOT}/Interaction/Interfaces",
    f"{GAME_ROOT}/Interaction/Data",
    f"{GAME_ROOT}/Guild/Blueprints",
    f"{GAME_ROOT}/Guild/Data",
    f"{GAME_ROOT}/Combat/Blueprints",
    f"{GAME_ROOT}/Combat/Components",
    f"{GAME_ROOT}/Combat/Data",
    f"{GAME_ROOT}/Combat/Styles",
    f"{GAME_ROOT}/Classes/Blueprints",
    f"{GAME_ROOT}/Classes/Data",
    f"{GAME_ROOT}/Classes/UI",
    f"{GAME_ROOT}/Audio/Music",
    f"{GAME_ROOT}/Audio/SFX",
    f"{GAME_ROOT}/Audio/Voice",
    f"{GAME_ROOT}/Cinematics/Intro",
    f"{GAME_ROOT}/Cinematics/Shared",
    f"{GAME_ROOT}/Art/Materials",
    f"{GAME_ROOT}/Art/VFX",
    f"{GAME_ROOT}/Art/Textures",
    f"{GAME_ROOT}/Tests/Functional",
    f"{GAME_ROOT}/Tests/Fixtures",
    f"{GAME_ROOT}/Prototypes/Maps",
    f"{GAME_ROOT}/Prototypes/Blueprints",
    f"{GAME_ROOT}/Prototypes/Assets",
)


def require(value, message: str):
    if not value:
        raise RuntimeError(f"{LOG_PREFIX} {message}")
    return value


def create_data_asset(asset_tools, asset_name: str, package_path: str, asset_class):
    object_path = f"{package_path}/{asset_name}.{asset_name}"
    existing = unreal.load_asset(object_path)
    if existing:
        return existing

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    return require(
        asset_tools.create_asset(asset_name, package_path, asset_class, factory),
        f"Failed to create {object_path}",
    )


def create_blueprint(asset_tools, asset_name: str, package_path: str, parent_class_path: str):
    object_path = f"{package_path}/{asset_name}.{asset_name}"
    blueprint = unreal.load_asset(object_path)
    if not blueprint:
        parent_class = require(
            unreal.load_class(None, parent_class_path),
            f"Missing native parent class {parent_class_path}",
        )
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = require(
            asset_tools.create_asset(asset_name, package_path, unreal.Blueprint, factory),
            f"Failed to create {object_path}",
        )

    require(
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint),
        f"Failed to compile {object_path}",
    )
    return blueprint


def generated_class(blueprint):
    return require(blueprint.generated_class(), f"Blueprint {blueprint.get_name()} has no generated class")


def default_object(blueprint):
    return require(
        unreal.get_default_object(generated_class(blueprint)),
        f"Blueprint {blueprint.get_name()} has no class default object",
    )


def make_mapping(action, key_name: str, outer, modifier_classes=(), triggers=()):
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    mapping.set_editor_property("key", key)
    modifiers = [unreal.new_object(modifier_class, outer=outer) for modifier_class in modifier_classes]
    mapping.set_editor_property("modifiers", modifiers)
    mapping.set_editor_property("triggers", list(triggers))
    return mapping


def make_mouse_look_mapping(action, outer):
    mapping = make_mapping(action, "Mouse2D", outer)
    invert_vertical = unreal.new_object(unreal.InputModifierNegate, outer=outer)
    invert_vertical.set_editor_property("x", False)
    invert_vertical.set_editor_property("y", True)
    invert_vertical.set_editor_property("z", False)
    mapping.set_editor_property("modifiers", [invert_vertical])
    return mapping


def create_input_assets(asset_tools):
    move_action = create_data_asset(asset_tools, "IA_Move", ACTION_PATH, unreal.InputAction)
    look_action = create_data_asset(asset_tools, "IA_Look", ACTION_PATH, unreal.InputAction)
    jump_action = create_data_asset(asset_tools, "IA_Jump", ACTION_PATH, unreal.InputAction)
    crouch_action = create_data_asset(asset_tools, "IA_Crouch", ACTION_PATH, unreal.InputAction)
    roll_action = create_data_asset(asset_tools, "IA_Roll", ACTION_PATH, unreal.InputAction)
    mapping_context = create_data_asset(
        asset_tools, "IMC_Exploration", INPUT_PATH, unreal.InputMappingContext
    )

    move_action.set_editor_property("value_type", unreal.InputActionValueType.AXIS2D)
    look_action.set_editor_property("value_type", unreal.InputActionValueType.AXIS2D)
    jump_action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    crouch_action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    roll_action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)

    mappings = [
        make_mapping(move_action, "W", mapping_context, (unreal.InputModifierSwizzleAxis,)),
        make_mapping(
            move_action,
            "S",
            mapping_context,
            (unreal.InputModifierNegate, unreal.InputModifierSwizzleAxis),
        ),
        make_mapping(move_action, "A", mapping_context, (unreal.InputModifierNegate,)),
        make_mapping(move_action, "D", mapping_context),
        make_mapping(move_action, "Gamepad_Left2D", mapping_context),
        make_mouse_look_mapping(look_action, mapping_context),
        make_mapping(look_action, "Gamepad_Right2D", mapping_context),
        make_mapping(jump_action, "SpaceBar", mapping_context),
        make_mapping(jump_action, "Gamepad_FaceButton_Bottom", mapping_context),
        make_mapping(crouch_action, "LeftControl", mapping_context),
        make_mapping(roll_action, "LeftShift", mapping_context),
    ]
    mapping_data = unreal.InputMappingContextMappingData()
    mapping_data.set_editor_property("mappings", mappings)
    mapping_context.set_editor_property("default_key_mappings", mapping_data)

    return move_action, look_action, jump_action, crouch_action, roll_action, mapping_context


def remove_legacy_input_assets():
    if unreal.EditorAssetLibrary.does_asset_exist(LEGACY_SPRINT_ACTION):
        referencers = unreal.EditorAssetLibrary.find_package_referencers_for_asset(
            LEGACY_SPRINT_ACTION, load_assets_to_confirm=True
        )
        require(
            not referencers,
            f"Cannot remove {LEGACY_SPRINT_ACTION}; still referenced by {referencers}",
        )
        require(
            unreal.EditorAssetLibrary.delete_asset(LEGACY_SPRINT_ACTION),
            f"Failed to remove obsolete {LEGACY_SPRINT_ACTION}",
        )


def create_framework_blueprints(
    asset_tools,
    move_action,
    look_action,
    jump_action,
    crouch_action,
    roll_action,
    mapping_context,
):
    controller_blueprint = create_blueprint(
        asset_tools,
        "BP_PC_Requiem",
        FRAMEWORK_PATH,
        "/Script/ProjectRequiem.RequiemPlayerController",
    )
    character_blueprint = create_blueprint(
        asset_tools,
        "BP_CH_Player",
        CHARACTER_PATH,
        "/Script/ProjectRequiem.RequiemCharacter",
    )
    game_mode_blueprint = create_blueprint(
        asset_tools,
        "BP_GM_Foundation",
        FRAMEWORK_PATH,
        "/Script/ProjectRequiem.RequiemGameModeBase",
    )

    controller_cdo = default_object(controller_blueprint)
    controller_cdo.modify()
    controller_cdo.set_editor_property("default_mapping_context", mapping_context)

    character_cdo = default_object(character_blueprint)
    character_cdo.modify()
    character_cdo.set_editor_property("move_action", move_action)
    character_cdo.set_editor_property("look_action", look_action)
    character_cdo.set_editor_property("jump_action", jump_action)
    character_cdo.set_editor_property("crouch_action", crouch_action)
    character_cdo.set_editor_property("roll_action", roll_action)

    game_mode_cdo = default_object(game_mode_blueprint)
    game_mode_cdo.modify()
    game_mode_cdo.set_editor_property("default_pawn_class", generated_class(character_blueprint))
    game_mode_cdo.set_editor_property(
        "player_controller_class", generated_class(controller_blueprint)
    )

    for blueprint in (controller_blueprint, character_blueprint, game_mode_blueprint):
        require(
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint),
            f"Failed to compile {blueprint.get_name()} after setting defaults",
        )

    return controller_blueprint, character_blueprint, game_mode_blueprint


def configure_static_mesh_actor(actor, label: str, mesh, scale):
    actor.set_actor_label(label)
    actor.set_actor_scale3d(scale)
    mesh_component = require(
        actor.get_editor_property("static_mesh_component"),
        f"{label} is missing its StaticMeshComponent",
    )
    require(mesh_component.set_static_mesh(mesh), f"Failed to assign the cube mesh to {label}")


def create_development_map(asset_subsystem, level_subsystem, actor_subsystem):
    map_object_path = f"{MAP_PATH}.L_Dev_Foundation"
    if asset_subsystem.does_asset_exist(map_object_path):
        require(level_subsystem.load_level(MAP_PATH), f"Failed to load {MAP_PATH}")
        return

    require(level_subsystem.new_level(MAP_PATH, False), f"Failed to create {MAP_PATH}")
    cube_mesh = require(
        unreal.load_asset("/Engine/BasicShapes/Cube.Cube"),
        "Missing /Engine/BasicShapes/Cube",
    )

    floor = require(
        actor_subsystem.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, -50.0), unreal.Rotator()
        ),
        "Failed to spawn foundation floor",
    )
    configure_static_mesh_actor(floor, "Foundation_Floor", cube_mesh, unreal.Vector(20.0, 20.0, 1.0))

    step = require(
        actor_subsystem.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(450.0, 0.0, 50.0), unreal.Rotator()
        ),
        "Failed to spawn foundation step",
    )
    configure_static_mesh_actor(step, "Foundation_Step", cube_mesh, unreal.Vector(2.0, 4.0, 1.0))

    ramp = require(
        actor_subsystem.spawn_actor_from_class(
            unreal.StaticMeshActor,
            unreal.Vector(-500.0, 0.0, 30.0),
            unreal.Rotator(pitch=12.0, yaw=0.0, roll=0.0),
        ),
        "Failed to spawn foundation ramp",
    )
    configure_static_mesh_actor(ramp, "Foundation_Ramp", cube_mesh, unreal.Vector(4.0, 3.0, 0.25))

    player_start = require(
        actor_subsystem.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(0.0, 0.0, 120.0), unreal.Rotator()
        ),
        "Failed to spawn PlayerStart",
    )
    player_start.set_actor_label("Foundation_PlayerStart")

    directional_light = require(
        actor_subsystem.spawn_actor_from_class(
            unreal.DirectionalLight,
            unreal.Vector(0.0, 0.0, 500.0),
            unreal.Rotator(pitch=-45.0, yaw=-35.0, roll=0.0),
        ),
        "Failed to spawn DirectionalLight",
    )
    directional_light.set_actor_label("Foundation_DirectionalLight")

    sky_light = require(
        actor_subsystem.spawn_actor_from_class(
            unreal.SkyLight, unreal.Vector(), unreal.Rotator()
        ),
        "Failed to spawn SkyLight",
    )
    sky_light.set_actor_label("Foundation_SkyLight")

    sky_atmosphere = require(
        actor_subsystem.spawn_actor_from_class(
            unreal.SkyAtmosphere, unreal.Vector(), unreal.Rotator()
        ),
        "Failed to spawn SkyAtmosphere",
    )
    sky_atmosphere.set_actor_label("Foundation_SkyAtmosphere")

    require(level_subsystem.save_current_level(), f"Failed to save {MAP_PATH}")


def main():
    unreal.log(f"{LOG_PREFIX} Creating foundation assets")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    for folder in CONTENT_FOLDERS:
        if not asset_subsystem.does_directory_exist(folder):
            asset_subsystem.make_directory(folder)
        require(
            asset_subsystem.does_directory_exist(folder),
            f"Failed to create content folder {folder}",
        )

    (
        move_action,
        look_action,
        jump_action,
        crouch_action,
        roll_action,
        mapping_context,
    ) = create_input_assets(asset_tools)
    blueprints = create_framework_blueprints(
        asset_tools,
        move_action,
        look_action,
        jump_action,
        crouch_action,
        roll_action,
        mapping_context,
    )
    assets_to_save = [
        move_action,
        look_action,
        jump_action,
        crouch_action,
        roll_action,
        mapping_context,
        *blueprints,
    ]
    require(asset_subsystem.save_loaded_assets(assets_to_save), "Failed to save foundation assets")

    remove_legacy_input_assets()

    create_development_map(asset_subsystem, level_subsystem, actor_subsystem)
    unreal.log(f"{LOG_PREFIX} Foundation assets created successfully")


if __name__ == "__main__":
    main()
