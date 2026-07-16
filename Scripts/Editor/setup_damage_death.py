"""Import and configure the player damage and death presentation assets."""

from __future__ import annotations

import json
import struct
import zipfile
from pathlib import Path

import unreal


LOG_PREFIX = "[ProjectRequiem.DamageDeath]"
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

ANIMATION_ROOT = f"{PROJECT_ROOT}/Characters/Player/Animations"
DAMAGE_PATH = f"{ANIMATION_ROOT}/Damage"
DAMAGE_UAL1_PATH = f"{DAMAGE_PATH}/UAL1"
DAMAGE_UAL2_RM_PATH = f"{DAMAGE_PATH}/UAL2_RM"
DEATH_PATH = f"{ANIMATION_ROOT}/Death"
DEATH_UAL1_PATH = f"{DEATH_PATH}/UAL1"

BASE_ZIP_NAME = "Universal Base Characters[Standard].zip"
BASE_CHARACTER_GLTF = (
    "Universal Base Characters[Standard]/Base Characters/Godot - UE/"
    "Superhero_Female_FullBody.gltf"
)
UAL1_ZIP_NAME = "Universal Animation Library[Pro].zip"
UAL1_ARCHIVE_PATH = "Unreal-Godot/UAL1.glb"
UAL2_ZIP_NAME = "Universal Animation Library 2[Source].zip"
UAL2_ROOT_MOTION_ARCHIVE_PATH = "Unreal-Godot/UAL2_RM.glb"

HIT_ANIMATIONS = (
    "Hit_Head",
    "Hit_Chest",
    "Hit_Stomach",
    "Hit_Shoulder_L",
    "Hit_Shoulder_R",
)
KNOCKBACK_ANIMATION = "Hit_Knockback"
DEATH_ANIMATIONS = (
    "Death01",
    "Death02",
)

ANIMATION_PROPERTIES = {
    "Hit_Head": "hit_head_animation",
    "Hit_Chest": "hit_chest_animation",
    "Hit_Stomach": "hit_stomach_animation",
    "Hit_Shoulder_L": "hit_shoulder_left_animation",
    "Hit_Shoulder_R": "hit_shoulder_right_animation",
    KNOCKBACK_ANIMATION: "hit_knockback_animation",
    "Death01": "death01_animation",
    "Death02": "death02_animation",
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


def accessor_data_start(accessor, buffer_view, bin_start: int):
    return (
        bin_start
        + int(buffer_view.get("byteOffset", 0))
        + int(accessor.get("byteOffset", 0))
    )


def validate_accessor_bounds(
    accessor,
    buffer_view,
    element_size: int,
    stride: int,
    bin_start: int,
    bin_length: int,
    description: str,
):
    count = int(accessor["count"])
    start = accessor_data_start(accessor, buffer_view, bin_start)
    end = start if count == 0 else start + (count - 1) * stride + element_size
    require(end <= bin_start + bin_length, f"{description} exceeds the BIN chunk")
    return count, start


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
    count, start = validate_accessor_bounds(
        accessor,
        buffer_view,
        element_size,
        stride,
        bin_start,
        bin_length,
        "Pelvis translation accessor",
    )

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


def vec3_accessor_delta(
    source,
    gltf_data,
    accessor_index: int,
    bin_start: int,
    bin_length: int,
):
    accessor, buffer_view, component_count, element_size, stride = accessor_layout(
        gltf_data, accessor_index
    )
    require(component_count == 3, "Root translation accessor must be VEC3")
    count, start = validate_accessor_bounds(
        accessor,
        buffer_view,
        element_size,
        stride,
        bin_start,
        bin_length,
        "Root translation accessor",
    )
    require(count >= 2, "Root translation must contain at least two keys")
    first = struct.unpack_from("<3f", source, start)
    last = struct.unpack_from("<3f", source, start + (count - 1) * stride)
    return tuple(last[axis] - first[axis] for axis in range(3))


def read_glb_archive_entry(archive_path: Path, archive_entry: str):
    require(archive_path.is_file(), f"Missing animation archive {archive_path}")
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
    return (
        source,
        gltf_data,
        magic,
        version,
        json_type,
        json_end,
        bin_start,
        bin_length,
    )


def write_filtered_glb(
    output_path: Path,
    source,
    gltf_data,
    magic: int,
    version: int,
    json_type: int,
    json_end: int,
):
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


def prepare_in_place_source(
    output_path: Path,
    archive_path: Path,
    archive_entry: str,
    selected_animations,
    target_pelvis_translation,
):
    require(
        "_RM" not in Path(archive_entry).stem,
        f"Root-motion source forbidden for in-place animations: {archive_entry}",
    )
    (
        source,
        gltf_data,
        magic,
        version,
        json_type,
        json_end,
        bin_start,
        bin_length,
    ) = read_glb_archive_entry(archive_path, archive_entry)

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
        f"Missing animations in {archive_entry}: "
        f"{set(selected_animations) - selected_names}",
    )
    gltf_data["animations"] = selected
    return write_filtered_glb(
        output_path,
        source,
        gltf_data,
        magic,
        version,
        json_type,
        json_end,
    )


