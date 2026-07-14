"""Import and configure the minimal temporary character used by L_Dev_Foundation."""

from __future__ import annotations

import json
import struct
import zipfile
from pathlib import Path

import unreal


LOG_PREFIX = "[ProjectRequiem.VisualFoundation]"
PROJECT_ROOT = "/Game/ProjectRequiem"
CHARACTER_BLUEPRINT = (
    f"{PROJECT_ROOT}/Characters/Player/Blueprints/BP_CH_Player.BP_CH_Player"
)
VISUAL_CHARACTER_CLASS = "/Script/ProjectRequiem.RequiemVisualValidationCharacter"
MESH_PATH = f"{PROJECT_ROOT}/Characters/Player/Meshes/Temporary"
MATERIAL_PATH = f"{PROJECT_ROOT}/Characters/Player/Materials/Temporary"
ANIMATION_PATH = f"{PROJECT_ROOT}/Characters/Player/Animations/Temporary"
MAP_PATH = f"{PROJECT_ROOT}/World/Maps/Dev/L_Dev_Foundation"

MESH_ASSET = f"{MESH_PATH}/SKM_Temp_SuperheroFemale"
SKELETON_ASSET = f"{MESH_PATH}/SKEL_Temp_SuperheroFemale"
IDLE_ASSET = f"{ANIMATION_PATH}/A_Temp_Idle"
MOVE_ASSET = f"{ANIMATION_PATH}/A_Temp_Walk"
JUMP_ASSET = f"{ANIMATION_PATH}/A_Temp_JumpLoop"
LEGACY_MOVE_ASSET = f"{ANIMATION_PATH}/A_Temp_Jog"
CHARACTER_MATERIALS = (
    (
        (f"{MESH_PATH}/M_Temp_Hair_2", f"{MESH_PATH}/MI_Hair_2"),
        f"{MATERIAL_PATH}/M_Temp_Hair_2",
    ),
    (
        (f"{MESH_PATH}/M_Temp_Eyes", f"{MESH_PATH}/MI_Eyes"),
        f"{MATERIAL_PATH}/M_Temp_Eyes",
    ),
    (
        (
            f"{MESH_PATH}/M_Temp_SuperheroFemale",
            f"{MESH_PATH}/MI_Superhero_Female",
        ),
        f"{MATERIAL_PATH}/M_Temp_SuperheroFemale",
    ),
)
CHARACTER_TEXTURES = (
    "T_Eye_Brown",
    "T_Eye_Normal_png",
    "T_Hair_2_BaseColor",
    "T_Hair_2_Normal",
    "T_Superhero_Female_Dark_BaseColor",
    "T_Superhero_Female_Normal",
    "T_Superhero_Female_Roughness",
)

BASE_ZIP_NAME = "Universal Base Characters[Standard].zip"
ANIMATION_ZIP_NAME = "Universal Animation Library[Standard].zip"
BASE_ARCHIVE_ROOT = "Universal Base Characters[Standard]/Base Characters/Godot - UE"
ANIMATION_ARCHIVE_PATH = (
    "Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb"
)

CHARACTER_SOURCE_FILES = (
    "Superhero_Female_FullBody.bin",
    "T_Hair_2_Normal.png",
    "T_Hair_2_BaseColor.png",
    "T_Eye_Brown.png",
    "T_Eye_Normal.png",
    "T_Superhero_Female_Normal.png",
    "T_Superhero_Female_Dark_BaseColor.png",
    "T_Superhero_Female_Roughness.png",
)
SELECTED_ANIMATIONS = ("Idle_Loop", "Walk_Loop", "Jump_Loop")


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


def prepare_character_source(source_dir: Path, archive_path: Path) -> Path:
    character_dir = source_dir / "Character"
    character_dir.mkdir(parents=True, exist_ok=True)

    gltf_name = "Superhero_Female_FullBody.gltf"
    with zipfile.ZipFile(archive_path) as archive:
        gltf_data = json.loads(
            archive.read(f"{BASE_ARCHIVE_ROOT}/{gltf_name}").decode("utf-8")
        )

        # The Standard archive contains the eye normal under this shorter filename.
        for image in gltf_data.get("images", []):
            if image.get("uri") == "T_Eye_Normal_png.png":
                image["uri"] = "T_Eye_Normal.png"

        (character_dir / gltf_name).write_text(
            json.dumps(gltf_data, separators=(",", ":")), encoding="utf-8"
        )
        for filename in CHARACTER_SOURCE_FILES:
            (character_dir / filename).write_bytes(
                archive.read(f"{BASE_ARCHIVE_ROOT}/{filename}")
            )

    return character_dir / gltf_name


