"""Import and configure the first unarmed-combat presentation assets."""

from __future__ import annotations

import json
import struct
import zipfile
from pathlib import Path

import unreal


LOG_PREFIX = "[ProjectRequiem.UnarmedCombat]"
PROJECT_ROOT = "/Game/ProjectRequiem"
CHARACTER_BLUEPRINT = (
    f"{PROJECT_ROOT}/Characters/Player/Blueprints/BP_CH_Player.BP_CH_Player"
)
ANIMATION_BLUEPRINT = (
    f"{PROJECT_ROOT}/Characters/Player/Animations/Locomotion/"
    "ABP_PR_PlayerLocomotion.ABP_PR_PlayerLocomotion"
)
SKELETON_ASSET = (
    f"{PROJECT_ROOT}/Characters/Player/Meshes/Temporary/"
    "SKEL_Temp_SuperheroFemale"
)
INPUT_PATH = f"{PROJECT_ROOT}/Core/Input"
ACTION_PATH = f"{INPUT_PATH}/Actions"
TOGGLE_COMBAT_ACTION = f"{ACTION_PATH}/IA_ToggleCombat"
PRIMARY_ATTACK_ACTION = f"{ACTION_PATH}/IA_PrimaryAttack"
MAPPING_CONTEXT = f"{INPUT_PATH}/IMC_Exploration"

UNARMED_PATH = (
    f"{PROJECT_ROOT}/Characters/Player/Animations/Combat/Unarmed"
)
UAL1_PATH = f"{UNARMED_PATH}/UAL1"
UAL2_PATH = f"{UNARMED_PATH}/UAL2"

BASE_ZIP_NAME = "Universal Base Characters[Standard].zip"
BASE_CHARACTER_GLTF = (
    "Universal Base Characters[Standard]/Base Characters/Godot - UE/"
    "Superhero_Female_FullBody.gltf"
)
UAL1_ZIP_NAME = "Universal Animation Library[Pro].zip"
UAL1_ARCHIVE_PATH = "Unreal-Godot/UAL1.glb"
UAL2_ZIP_NAME = "Universal Animation Library 2[Source].zip"
UAL2_ARCHIVE_PATH = "Unreal-Godot/UAL2.glb"

UAL1_ANIMATIONS = (
    "PunchKick_Enter",
    "Punch_Cross",
    "Punch_Jab",
    "PunchKick_Exit",
)
UAL2_ANIMATIONS = (
    "Melee_Knee",
    "Melee_Knee_Rec",
    "Melee_Hook",
    "Melee_Hook_Rec",
    "Melee_Uppercut",
)
COMBAT_IDLE_ANIMATION = "CombatUnarmed_Idle_Loop"

ANIMATION_PROPERTIES = {
    "PunchKick_Enter": "combat_enter_animation",
    COMBAT_IDLE_ANIMATION: "combat_idle_animation",
    "PunchKick_Exit": "combat_exit_animation",
    "Punch_Cross": "punch_cross_animation",
    "Punch_Jab": "punch_jab_animation",
    "Melee_Knee": "melee_knee_animation",
    "Melee_Knee_Rec": "melee_knee_recovery_animation",
    "Melee_Hook": "melee_hook_animation",
    "Melee_Hook_Rec": "melee_hook_recovery_animation",
    "Melee_Uppercut": "melee_uppercut_animation",
}


def require(value, message: str):
    if not value:
        raise RuntimeError(f"{LOG_PREFIX} {message}")
    return value


def package_path(asset_path: str) -> str:
    return asset_path.split(".", maxsplit=1)[0]


def load_existing(asset_path: str):
    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return None
    return unreal.load_asset(asset_path)


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


def make_mapping(action, key_name: str, outer):
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    mapping.set_editor_property("key", key)
    mapping.set_editor_property("modifiers", [])
    mapping.set_editor_property("triggers", [])
    return mapping


def mapping_matches(mapping, action, key_name: str) -> bool:
    mapped_action = mapping.get_editor_property("action")
    mapped_key = mapping.get_editor_property("key")
    if not mapped_action or not mapped_key:
        return False
    return (
        mapped_action.get_path_name() == action.get_path_name()
        and str(mapped_key.get_editor_property("key_name")) == key_name
    )