def prepare_root_motion_source(
    output_path: Path,
    archive_path: Path,
    archive_entry: str,
    animation_name: str,
    target_pelvis_translation,
):
    (
        source,
        gltf_data,
        magic,
        version,
        json_type,
        json_end,
        bin_start,
        bin_length,
    ) = read_glb_archive_entry(archive_path, archive_entry)

    node_indices = {}
    for node_name in ("root", "pelvis"):
        indices = [
            index
            for index, node in enumerate(gltf_data.get("nodes", []))
            if node.get("name") == node_name
        ]
        require(
            len(indices) == 1,
            f"Expected one {node_name} node in {archive_entry}",
        )
        node_indices[node_name] = indices[0]

    matching_animations = [
        animation
        for animation in gltf_data.get("animations", [])
        if animation.get("name") == animation_name
    ]
    require(
        len(matching_animations) == 1,
        f"Expected one {animation_name} animation in {archive_entry}",
    )
    animation = matching_animations[0]

    def find_translation_channel(node_name: str):
        channels = [
            channel
            for channel in animation.get("channels", [])
            if channel.get("target", {}).get("node") == node_indices[node_name]
            and channel.get("target", {}).get("path") == "translation"
        ]
        require(
            len(channels) == 1,
            f"{animation_name} must contain one {node_name} translation channel",
        )
        channel = channels[0]
        sampler = animation["samplers"][channel["sampler"]]
        require(
            sampler.get("interpolation", "LINEAR") in ("LINEAR", "STEP"),
            f"Unsupported {node_name} interpolation in {animation_name}",
        )
        return channel, sampler

    root_translation_channel, root_translation_sampler = find_translation_channel(
        "root"
    )
    pelvis_translation_channel, pelvis_translation_sampler = (
        find_translation_channel("pelvis")
    )

    root_delta = vec3_accessor_delta(
        source,
        gltf_data,
        int(root_translation_sampler["output"]),
        bin_start,
        bin_length,
    )
    require(
        sum(component * component for component in root_delta) > 0.01,
        f"{animation_name} root translation contains no authored displacement",
    )

    source_pelvis_translation = node_translation(gltf_data, "pelvis")
    pelvis_delta = tuple(
        target_pelvis_translation[axis] - source_pelvis_translation[axis]
        for axis in range(3)
    )
    offset_vec3_accessor(
        source,
        gltf_data,
        int(pelvis_translation_sampler["output"]),
        bin_start,
        bin_length,
        pelvis_delta,
    )

    # Preserve authored rotations, root displacement and the pelvis translation
    # adjusted to the project's temporary player skeleton.
    kept_channels = [
        channel
        for channel in animation.get("channels", [])
        if channel.get("target", {}).get("path") == "rotation"
        or channel is root_translation_channel
        or channel is pelvis_translation_channel
    ]
    require(kept_channels, f"{animation_name} has no supported animation channels")
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
    gltf_data["animations"] = [animation]
    return write_filtered_glb(
        output_path,
        source,
        gltf_data,
        magic,
        version,
        json_type,
        json_end,
    )


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
    *,
    root_motion: bool,
):
    expected = {name: f"{destination}/{name}" for name in animation_names}
    if not all(load_existing(path) for path in expected.values()):
        import_source(asset_tools, source_path, destination, skeleton)

    animations = {}
    expected_skeleton_path = package_path(skeleton.get_path_name())
    for name, path in expected.items():
        animation = require(load_existing(path), f"Missing imported animation {path}")
        require(
            isinstance(animation, unreal.AnimSequence),
            f"Expected AnimSequence at {path}",
        )
        animation_skeleton = require(
            animation.get_editor_property("skeleton"),
            f"Imported {name} has no skeleton",
        )
        require(
            package_path(animation_skeleton.get_path_name()) == expected_skeleton_path,
            f"Imported {name} uses an unexpected skeleton",
        )

        animation.modify()
        animation.set_editor_property("enable_root_motion", root_motion)
        if root_motion:
            animation.set_editor_property(
                "root_motion_root_lock",
                unreal.RootMotionRootLock.ANIM_FIRST_FRAME,
            )
            animation.set_editor_property("force_root_lock", False)
            animation.set_editor_property("use_normalized_root_motion_scale", True)

        require(
            bool(animation.get_editor_property("enable_root_motion")) == root_motion,
            f"Unexpected root-motion setting on {name}",
        )
        if root_motion:
            require(
                animation.get_editor_property("root_motion_root_lock")
                == unreal.RootMotionRootLock.ANIM_FIRST_FRAME,
                f"{name} root lock was not set to Anim First Frame",
            )
            require(
                not animation.get_editor_property("force_root_lock"),
                f"{name} unexpectedly forces root lock",
            )
            require(
                animation.get_editor_property("use_normalized_root_motion_scale"),
                f"{name} normalized root-motion scale was not enabled",
            )
        require(
            asset_subsystem.save_loaded_asset(animation),
            f"Failed to save animation {path}",
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


def configure_blueprints(asset_subsystem, animations):
    animation_blueprint = require(
        unreal.load_asset(ANIMATION_BLUEPRINT),
        f"Missing {ANIMATION_BLUEPRINT}",
    )
    character_blueprint = require(
        unreal.load_asset(CHARACTER_BLUEPRINT),
        f"Missing {CHARACTER_BLUEPRINT}",
    )

    # Compile first so newly rebuilt native properties are reflected on both CDOs.
    compile_blueprint(
        animation_blueprint,
        "ABP_PR_PlayerLocomotion before damage/death setup",
    )
    compile_blueprint(character_blueprint, "BP_CH_Player before damage/death setup")

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

    compile_blueprint(
        animation_blueprint,
        "ABP_PR_PlayerLocomotion after damage/death setup",
    )
    compile_blueprint(character_blueprint, "BP_CH_Player after damage/death setup")
    require(
        asset_subsystem.save_loaded_assets(
            [animation_blueprint, character_blueprint]
        ),
        "Failed to save damage/death-configured Blueprints",
    )


def ensure_content_directories(asset_subsystem):
    for folder in (
        DAMAGE_PATH,
        DAMAGE_UAL1_PATH,
        DAMAGE_UAL2_RM_PATH,
        DEATH_PATH,
        DEATH_UAL1_PATH,
    ):
        if not asset_subsystem.does_directory_exist(folder):
            asset_subsystem.make_directory(folder)
        require(
            asset_subsystem.does_directory_exist(folder),
            f"Failed to create content folder {folder}",
        )


def main():
    unreal.log(f"{LOG_PREFIX} Preparing player damage and death assets")

    downloads = Path.home() / "Downloads"
    base_archive = downloads / BASE_ZIP_NAME
    ual1_archive = downloads / UAL1_ZIP_NAME
    ual2_archive = downloads / UAL2_ZIP_NAME
    target_pelvis_translation = read_target_pelvis_translation(base_archive)

    source_dir = (
        Path(unreal.Paths.project_saved_dir())
        / "ImportSources"
        / "PlayerDamageDeath"
    )
    hit_source = prepare_in_place_source(
        source_dir / "UAL1_PlayerDamage.glb",
        ual1_archive,
        UAL1_ARCHIVE_PATH,
        HIT_ANIMATIONS,
        target_pelvis_translation,
    )
    death_source = prepare_in_place_source(
        source_dir / "UAL1_PlayerDeath.glb",
        ual1_archive,
        UAL1_ARCHIVE_PATH,
        DEATH_ANIMATIONS,
        target_pelvis_translation,
    )
    knockback_source = prepare_root_motion_source(
        source_dir / "UAL2_RM_Hit_Knockback.glb",
        ual2_archive,
        UAL2_ROOT_MOTION_ARCHIVE_PATH,
        KNOCKBACK_ANIMATION,
        target_pelvis_translation,
    )

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    ensure_content_directories(asset_subsystem)
    skeleton = require(load_existing(SKELETON_ASSET), f"Missing {SKELETON_ASSET}")

    animations = import_animation_group(
        asset_tools,
        asset_subsystem,
        hit_source,
        DAMAGE_UAL1_PATH,
        HIT_ANIMATIONS,
        skeleton,
        root_motion=False,
    )
    animations.update(
        import_animation_group(
            asset_tools,
            asset_subsystem,
            knockback_source,
            DAMAGE_UAL2_RM_PATH,
            (KNOCKBACK_ANIMATION,),
            skeleton,
            root_motion=True,
        )
    )
    animations.update(
        import_animation_group(
            asset_tools,
            asset_subsystem,
            death_source,
            DEATH_UAL1_PATH,
            DEATH_ANIMATIONS,
            skeleton,
            root_motion=False,
        )
    )

    configure_blueprints(asset_subsystem, animations)
    unreal.log(
        f"{LOG_PREFIX} Configured damage/death animations: "
        f"{[package_path(animation.get_path_name()) for animation in animations.values()]}"
    )


if __name__ == "__main__":
    main()