def node_translation(gltf_data, node_name: str):
    matching_nodes = [
        node for node in gltf_data.get("nodes", []) if node.get("name") == node_name
    ]
    require(len(matching_nodes) == 1, f"Expected one {node_name} node")
    value = matching_nodes[0].get("translation", [0.0, 0.0, 0.0])
    require(len(value) == 3, f"Invalid {node_name} translation")
    return tuple(float(component) for component in value)


def offset_vec3_accessor(
    source,
    gltf_data,
    accessor_index: int,
    bin_start: int,
    bin_length: int,
    delta,
):
    accessor = gltf_data["accessors"][accessor_index]
    require(accessor.get("componentType") == 5126, "Pelvis translation must use FLOAT")
    require(accessor.get("type") == "VEC3", "Pelvis translation must be VEC3")
    require(not accessor.get("normalized", False), "Normalized pelvis accessor unsupported")
    require("sparse" not in accessor, "Sparse pelvis accessor unsupported")
    require("bufferView" in accessor, "Pelvis accessor has no bufferView")

    buffer_view = gltf_data["bufferViews"][accessor["bufferView"]]
    require(buffer_view.get("buffer", 0) == 0, "Pelvis accessor must use GLB buffer 0")
    element_size = struct.calcsize("<3f")
    stride = int(buffer_view.get("byteStride", element_size))
    require(stride >= element_size and stride % 4 == 0, "Invalid pelvis byteStride")

    count = int(accessor["count"])
    start = (
        bin_start
        + int(buffer_view.get("byteOffset", 0))
        + int(accessor.get("byteOffset", 0))
    )
    end = start if count == 0 else start + (count - 1) * stride + element_size
    require(end <= bin_start + bin_length, "Pelvis accessor exceeds BIN chunk")

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


def prepare_animation_source(
    source_dir: Path, archive_path: Path, target_pelvis_translation
) -> Path:
    animation_dir = source_dir / "Animation"
    animation_dir.mkdir(parents=True, exist_ok=True)
    output_path = animation_dir / "UAL1_VisualValidation.glb"

    with zipfile.ZipFile(archive_path) as archive:
        source = bytearray(archive.read(ANIMATION_ARCHIVE_PATH))

    require(len(source) >= 20, "Animation GLB is too small")
    magic, version, _ = struct.unpack_from("<III", source, 0)
    require(magic == 0x46546C67 and version == 2, "Animation source is not GLB 2.0")
    json_length, json_type = struct.unpack_from("<II", source, 12)
    require(json_type == 0x4E4F534A, "Animation GLB does not start with a JSON chunk")

    json_start = 20
    json_end = json_start + json_length
    gltf_data = json.loads(source[json_start:json_end].decode("utf-8").rstrip(" \x00"))

    bin_length, bin_type = struct.unpack_from("<II", source, json_end)
    require(bin_type == 0x004E4942, "Animation GLB has no BIN chunk after JSON")
    bin_start = json_end + 8
    require(bin_start + bin_length <= len(source), "Animation GLB has a truncated BIN chunk")

    pelvis_node_indices = [
        index
        for index, node in enumerate(gltf_data.get("nodes", []))
        if node.get("name") == "pelvis"
    ]
    require(len(pelvis_node_indices) == 1, "Expected one pelvis node in UAL1")
    pelvis_node_index = pelvis_node_indices[0]
    source_pelvis_translation = node_translation(gltf_data, "pelvis")
    pelvis_translation_delta = tuple(
        target_pelvis_translation[axis] - source_pelvis_translation[axis]
        for axis in range(3)
    )

    selected = []
    patched_accessors = set()
    for animation in gltf_data.get("animations", []):
        if animation.get("name") not in SELECTED_ANIMATIONS:
            continue

        # The library and character share bone names but use different proportions.
        # Keep rotations and a proportion-adjusted pelvis translation for grounded motion.
        pelvis_channels = [
            channel
            for channel in animation.get("channels", [])
            if channel.get("target", {}).get("node") == pelvis_node_index
            and channel.get("target", {}).get("path") == "translation"
        ]
        require(
            len(pelvis_channels) == 1,
            f"{animation.get('name')} must have one pelvis translation channel",
        )
        pelvis_channel = pelvis_channels[0]
        pelvis_sampler = animation["samplers"][pelvis_channel["sampler"]]
        require(
            pelvis_sampler.get("interpolation", "LINEAR") in ("LINEAR", "STEP"),
            "CUBICSPLINE pelvis translation is unsupported",
        )
        output_accessor = pelvis_sampler["output"]
        if output_accessor not in patched_accessors:
            offset_vec3_accessor(
                source,
                gltf_data,
                output_accessor,
                bin_start,
                bin_length,
                pelvis_translation_delta,
            )
            patched_accessors.add(output_accessor)

        kept_channels = [
            channel
            for channel in animation.get("channels", [])
            if channel.get("target", {}).get("path") == "rotation"
            or channel is pelvis_channel
        ]
        used_samplers = sorted({channel["sampler"] for channel in kept_channels})
        sampler_remap = {old_index: new_index for new_index, old_index in enumerate(used_samplers)}
        for channel in kept_channels:
            channel["sampler"] = sampler_remap[channel["sampler"]]

        animation["channels"] = kept_channels
        animation["samplers"] = [animation["samplers"][index] for index in used_samplers]
        selected.append(animation)

    selected_names = {animation.get("name") for animation in selected}
    require(
        selected_names == set(SELECTED_ANIMATIONS),
        f"Missing selected animations: {set(SELECTED_ANIMATIONS) - selected_names}",
    )
    gltf_data["animations"] = selected

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
    output_path.write_bytes(filtered_glb)
    return output_path