def ensure_unique_mapping(mappings, action, key_name: str, outer):
    result = []
    found = False
    for mapping in mappings:
        if not mapping_matches(mapping, action, key_name):
            result.append(mapping)
            continue
        if not found:
            result.append(mapping)
            found = True

    if not found:
        result.append(make_mapping(action, key_name, outer))
    return result


def configure_combat_input_assets(asset_tools, asset_subsystem):
    toggle_combat_action = create_data_asset(
        asset_tools, "IA_ToggleCombat", ACTION_PATH, unreal.InputAction
    )
    primary_attack_action = create_data_asset(
        asset_tools, "IA_PrimaryAttack", ACTION_PATH, unreal.InputAction
    )
    mapping_context = require(
        load_existing(MAPPING_CONTEXT),
        f"Missing existing exploration mapping context {MAPPING_CONTEXT}",
    )

    toggle_combat_action.modify()
    primary_attack_action.modify()
    mapping_context.modify()
    toggle_combat_action.set_editor_property(
        "value_type", unreal.InputActionValueType.BOOLEAN
    )
    primary_attack_action.set_editor_property(
        "value_type", unreal.InputActionValueType.BOOLEAN
    )

    mapping_data = mapping_context.get_editor_property("default_key_mappings")
    if not mapping_data:
        mapping_data = unreal.InputMappingContextMappingData()
    mappings = list(mapping_data.get_editor_property("mappings") or [])
    mappings = ensure_unique_mapping(
        mappings, toggle_combat_action, "Z", mapping_context
    )
    mappings = ensure_unique_mapping(
        mappings, primary_attack_action, "LeftMouseButton", mapping_context
    )
    mapping_data.set_editor_property("mappings", mappings)
    mapping_context.set_editor_property("default_key_mappings", mapping_data)

    require(
        asset_subsystem.save_loaded_assets(
            [toggle_combat_action, primary_attack_action, mapping_context]
        ),
        "Failed to save unarmed-combat input assets",
    )
    return toggle_combat_action, primary_attack_action


def node_translation(gltf_data, node_name: str):
    matching_nodes = [
        node for node in gltf_data.get("nodes", []) if node.get("name") == node_name
    ]
    require(len(matching_nodes) == 1, f"Expected exactly one {node_name} node")
    value = matching_nodes[0].get("translation", [0.0, 0.0, 0.0])
    require(len(value) == 3, f"Invalid {node_name} translation")
    return tuple(float(component) for component in value)


def read_target_pelvis_translation(base_archive: Path):
    require(base_archive.is_file(), f"Missing character archive {base_archive}")
    with zipfile.ZipFile(base_archive) as archive:
        try:
            source = archive.read(BASE_CHARACTER_GLTF)
        except KeyError as exc:
            raise RuntimeError(
                f"{LOG_PREFIX} Missing {BASE_CHARACTER_GLTF} in {base_archive.name}"
            ) from exc
    return node_translation(json.loads(source.decode("utf-8")), "pelvis")


def accessor_layout(gltf_data, accessor_index: int):
    accessor = gltf_data["accessors"][accessor_index]
    require(
        accessor.get("componentType") == 5126,
        f"Accessor {accessor_index} must use FLOAT components",
    )
    require(
        not accessor.get("normalized", False),
        f"Accessor {accessor_index} must not be normalized",
    )
    require("sparse" not in accessor, f"Sparse accessor {accessor_index} unsupported")
    require("bufferView" in accessor, f"Accessor {accessor_index} has no bufferView")

    component_count = {
        "SCALAR": 1,
        "VEC2": 2,
        "VEC3": 3,
        "VEC4": 4,
    }.get(accessor.get("type"))
    require(component_count, f"Unsupported accessor type {accessor.get('type')}")

    buffer_view = gltf_data["bufferViews"][accessor["bufferView"]]
    require(
        buffer_view.get("buffer", 0) == 0,
        f"Accessor {accessor_index} must use GLB buffer 0",
    )
    element_size = component_count * struct.calcsize("<f")
    stride = int(buffer_view.get("byteStride", element_size))
    require(
        stride >= element_size and stride % 4 == 0,
        f"Accessor {accessor_index} has invalid byteStride {stride}",
    )
    return accessor, buffer_view, component_count, element_size, stride


