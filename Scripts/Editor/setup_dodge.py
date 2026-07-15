"""Import and configure the root-motion Roll presentation asset."""

from __future__ import annotations

import json
import struct
import zipfile
from pathlib import Path

import unreal


LOG_PREFIX = "[ProjectRequiem.Dodge]"
PROJECT_ROOT = "/Game/ProjectRequiem"
ANIMATION_BLUEPRINT = (
    f"{PROJECT_ROOT}/Characters/Player/Animations/Locomotion/"
    "ABP_PR_PlayerLocomotion.ABP_PR_PlayerLocomotion"
)
SKELETON_ASSET = (
    f"{PROJECT_ROOT}/Characters/Player/Meshes/Temporary/"
    "SKEL_Temp_SuperheroFemale"
)
ROLL_PATH = (
    f"{PROJECT_ROOT}/Characters/Player/Animations/Locomotion/UAL1_RM"
)
ROLL_ASSET = f"{ROLL_PATH}/Roll"

BASE_ZIP_NAME = "Universal Base Characters[Standard].zip"
BASE_CHARACTER_GLTF = (
    "Universal Base Characters[Standard]/Base Characters/Godot - UE/"
    "Superhero_Female_FullBody.gltf"
)
UAL1_ZIP_NAME = "Universal Animation Library[Pro].zip"
UAL1_ROOT_MOTION_ARCHIVE_PATH = "Unreal-Godot/UAL1_RM.glb"
ROLL_ANIMATION_NAME = "Roll"


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
                f"{LOG_PREFIX} Missing {BASE_CHARACTER_GLTF} in "
                f"{base_archive.name}"
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


def accessor_data_start(
    accessor,
    buffer_view,
    bin_start: int,
):
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
    require(count >= 2, "Roll root translation must contain at least two keys")
    first = struct.unpack_from("<3f", source, start)
    last = struct.unpack_from("<3f", source, start + (count - 1) * stride)
    return tuple(last[axis] - first[axis] for axis in range(3))