def make_pipeline(*, animations_only=False, skeleton=None):
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.set_editor_property("use_source_name_for_asset", False)
    pipeline.set_editor_property("asset_type_sub_folders", False)

    mesh_pipeline = pipeline.get_editor_property("mesh_pipeline")
    mesh_pipeline.set_editor_property("import_static_meshes", False)
    mesh_pipeline.set_editor_property("import_skeletal_meshes", not animations_only)
    mesh_pipeline.set_editor_property("create_physics_asset", False)

    animation_pipeline = pipeline.get_editor_property("animation_pipeline")
    animation_pipeline.set_editor_property("import_animations", animations_only)
    if animations_only:
        animation_pipeline.set_editor_property("use30_hz_to_bake_bone_animation", True)

    material_pipeline = pipeline.get_editor_property("material_pipeline")
    material_pipeline.set_editor_property("import_materials", not animations_only)
    texture_pipeline = material_pipeline.get_editor_property("texture_pipeline")
    texture_pipeline.set_editor_property("import_textures", not animations_only)

    common_skeletal = pipeline.get_editor_property(
        "common_skeletal_meshes_and_animations_properties"
    )
    common_skeletal.set_editor_property("import_only_animations", animations_only)
    if animations_only:
        common_skeletal.set_editor_property("try_auto_select_skeleton", False)
        common_skeletal.set_editor_property("skeleton", require(skeleton, "Missing skeleton"))

    pipeline_stack = unreal.InterchangePipelineStackOverride()
    pipeline_stack.add_pipeline(pipeline)
    return pipeline_stack


def import_source(asset_tools, filename: Path, destination: str, options):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(filename))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])
    unreal.InterchangeManager.get_interchange_manager_scripted().wait_until_all_tasks_done(False)
    imported = list(task.get_editor_property("imported_object_paths"))
    require(imported, f"Import produced no assets for {filename}")
    unreal.log(f"{LOG_PREFIX} Imported {filename.name}: {imported}")
    return imported


def assets_in_path(asset_subsystem, path: str):
    return [
        unreal.load_asset(asset_path)
        for asset_path in asset_subsystem.list_assets(path, recursive=True, include_folder=False)
    ]


def find_assets(asset_subsystem, path: str, asset_class):
    return [asset for asset in assets_in_path(asset_subsystem, path) if isinstance(asset, asset_class)]


def rename_asset(source_asset, destination_path: str):
    source_path = package_path(source_asset.get_path_name())
    if source_path == destination_path:
        return source_asset
    require(
        unreal.EditorAssetLibrary.rename_asset(source_path, destination_path),
        f"Failed to rename {source_path} to {destination_path}",
    )
    return require(unreal.load_asset(destination_path), f"Failed to load {destination_path}")