def offset_vec3_accessor(
    source,
    gltf_data,
    accessor_index: int,
    bin_start: int,
    bin_length: int,
    delta,
):
    accessor, buffer_view, component_count, element_size, stride = accessor_layout(
        gltf_data, accessor_index
    )
    require(component_count == 3, "Pelvis translation accessor must be VEC3")

    count = int(accessor["count"])
    start = (
        bin_start
        + int(buffer_view.get("byteOffset", 0))
        + int(accessor.get("byteOffset", 0))
    )
    end = start if count == 0 else start + (count - 1) * stride + element_size
    require(end <= bin_start + bin_length, "Pelvis accessor exceeds the BIN chunk")

    for index in range(count):
        offset = start + index * stride
        value = struct.unpack_from("<3f", source, offset)
        struct.pack_into(
            "<3f",
            source,
            offset,
            *(value[axis] + delta[axis] for axis in range(3)),
        )

    for key in ("min", "max"):
        if key in accessor:
            accessor[key] = [
                float(accessor[key][axis]) + delta[axis] for axis in range(3)
            ]


def freeze_animation_final_pose(
    source,
    gltf_data,
    animation,
    bin_start: int,
    bin_length: int,
):
    """Keep two identical final-pose keys so the imported idle can loop safely."""
    input_accessors = {
        int(sampler["input"]) for sampler in animation.get("samplers", [])
    }
    output_accessors = {
        int(sampler["output"]) for sampler in animation.get("samplers", [])
    }

    for sampler in animation.get("samplers", []):
        require(
            sampler.get("interpolation", "LINEAR") in ("LINEAR", "STEP"),
            "CUBICSPLINE combat-idle sampling is unsupported",
        )

    for accessor_index in input_accessors:
        accessor, buffer_view, component_count, element_size, stride = accessor_layout(
            gltf_data, accessor_index
        )
        require(component_count == 1, "Animation time accessor must be SCALAR")
        count = int(accessor["count"])
        require(count >= 2, "Combat idle source must contain at least two time keys")
        old_offset = int(accessor.get("byteOffset", 0))
        new_offset = old_offset + (count - 2) * stride
        first_key = bin_start + int(buffer_view.get("byteOffset", 0)) + new_offset
        second_key = first_key + stride
        require(
            second_key + element_size <= bin_start + bin_length,
            "Combat idle time keys exceed the BIN chunk",
        )
        struct.pack_into("<f", source, first_key, 0.0)
        struct.pack_into("<f", source, second_key, 1.0 / 30.0)
        accessor["byteOffset"] = new_offset
        accessor["count"] = 2
        accessor["min"] = [0.0]
        accessor["max"] = [1.0 / 30.0]

    for accessor_index in output_accessors:
        accessor, buffer_view, component_count, element_size, stride = accessor_layout(
            gltf_data, accessor_index
        )
        count = int(accessor["count"])
        require(count >= 2, "Combat idle pose channel must contain at least two keys")
        old_offset = int(accessor.get("byteOffset", 0))
        new_offset = old_offset + (count - 2) * stride
        first_key = bin_start + int(buffer_view.get("byteOffset", 0)) + new_offset
        second_key = first_key + stride
        require(
            second_key + element_size <= bin_start + bin_length,
            "Combat idle pose keys exceed the BIN chunk",
        )
        final_value = bytes(source[second_key : second_key + element_size])
        source[first_key : first_key + element_size] = final_value
        accessor["byteOffset"] = new_offset
        accessor["count"] = 2
        final_components = list(
            struct.unpack_from(f"<{component_count}f", source, first_key)
        )
        if "min" in accessor:
            accessor["min"] = final_components
        if "max" in accessor:
            accessor["max"] = final_components

    animation["name"] = COMBAT_IDLE_ANIMATION


