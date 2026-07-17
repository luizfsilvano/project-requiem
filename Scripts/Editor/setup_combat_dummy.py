"""Create the ProjectRequiem combat-dummy composition and place its test instance."""

from __future__ import annotations

import unreal


LOG_PREFIX = "[ProjectRequiem.CombatDummy]"
PROJECT_ROOT = "/Game/ProjectRequiem"
DUMMY_BLUEPRINT_PATH = f"{PROJECT_ROOT}/Combat/Blueprints/Targets"
DUMMY_BLUEPRINT_NAME = "BP_PR_CombatDummy"
DUMMY_BLUEPRINT = (
    f"{DUMMY_BLUEPRINT_PATH}/{DUMMY_BLUEPRINT_NAME}.{DUMMY_BLUEPRINT_NAME}"
)
DUMMY_NATIVE_CLASS = "/Script/ProjectRequiem.RequiemCombatDummy"
FAB_ROOT = "/Game/Fab/Medieval_Combat_Dummy"
FAB_STATIC_MESH = (
    f"{FAB_ROOT}/medieval_combat_dummy/StaticMeshes/"
    "medieval_combat_dummy.medieval_combat_dummy"
)
MAP_PATH = f"{PROJECT_ROOT}/World/Maps/Dev/L_Dev_Foundation"
MAP_OBJECT_PATH = f"{MAP_PATH}.L_Dev_Foundation"
DUMMY_ACTOR_LABEL = "CombatDummy_Test"
DUMMY_LOCATION = unreal.Vector(500.0, 500.0, 112.0)
DUMMY_ROTATION = unreal.Rotator(pitch=0.0, yaw=-135.0, roll=0.0)
DUMMY_VISUAL_LOCATION = unreal.Vector(0.0, 0.0, -98.5)
DUMMY_VISUAL_SCALE = unreal.Vector(0.5, 0.5, 0.5)


def require(value, message: str):
    if not value:
        raise RuntimeError(f"{LOG_PREFIX} {message}")
    return value


def create_or_load_blueprint(asset_tools):
    blueprint = unreal.load_asset(DUMMY_BLUEPRINT)
    if blueprint:
        return blueprint

    parent_class = require(
        unreal.load_class(None, DUMMY_NATIVE_CLASS),
        f"Missing native class {DUMMY_NATIVE_CLASS}; build the Editor target first",
    )
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return require(
        asset_tools.create_asset(
            DUMMY_BLUEPRINT_NAME,
            DUMMY_BLUEPRINT_PATH,
            unreal.Blueprint,
            factory,
        ),
        f"Failed to create {DUMMY_BLUEPRINT}",
    )


def generated_class(blueprint):
    return require(
        blueprint.generated_class(),
        f"{DUMMY_BLUEPRINT} has no generated class",
    )


def configure_blueprint(blueprint, static_mesh):
    require(
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint),
        f"Failed to compile {DUMMY_BLUEPRINT} before configuration",
    )
    dummy_cdo = require(
        unreal.get_default_object(generated_class(blueprint)),
        f"{DUMMY_BLUEPRINT} has no class default object",
    )
    visual_mesh = require(
        dummy_cdo.get_editor_property("visual_mesh"),
        f"{DUMMY_BLUEPRINT} is missing its inherited VisualMesh component",
    )

    dummy_cdo.modify()
    visual_mesh.modify()
    require(
        visual_mesh.set_static_mesh(static_mesh),
        f"Failed to assign {FAB_STATIC_MESH} to the ProjectRequiem visual component",
    )
    visual_mesh.set_editor_property("relative_location", DUMMY_VISUAL_LOCATION)
    visual_mesh.set_editor_property("relative_rotation", unreal.Rotator())
    visual_mesh.set_editor_property("relative_scale3d", DUMMY_VISUAL_SCALE)
    visual_mesh.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)

    require(
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint),
        f"Failed to compile {DUMMY_BLUEPRINT} after configuration",
    )
    return generated_class(blueprint)


def place_dummy(level_subsystem, actor_subsystem, dummy_class):
    require(level_subsystem.load_level(MAP_PATH), f"Failed to load {MAP_PATH}")
    all_actors = list(actor_subsystem.get_all_level_actors())
    matching_actors = [
        actor
        for actor in all_actors
        if actor.get_actor_label() == DUMMY_ACTOR_LABEL
    ]
    matching_class_instances = [
        actor for actor in all_actors if actor.get_class() == dummy_class
    ]
    require(
        len(matching_actors) <= 1,
        f"Expected at most one {DUMMY_ACTOR_LABEL}, found {len(matching_actors)}",
    )
    require(
        len(matching_class_instances) <= 1,
        f"Expected at most one {DUMMY_BLUEPRINT_NAME}, found {len(matching_class_instances)}",
    )

    if matching_actors:
        dummy_actor = matching_actors[0]
        require(
            dummy_actor.get_class() == dummy_class,
            f"{DUMMY_ACTOR_LABEL} exists but is not an instance of {DUMMY_BLUEPRINT}",
        )
        require(
            not matching_class_instances or matching_class_instances[0] == dummy_actor,
            f"Found separate label and class instances for {DUMMY_BLUEPRINT}",
        )
    elif matching_class_instances:
        dummy_actor = matching_class_instances[0]
    else:
        dummy_actor = require(
            actor_subsystem.spawn_actor_from_class(
                dummy_class,
                DUMMY_LOCATION,
                DUMMY_ROTATION,
            ),
            f"Failed to place {DUMMY_BLUEPRINT_NAME} in {MAP_PATH}",
        )

    dummy_actor.set_actor_label(DUMMY_ACTOR_LABEL)
    dummy_actor.set_actor_location(DUMMY_LOCATION, False, False)
    dummy_actor.set_actor_rotation(DUMMY_ROTATION, False)
    dummy_actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
    require(level_subsystem.save_current_level(), f"Failed to save {MAP_PATH}")


def main():
    unreal.log(f"{LOG_PREFIX} Configuring the simple combat target")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    require(
        asset_subsystem.does_asset_exist(FAB_STATIC_MESH),
        f"Missing inspected Fab visual {FAB_STATIC_MESH}",
    )
    require(
        asset_subsystem.does_asset_exist(MAP_OBJECT_PATH),
        f"Missing development map {MAP_OBJECT_PATH}",
    )
    if not asset_subsystem.does_directory_exist(DUMMY_BLUEPRINT_PATH):
        asset_subsystem.make_directory(DUMMY_BLUEPRINT_PATH)
    require(
        asset_subsystem.does_directory_exist(DUMMY_BLUEPRINT_PATH),
        f"Failed to create {DUMMY_BLUEPRINT_PATH}",
    )

    static_mesh = require(unreal.load_asset(FAB_STATIC_MESH), f"Failed to load {FAB_STATIC_MESH}")
    require(
        isinstance(static_mesh, unreal.StaticMesh),
        f"{FAB_STATIC_MESH} is not a StaticMesh",
    )
    blueprint = create_or_load_blueprint(asset_tools)
    dummy_class = configure_blueprint(blueprint, static_mesh)
    require(
        asset_subsystem.save_loaded_asset(blueprint),
        f"Failed to save {DUMMY_BLUEPRINT}",
    )
    place_dummy(level_subsystem, actor_subsystem, dummy_class)

    unreal.log(
        f"{LOG_PREFIX} Created {DUMMY_BLUEPRINT} with {FAB_STATIC_MESH} "
        f"and placed {DUMMY_ACTOR_LABEL} in {MAP_PATH}"
    )


if __name__ == "__main__":
    main()