def import_character(asset_tools, asset_subsystem, source_path: Path):
    skeletal_mesh = load_existing(MESH_ASSET)
    if not skeletal_mesh:
        skeletal_meshes = find_assets(asset_subsystem, MESH_PATH, unreal.SkeletalMesh)
        require(
            len(skeletal_meshes) <= 1,
            f"Cannot recover character import with multiple skeletal meshes: {skeletal_meshes}",
        )
        if skeletal_meshes:
            skeletal_mesh = rename_asset(skeletal_meshes[0], MESH_ASSET)
        else:
            import_source(asset_tools, source_path, MESH_PATH, make_pipeline())
            skeletal_meshes = find_assets(asset_subsystem, MESH_PATH, unreal.SkeletalMesh)
            require(
                len(skeletal_meshes) == 1,
                f"Expected one imported skeletal mesh, found {skeletal_meshes}",
            )
            skeletal_mesh = rename_asset(skeletal_meshes[0], MESH_ASSET)

    skeleton = require(
        skeletal_mesh.get_editor_property("skeleton"),
        f"{MESH_ASSET} has no skeleton",
    )
    if package_path(skeleton.get_path_name()) != SKELETON_ASSET:
        existing_skeleton = load_existing(SKELETON_ASSET)
        if existing_skeleton:
            require(
                package_path(existing_skeleton.get_path_name())
                == package_path(skeleton.get_path_name()),
                f"Unexpected skeleton already exists at {SKELETON_ASSET}",
            )
        else:
            skeleton = rename_asset(skeleton, SKELETON_ASSET)

    return skeletal_mesh, skeleton


def reconcile_animation_assets(expected_names, replace_existing=False):
    for source_name, destination_path in expected_names.items():
        source_path = f"{ANIMATION_PATH}/{source_name}"
        destination_asset = load_existing(destination_path)
        source_asset = load_existing(source_path)
        if destination_asset and source_asset:
            asset_to_delete = destination_path if replace_existing else source_path
            require(
                unreal.EditorAssetLibrary.delete_asset(asset_to_delete),
                f"Failed to remove redundant animation {asset_to_delete}",
            )
            if replace_existing:
                rename_asset(source_asset, destination_path)
        elif source_asset:
            rename_asset(source_asset, destination_path)


def move_asset_from_candidates(source_paths, destination_path: str):
    destination_asset = load_existing(destination_path)
    source_assets = [asset for path in source_paths if (asset := load_existing(path))]
    if destination_asset:
        require(
            not source_assets,
            f"Redundant assets exist for {destination_path}: {source_assets}",
        )
        return destination_asset

    require(
        len(source_assets) == 1,
        f"Expected one source for {destination_path}, found {source_assets}",
    )
    return rename_asset(source_assets[0], destination_path)


def import_animations(asset_tools, asset_subsystem, source_path: Path, skeleton):
    expected_names = {
        "Idle_Loop": IDLE_ASSET,
        "Walk_Loop": MOVE_ASSET,
        "Jump_Loop": JUMP_ASSET,
    }
    if any(load_existing(f"{ANIMATION_PATH}/{name}") for name in expected_names):
        reconcile_animation_assets(expected_names, replace_existing=True)

    needs_refresh = load_existing(LEGACY_MOVE_ASSET) or not all(
        load_existing(path) for path in expected_names.values()
    )
    if needs_refresh:
        import_source(
            asset_tools,
            source_path,
            ANIMATION_PATH,
            make_pipeline(animations_only=True, skeleton=skeleton),
        )
        reconcile_animation_assets(expected_names, replace_existing=True)

    return tuple(
        require(load_existing(path), f"Missing animation {path}")
        for path in (IDLE_ASSET, MOVE_ASSET, JUMP_ASSET)
    )


def organize_character_textures():
    for texture_name in CHARACTER_TEXTURES:
        texture = move_asset_from_candidates(
            (f"{MESH_PATH}/{texture_name}",),
            f"{MATERIAL_PATH}/{texture_name}",
        )
        require(
            isinstance(texture, unreal.Texture),
            f"Expected Texture for {texture_name}",
        )


def configure_character_materials(asset_subsystem):
    for source_paths, material_path in CHARACTER_MATERIALS:
        material = move_asset_from_candidates(source_paths, material_path)
        require(isinstance(material, unreal.Material), f"Expected Material at {material_path}")
        if not material.get_editor_property("used_with_skeletal_mesh"):
            material.modify()
            material.set_editor_property("used_with_skeletal_mesh", True)
            unreal.MaterialEditingLibrary.recompile_material(material)
            require(
                asset_subsystem.save_loaded_asset(material),
                f"Failed to save {material_path}",
            )