def prepare_animation_source(
    output_path: Path,
    archive_path: Path,
    archive_entry: str,
    selected_animations,
    target_pelvis_translation,
    *,
    freeze_final_pose: bool = False,
):
    require(archive_path.is_file(), f"Missing animation archive {archive_path}")
    require(
        "_RM" not in Path(archive_entry).stem,
        f"Root-motion source forbidden: {archive_entry}",
    )
    with zipfile.ZipFile(archive_path) as archive:
        try:
            source = bytearray(archive.read(archive_entry))
        except KeyError as exc:
            raise RuntimeError(
                f"{LOG_PREFIX} Missing {archive_entry} in {archive_path.name}"
            ) from exc

    require(len(source) >= 28, f"Animation source {archive_entry} is too small")
    magic, version, _ = struct.unpack_from("<III", source, 0)
    require(magic == 0x46546C67 and version == 2, f"{archive_entry} is not GLB 2.0")
    json_length, json_type = struct.unpack_from("<II", source, 12)
    require(json_type == 0x4E4F534A, f"{archive_entry} has no leading JSON chunk")

    json_start = 20
    json_end = json_start + json_length
    gltf_data = json.loads(source[json_start:json_end].decode("utf-8").rstrip(" \x00"))
    bin_length, bin_type = struct.unpack_from("<II", source, json_end)
    require(bin_type == 0x004E4942, f"{archive_entry} has no BIN chunk")
    bin_start = json_end + 8
    require(
        bin_start + bin_length <= len(source),
        f"{archive_entry} contains a truncated BIN chunk",
    )

    pelvis_indices = [
        index
        for index, node in enumerate(gltf_data.get("nodes", []))
        if node.get("name") == "pelvis"
    ]
    require(len(pelvis_indices) == 1, f"Expected one pelvis node in {archive_entry}")
    pelvis_index = pelvis_indices[0]
    source_pelvis_translation = node_translation(gltf_data, "pelvis")
    pelvis_delta = tuple(
        target_pelvis_translation[axis] - source_pelvis_translation[axis]
        for axis in range(3)
    )

    selected = []
    patched_accessors = set()
    for animation in gltf_data.get("animations", []):
        if animation.get("name") not in selected_animations:
            continue

        pelvis_channels = [
            channel
            for channel in animation.get("channels", [])
            if channel.get("target", {}).get("node") == pelvis_index
            and channel.get("target", {}).get("path") == "translation"
        ]
        require(
            len(pelvis_channels) == 1,
            f"{animation.get('name')} must contain one pelvis translation channel",
        )
        pelvis_channel = pelvis_channels[0]
        pelvis_sampler = animation["samplers"][pelvis_channel["sampler"]]
        require(
            pelvis_sampler.get("interpolation", "LINEAR") in ("LINEAR", "STEP"),
            f"Unsupported pelvis interpolation in {animation.get('name')}",
        )
        output_accessor = int(pelvis_sampler["output"])
        if output_accessor not in patched_accessors:
            offset_vec3_accessor(
                source,
                gltf_data,
                output_accessor,
                bin_start,
                bin_length,
                pelvis_delta,
            )
            patched_accessors.add(output_accessor)

        kept_channels = [
            channel
            for channel in animation.get("channels", [])
            if channel.get("target", {}).get("path") == "rotation"
            or channel is pelvis_channel
        ]
        require(kept_channels, f"{animation.get('name')} has no supported channels")
        used_samplers = sorted({channel["sampler"] for channel in kept_channels})
        sampler_remap = {
            old_index: new_index
            for new_index, old_index in enumerate(used_samplers)
        }
        for channel in kept_channels:
            channel["sampler"] = sampler_remap[channel["sampler"]]

        animation["channels"] = kept_channels
        animation["samplers"] = [
            animation["samplers"][index] for index in used_samplers
        ]
        selected.append(animation)

    selected_names = {animation.get("name") for animation in selected}
    require(
        selected_names == set(selected_animations)
        and len(selected) == len(selected_animations),
        f"Missing animations in {archive_entry}: {set(selected_animations) - selected_names}",
    )
    gltf_data["animations"] = selected

    if freeze_final_pose:
        require(len(selected) == 1, "A frozen pose source must contain one animation")
        freeze_animation_final_pose(
            source, gltf_data, selected[0], bin_start, bin_length
        )

    json_bytes = json.dumps(gltf_data, separators=(",", ":")).encode("utf-8")
    json_bytes += b" " * ((4 - len(json_bytes) % 4) % 4)
    remainder = bytes(source[json_end:])
    total_length = 12 + 8 + len(json_bytes) + len(remainder)
    filtered_glb = (
        struct.pack("<III", magic, version, total_length)
        + struct.pack("<II", len(json_bytes), json_type)
        + json_bytes
        + remainder
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(filtered_glb)
    return output_path


def make_animation_pipeline(skeleton):
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.set_editor_property("use_source_name_for_asset", False)
    pipeline.set_editor_property("asset_type_sub_folders", False)

    mesh_pipeline = pipeline.get_editor_property("mesh_pipeline")
    mesh_pipeline.set_editor_property("import_static_meshes", False)
    mesh_pipeline.set_editor_property("import_skeletal_meshes", False)
    mesh_pipeline.set_editor_property("create_physics_asset", False)

    animation_pipeline = pipeline.get_editor_property("animation_pipeline")
    animation_pipeline.set_editor_property("import_animations", True)
    animation_pipeline.set_editor_property("use30_hz_to_bake_bone_animation", True)

    material_pipeline = pipeline.get_editor_property("material_pipeline")
    material_pipeline.set_editor_property("import_materials", False)
    material_pipeline.get_editor_property("texture_pipeline").set_editor_property(
        "import_textures", False
    )

    common_skeletal = pipeline.get_editor_property(
        "common_skeletal_meshes_and_animations_properties"
    )
    common_skeletal.set_editor_property("import_only_animations", True)
    common_skeletal.set_editor_property("try_auto_select_skeleton", False)
    common_skeletal.set_editor_property("skeleton", skeleton)

    stack = unreal.InterchangePipelineStackOverride()
    stack.add_pipeline(pipeline)
    return stack


def import_source(asset_tools, source_path: Path, destination: str, skeleton):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", make_animation_pipeline(skeleton))
    asset_tools.import_asset_tasks([task])
    unreal.InterchangeManager.get_interchange_manager_scripted().wait_until_all_tasks_done(
        False
    )
    imported = list(task.get_editor_property("imported_object_paths"))
    require(imported, f"Import produced no assets for {source_path.name}")
    unreal.log(f"{LOG_PREFIX} Imported {source_path.name}: {imported}")


def import_animation_group(
    asset_tools,
    asset_subsystem,
    source_path: Path,
    destination: str,
    animation_names,
    skeleton,
):
    expected = {name: f"{destination}/{name}" for name in animation_names}
    if not all(load_existing(path) for path in expected.values()):
        import_source(asset_tools, source_path, destination, skeleton)

    animations = {}
    for name, path in expected.items():
        animation = require(load_existing(path), f"Missing imported animation {path}")
        require(
            isinstance(animation, unreal.AnimSequence),
            f"Expected AnimSequence at {path}",
        )
        animation.modify()
        animation.set_editor_property("enable_root_motion", False)
        require(
            asset_subsystem.save_loaded_asset(animation),
            f"Failed to save non-root-motion animation {path}",
        )
        animations[name] = animation
    return animations


def set_required_property(instance, property_name: str, value, description: str):
    try:
        instance.set_editor_property(property_name, value)
    except Exception as exc:
        raise RuntimeError(
            f"{LOG_PREFIX} Failed to set {description}.{property_name}; "
            "rebuild the native ProjectRequiem module and reopen the Editor"
        ) from exc


def compile_blueprint(blueprint, description: str):
    require(
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint),
        f"Failed to compile {description}",
    )