def prepare_roll_source(
    output_path: Path,
    archive_path: Path,
    target_pelvis_translation,
):
    require(archive_path.is_file(), f"Missing animation archive {archive_path}")
    with zipfile.ZipFile(archive_path) as archive:
        try:
            source = bytearray(archive.read(UAL1_ROOT_MOTION_ARCHIVE_PATH))
        except KeyError as exc:
            raise RuntimeError(
                f"{LOG_PREFIX} Missing {UAL1_ROOT_MOTION_ARCHIVE_PATH} in "
                f"{archive_path.name}"
            ) from exc

    require(len(source) >= 28, "UAL1_RM animation source is too small")
    magic, version, _ = struct.unpack_from("<III", source, 0)
    require(magic == 0x46546C67 and version == 2, "UAL1_RM is not GLB 2.0")
    json_length, json_type = struct.unpack_from("<II", source, 12)
    require(json_type == 0x4E4F534A, "UAL1_RM has no leading JSON chunk")

    json_start = 20
    json_end = json_start + json_length
    gltf_data = json.loads(
        source[json_start:json_end].decode("utf-8").rstrip(" \x00")
    )
    bin_length, bin_type = struct.unpack_from("<II", source, json_end)
    require(bin_type == 0x004E4942, "UAL1_RM has no BIN chunk")
    bin_start = json_end + 8
    require(
        bin_start + bin_length <= len(source),
        "UAL1_RM contains a truncated BIN chunk",
    )

    node_indices = {}
    for node_name in ("root", "pelvis"):
        indices = [
            index
            for index, node in enumerate(gltf_data.get("nodes", []))
            if node.get("name") == node_name
        ]
        require(len(indices) == 1, f"Expected one {node_name} node in UAL1_RM")
        node_indices[node_name] = indices[0]

    matching_animations = [
        animation
        for animation in gltf_data.get("animations", [])
        if animation.get("name") == ROLL_ANIMATION_NAME
    ]
    require(
        len(matching_animations) == 1,
        f"Expected one {ROLL_ANIMATION_NAME} animation in UAL1_RM",
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
            f"Roll must contain one {node_name} translation channel",
        )
        channel = channels[0]
        sampler = animation["samplers"][channel["sampler"]]
        require(
            sampler.get("interpolation", "LINEAR") in ("LINEAR", "STEP"),
            f"Unsupported {node_name} interpolation in Roll",
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
        "UAL1_RM Roll root translation contains no authored displacement",
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

    # Preserve authored rotations, the untouched root translation that drives
    # root motion, and the proportion-adjusted pelvis translation. All other
    # translation/scale tracks are discarded like the established import pipeline.
    kept_channels = [
        channel
        for channel in animation.get("channels", [])
        if channel.get("target", {}).get("path") == "rotation"
        or channel is root_translation_channel
        or channel is pelvis_translation_channel
    ]
    require(kept_channels, "Roll has no supported animation channels")
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


def import_roll(asset_tools, source_path: Path, skeleton):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property("destination_path", ROLL_PATH)
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
    return require(load_existing(ROLL_ASSET), f"Missing imported animation {ROLL_ASSET}")


def configure_roll(asset_subsystem, animation, skeleton):
    require(
        isinstance(animation, unreal.AnimSequence),
        f"Expected AnimSequence at {ROLL_ASSET}",
    )
    animation_skeleton = require(
        animation.get_editor_property("skeleton"),
        "Imported Roll has no skeleton",
    )
    require(
        package_path(animation_skeleton.get_path_name())
        == package_path(skeleton.get_path_name()),
        "Imported Roll uses an unexpected skeleton",
    )

    root_lock = unreal.RootMotionRootLock.ANIM_FIRST_FRAME
    animation.modify()
    animation.set_editor_property("enable_root_motion", True)
    animation.set_editor_property("root_motion_root_lock", root_lock)
    animation.set_editor_property("force_root_lock", False)
    animation.set_editor_property("use_normalized_root_motion_scale", True)

    require(
        animation.get_editor_property("enable_root_motion"),
        "Roll root motion was not enabled",
    )
    require(
        animation.get_editor_property("root_motion_root_lock") == root_lock,
        "Roll root lock was not set to Anim First Frame",
    )
    require(
        not animation.get_editor_property("force_root_lock"),
        "Roll unexpectedly forces root lock",
    )
    require(
        animation.get_editor_property("use_normalized_root_motion_scale"),
        "Roll normalized root-motion scale was not enabled",
    )
    require(
        asset_subsystem.save_loaded_asset(animation),
        f"Failed to save root-motion animation {ROLL_ASSET}",
    )


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


def configure_animation_blueprint(asset_subsystem, roll_animation):
    animation_blueprint = require(
        unreal.load_asset(ANIMATION_BLUEPRINT),
        f"Missing {ANIMATION_BLUEPRINT}",
    )

    # Compile first so a newly rebuilt native roll_animation property is present.
    compile_blueprint(animation_blueprint, "ABP_PR_PlayerLocomotion before dodge setup")
    animation_class = require(
        animation_blueprint.generated_class(),
        "ABP_PR_PlayerLocomotion has no generated class",
    )
    animation_cdo = require(
        unreal.get_default_object(animation_class),
        "ABP_PR_PlayerLocomotion has no class default object",
    )
    animation_cdo.modify()
    set_required_property(
        animation_cdo,
        "roll_animation",
        roll_animation,
        "ABP_PR_PlayerLocomotion CDO",
    )

    compile_blueprint(animation_blueprint, "ABP_PR_PlayerLocomotion after dodge setup")
    require(
        asset_subsystem.save_loaded_asset(animation_blueprint),
        "Failed to save dodge-configured ABP_PR_PlayerLocomotion",
    )


def main():
    unreal.log(f"{LOG_PREFIX} Preparing root-motion Roll asset")
    downloads = Path.home() / "Downloads"
    base_archive = downloads / BASE_ZIP_NAME
    animation_archive = downloads / UAL1_ZIP_NAME
    target_pelvis_translation = read_target_pelvis_translation(base_archive)

    source_path = prepare_roll_source(
        Path(unreal.Paths.project_saved_dir())
        / "ImportSources"
        / "PlayerDodge"
        / "UAL1_RM_Roll.glb",
        animation_archive,
        target_pelvis_translation,
    )

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if not asset_subsystem.does_directory_exist(ROLL_PATH):
        asset_subsystem.make_directory(ROLL_PATH)
    require(
        asset_subsystem.does_directory_exist(ROLL_PATH),
        f"Failed to create content folder {ROLL_PATH}",
    )

    skeleton = require(load_existing(SKELETON_ASSET), f"Missing {SKELETON_ASSET}")
    roll_animation = import_roll(asset_tools, source_path, skeleton)
    configure_roll(asset_subsystem, roll_animation, skeleton)
    configure_animation_blueprint(asset_subsystem, roll_animation)
    unreal.log(
        f"{LOG_PREFIX} Configured root-motion Roll at "
        f"{package_path(roll_animation.get_path_name())}"
    )


if __name__ == "__main__":
    main()