def configure_character_blueprint(asset_subsystem, skeletal_mesh, animations):
    blueprint = require(unreal.load_asset(CHARACTER_BLUEPRINT), "Missing BP_CH_Player")
    visual_character_class = require(
        unreal.load_class(None, VISUAL_CHARACTER_CLASS),
        f"Missing {VISUAL_CHARACTER_CLASS}",
    )
    if blueprint.get_blueprint_parent_class() != visual_character_class:
        unreal.BlueprintEditorLibrary.reparent_blueprint(
            blueprint, visual_character_class
        )
        require(
            blueprint.get_blueprint_parent_class() == visual_character_class,
            "Failed to reparent BP_CH_Player for visual validation",
        )
        require(
            unreal.BlueprintEditorLibrary.compile_blueprint(blueprint),
            "Failed to compile reparented BP_CH_Player",
        )

    character_cdo = require(
        unreal.get_default_object(blueprint.generated_class()),
        "BP_CH_Player has no class default object",
    )
    mesh_component = require(character_cdo.get_editor_property("mesh"), "Character has no mesh")
    movement_component = require(
        character_cdo.get_editor_property("character_movement"),
        "Character has no movement component",
    )

    character_cdo.modify()
    mesh_component.modify()
    movement_component.modify()
    mesh_component.set_editor_property("skeletal_mesh_asset", skeletal_mesh)
    mesh_component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, -88.0))
    mesh_component.set_editor_property(
        "relative_rotation", unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0)
    )
    character_cdo.set_editor_property("idle_animation", animations[0])
    character_cdo.set_editor_property("move_animation", animations[1])
    character_cdo.set_editor_property("jump_animation", animations[2])
    movement_component.set_editor_property("max_walk_speed", 250.0)

    require(
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint),
        "Failed to compile BP_CH_Player",
    )
    require(asset_subsystem.save_loaded_asset(blueprint), "Failed to save BP_CH_Player")


def remove_legacy_animation_assets():
    if load_existing(LEGACY_MOVE_ASSET):
        require(
            unreal.EditorAssetLibrary.delete_asset(LEGACY_MOVE_ASSET),
            f"Failed to remove obsolete {LEGACY_MOVE_ASSET}",
        )


def main():
    unreal.log(f"{LOG_PREFIX} Preparing minimal visual-validation assets")
    downloads = Path.home() / "Downloads"
    base_archive = require(downloads / BASE_ZIP_NAME, f"Missing {BASE_ZIP_NAME}")
    animation_archive = require(
        downloads / ANIMATION_ZIP_NAME, f"Missing {ANIMATION_ZIP_NAME}"
    )
    require(base_archive.is_file(), f"Missing {base_archive}")
    require(animation_archive.is_file(), f"Missing {animation_archive}")

    source_dir = Path(unreal.Paths.project_saved_dir()) / "ImportSources" / "VisualFoundation"
    character_source = prepare_character_source(source_dir, base_archive)
    character_gltf = json.loads(character_source.read_text(encoding="utf-8"))
    target_pelvis_translation = node_translation(character_gltf, "pelvis")
    animation_source = prepare_animation_source(
        source_dir, animation_archive, target_pelvis_translation
    )

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    for folder in (MESH_PATH, MATERIAL_PATH, ANIMATION_PATH):
        if not asset_subsystem.does_directory_exist(folder):
            asset_subsystem.make_directory(folder)

    skeletal_mesh, skeleton = import_character(
        asset_tools, asset_subsystem, character_source
    )
    animations = import_animations(
        asset_tools, asset_subsystem, animation_source, skeleton
    )
    organize_character_textures()
    configure_character_materials(asset_subsystem)
    configure_character_blueprint(asset_subsystem, skeletal_mesh, animations)
    remove_legacy_animation_assets()

    require(level_subsystem.load_level(MAP_PATH), f"Failed to load {MAP_PATH}")
    unreal.EditorAssetLibrary.save_directory(
        f"{PROJECT_ROOT}/Characters/Player", only_if_is_dirty=True, recursive=True
    )
    unreal.log(
        f"{LOG_PREFIX} Visual foundation configured with {skeletal_mesh.get_path_name()} "
        f"and {[animation.get_path_name() for animation in animations]}"
    )


if __name__ == "__main__":
    main()