def configure_blueprints(
    asset_subsystem,
    animations,
    toggle_combat_action,
    primary_attack_action,
):
    animation_blueprint = require(
        unreal.load_asset(ANIMATION_BLUEPRINT),
        f"Missing {ANIMATION_BLUEPRINT}",
    )
    character_blueprint = require(
        unreal.load_asset(CHARACTER_BLUEPRINT),
        f"Missing {CHARACTER_BLUEPRINT}",
    )

    # Compile first so newly rebuilt native properties are reflected on both CDOs.
    compile_blueprint(animation_blueprint, "ABP_PR_PlayerLocomotion before combat setup")
    compile_blueprint(character_blueprint, "BP_CH_Player before combat setup")

    animation_class = require(
        animation_blueprint.generated_class(),
        "ABP_PR_PlayerLocomotion has no generated class",
    )
    animation_cdo = require(
        unreal.get_default_object(animation_class),
        "ABP_PR_PlayerLocomotion has no class default object",
    )
    animation_cdo.modify()
    for animation_name, property_name in ANIMATION_PROPERTIES.items():
        set_required_property(
            animation_cdo,
            property_name,
            require(animations.get(animation_name), f"Missing {animation_name}"),
            "ABP_PR_PlayerLocomotion CDO",
        )
    set_required_property(
        animation_cdo,
        "root_motion_mode",
        unreal.RootMotionMode.IGNORE_ROOT_MOTION,
        "ABP_PR_PlayerLocomotion CDO",
    )

    character_class = require(
        character_blueprint.generated_class(),
        "BP_CH_Player has no generated class",
    )
    character_cdo = require(
        unreal.get_default_object(character_class),
        "BP_CH_Player has no class default object",
    )
    character_cdo.modify()
    set_required_property(
        character_cdo,
        "toggle_combat_action",
        toggle_combat_action,
        "BP_CH_Player CDO",
    )
    set_required_property(
        character_cdo,
        "primary_attack_action",
        primary_attack_action,
        "BP_CH_Player CDO",
    )

    compile_blueprint(animation_blueprint, "ABP_PR_PlayerLocomotion after combat setup")
    compile_blueprint(character_blueprint, "BP_CH_Player after combat setup")
    require(
        asset_subsystem.save_loaded_assets(
            [animation_blueprint, character_blueprint]
        ),
        "Failed to save combat-configured Blueprints",
    )


def main():
    unreal.log(f"{LOG_PREFIX} Preparing unarmed combat assets")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    toggle_combat_action, primary_attack_action = configure_combat_input_assets(
        asset_tools, asset_subsystem
    )

    downloads = Path.home() / "Downloads"
    base_archive = downloads / BASE_ZIP_NAME
    ual1_archive = downloads / UAL1_ZIP_NAME
    ual2_archive = downloads / UAL2_ZIP_NAME
    target_pelvis_translation = read_target_pelvis_translation(base_archive)

    source_dir = Path(unreal.Paths.project_saved_dir()) / "ImportSources" / "PlayerCombat"
    ual1_source = prepare_animation_source(
        source_dir / "UAL1_PlayerCombat.glb",
        ual1_archive,
        UAL1_ARCHIVE_PATH,
        UAL1_ANIMATIONS,
        target_pelvis_translation,
    )
    ual2_source = prepare_animation_source(
        source_dir / "UAL2_PlayerCombat.glb",
        ual2_archive,
        UAL2_ARCHIVE_PATH,
        UAL2_ANIMATIONS,
        target_pelvis_translation,
    )
    idle_source = prepare_animation_source(
        source_dir / f"{COMBAT_IDLE_ANIMATION}.glb",
        ual1_archive,
        UAL1_ARCHIVE_PATH,
        ("PunchKick_Enter",),
        target_pelvis_translation,
        freeze_final_pose=True,
    )

    for folder in (UNARMED_PATH, UAL1_PATH, UAL2_PATH):
        if not asset_subsystem.does_directory_exist(folder):
            asset_subsystem.make_directory(folder)
        require(
            asset_subsystem.does_directory_exist(folder),
            f"Failed to create content folder {folder}",
        )

    skeleton = require(load_existing(SKELETON_ASSET), f"Missing {SKELETON_ASSET}")
    animations = import_animation_group(
        asset_tools,
        asset_subsystem,
        ual1_source,
        UAL1_PATH,
        UAL1_ANIMATIONS,
        skeleton,
    )
    animations.update(
        import_animation_group(
            asset_tools,
            asset_subsystem,
            ual2_source,
            UAL2_PATH,
            UAL2_ANIMATIONS,
            skeleton,
        )
    )
    animations.update(
        import_animation_group(
            asset_tools,
            asset_subsystem,
            idle_source,
            UNARMED_PATH,
            (COMBAT_IDLE_ANIMATION,),
            skeleton,
        )
    )

    configure_blueprints(
        asset_subsystem,
        animations,
        toggle_combat_action,
        primary_attack_action,
    )
    unreal.log(
        f"{LOG_PREFIX} Configured non-root-motion combat animations: "
        f"{[package_path(animation.get_path_name()) for animation in animations.values()]}"
    )


if __name__ == "__main__":
    main()
