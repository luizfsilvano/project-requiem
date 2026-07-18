"""Import and configure the first player sword-combat presentation assets.

Run from Unreal Editor Python only after rebuilding the native ProjectRequiem
module that exposes the sword properties used near the end of this script.
"""

from __future__ import annotations

import json
import math
import struct
import zipfile
from pathlib import Path

import unreal


LOG_PREFIX = "[ProjectRequiem.SwordCombat]"
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

INPUT_ROOT = f"{PROJECT_ROOT}/Core/Input"
INPUT_ACTION_ROOT = f"{INPUT_ROOT}/Actions"
TOGGLE_COMBAT_ACTION = f"{INPUT_ACTION_ROOT}/IA_ToggleCombat"
PRIMARY_ATTACK_ACTION = f"{INPUT_ACTION_ROOT}/IA_PrimaryAttack"
MAPPING_CONTEXT = f"{INPUT_ROOT}/IMC_Exploration"

WEAPON_ROOT = f"{PROJECT_ROOT}/Combat/Styles/Sword/Weapons"
SWORD_MESH_ASSET = f"{WEAPON_ROOT}/SM_Sword_Bronze"
SWORD_MATERIAL_ASSET = f"{WEAPON_ROOT}/M_Sword_Bronze"
SWORD_BASE_COLOR_ASSET = f"{WEAPON_ROOT}/T_Sword_Bronze_BaseColor"
SWORD_NORMAL_ASSET = f"{WEAPON_ROOT}/T_Sword_Bronze_Normal"
SWORD_ORM_ASSET = f"{WEAPON_ROOT}/T_Sword_Bronze_ORM"
EXPECTED_WEAPON_ASSETS = {
    SWORD_MESH_ASSET: unreal.StaticMesh,
    SWORD_MATERIAL_ASSET: unreal.Material,
    SWORD_BASE_COLOR_ASSET: unreal.Texture2D,
    SWORD_NORMAL_ASSET: unreal.Texture2D,
    SWORD_ORM_ASSET: unreal.Texture2D,
}

SWORD_ANIMATION_ROOT = (
    f"{PROJECT_ROOT}/Characters/Player/Animations/Combat/Sword"
)
SWORD_UAL1_PATH = f"{SWORD_ANIMATION_ROOT}/UAL1"
SWORD_UAL2_PATH = f"{SWORD_ANIMATION_ROOT}/UAL2"
SWORD_UAL1_RM_PATH = f"{SWORD_ANIMATION_ROOT}/UAL1_RM"

BASE_ZIP_NAME = "Universal Base Characters[Standard].zip"
BASE_CHARACTER_GLTF = (
    "Universal Base Characters[Standard]/Base Characters/Godot - UE/"
    "Superhero_Female_FullBody.gltf"
)
UAL1_ZIP_NAME = "Universal Animation Library[Pro].zip"
UAL1_ARCHIVE_PATH = "Unreal-Godot/UAL1.glb"
UAL1_ROOT_MOTION_ARCHIVE_PATH = "Unreal-Godot/UAL1_RM.glb"
UAL2_ZIP_NAME = "Universal Animation Library 2[Source].zip"
UAL2_ARCHIVE_PATH = "Unreal-Godot/UAL2.glb"

SWORD_ZIP_NAME = "Fantasy Props MegaKit[Standard].zip"
SWORD_GLTF_ARCHIVE_PATH = "Exports/glTF/Sword_Bronze.gltf"
SWORD_BIN_ARCHIVE_PATH = "Exports/glTF/Sword_Bronze.bin"
SWORD_BASE_COLOR_ARCHIVE_PATH = "Textures/T_Trim_Props_BaseColor.png"
SWORD_NORMAL_ARCHIVE_PATH = (
    "Textures/Normals-UnrealEngine/T_Trim_Props_Normal.png"
)
SWORD_ORM_ARCHIVE_PATH = "Textures/T_Trim_Props_ORM.png"
SWORD_SOURCE_LICENSE = "CC0 1.0"

WEAPON_SOURCE_FILENAMES = {
    "gltf": "SM_Sword_Bronze.gltf",
    "bin": "SM_Sword_Bronze.bin",
    "base_color": "T_Sword_Bronze_BaseColor.png",
    "normal": "T_Sword_Bronze_Normal.png",
    "orm": "T_Sword_Bronze_ORM.png",
}

UAL1_ANIMATIONS = (
    "Sword_Enter",
    "Sword_Idle",
    "Sword_Exit",
)
UAL2_ANIMATIONS = (
    "Sword_Regular_A",
    "Sword_Regular_A_Rec",
    "Sword_Regular_B",
    "Sword_Regular_B_Rec",
    "Sword_Regular_C",
)
ROOT_MOTION_SOURCE_ANIMATION = "Sword_Attack"
ROOT_MOTION_FINAL_ANIMATION = "Sword_Attack_RM"

ANIMATION_PROPERTIES = {
    "Sword_Enter": "sword_enter_animation",
    "Sword_Idle": "sword_idle_animation",
    "Sword_Exit": "sword_exit_animation",
    "Sword_Regular_A": "sword_regular_a_animation",
    "Sword_Regular_A_Rec": "sword_regular_a_recovery_animation",
    "Sword_Regular_B": "sword_regular_b_animation",
    "Sword_Regular_B_Rec": "sword_regular_b_recovery_animation",
    "Sword_Regular_C": "sword_regular_c_animation",
    ROOT_MOTION_FINAL_ANIMATION: "sword_heavy_attack_animation",
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


def same_asset(left, right) -> bool:
    if not left or not right:
        return False
    return package_path(left.get_path_name()) == package_path(right.get_path_name())


def ensure_content_directory(asset_subsystem, folder: str):
    if not asset_subsystem.does_directory_exist(folder):
        asset_subsystem.make_directory(folder)
    require(
        asset_subsystem.does_directory_exist(folder),
        f"Failed to create content folder {folder}",
    )


def list_asset_packages(asset_subsystem, folder: str):
    if not asset_subsystem.does_directory_exist(folder):
        return set()
    return {
        package_path(asset_path)
        for asset_path in asset_subsystem.list_assets(
            folder,
            recursive=False,
            include_folder=False,
        )
    }


def rename_asset(source_asset, destination_path: str):
    source_path = package_path(source_asset.get_path_name())
    if source_path == destination_path:
        return source_asset
    require(
        not load_existing(destination_path),
        f"Cannot rename {source_path}; {destination_path} already exists",
    )
    require(
        unreal.EditorAssetLibrary.rename_asset(source_path, destination_path),
        f"Failed to rename {source_path} to {destination_path}",
    )
    return require(
        load_existing(destination_path),
        f"Failed to load renamed asset {destination_path}",
    )


def read_archive_member(archive, archive_path: str, archive_description: str):
    try:
        return archive.read(archive_path)
    except KeyError as exc:
        raise RuntimeError(
            f"{LOG_PREFIX} Missing {archive_path} in {archive_description}"
        ) from exc


def validate_identity_node(node):
    require("matrix" not in node, "Sword source node must not use a matrix transform")
    translation = tuple(float(value) for value in node.get("translation", (0, 0, 0)))
    rotation = tuple(float(value) for value in node.get("rotation", (0, 0, 0, 1)))
    scale = tuple(float(value) for value in node.get("scale", (1, 1, 1)))
    require(translation == (0.0, 0.0, 0.0), "Sword source node is translated")
    require(rotation == (0.0, 0.0, 0.0, 1.0), "Sword source node is rotated")
    require(scale == (1.0, 1.0, 1.0), "Sword source node is scaled")


def texture_image_index(gltf_data, texture_reference, description: str):
    require(isinstance(texture_reference, dict), f"Missing {description}")
    texture_index = int(texture_reference.get("index", -1))
    textures = gltf_data["textures"]
    require(
        0 <= texture_index < len(textures),
        f"Invalid texture index for {description}",
    )
    image_index = int(textures[texture_index].get("source", -1))
    require(
        0 <= image_index < len(gltf_data["images"]),
        f"Invalid image index for {description}",
    )
    return image_index


def validate_and_patch_sword_gltf(gltf_data, binary_data: bytes):
    require(len(gltf_data.get("scenes", [])) == 1, "Expected exactly one sword scene")
    require(len(gltf_data.get("nodes", [])) == 1, "Expected exactly one sword node")
    require(len(gltf_data.get("meshes", [])) == 1, "Expected exactly one sword mesh")
    require(
        len(gltf_data.get("materials", [])) == 1,
        "Expected exactly one sword material",
    )
    require(len(gltf_data.get("buffers", [])) == 1, "Expected exactly one sword buffer")
    require(len(gltf_data.get("textures", [])) == 3, "Expected exactly three textures")
    require(len(gltf_data.get("images", [])) == 3, "Expected exactly three images")
    require(not gltf_data.get("animations"), "Sword model unexpectedly contains animations")
    require(not gltf_data.get("skins"), "Sword model unexpectedly contains a skin")

    scene = gltf_data["scenes"][0]
    require(scene.get("nodes") == [0], "Sword scene must reference only node zero")
    node = gltf_data["nodes"][0]
    require(node.get("mesh") == 0, "Sword node must reference only mesh zero")
    validate_identity_node(node)

    mesh = gltf_data["meshes"][0]
    primitives = mesh.get("primitives", [])
    require(len(primitives) == 1, "Expected exactly one sword mesh primitive")
    primitive = primitives[0]
    require(primitive.get("material") == 0, "Sword primitive must use material zero")
    attributes = set(primitive.get("attributes", {}))
    required_attributes = {"POSITION", "NORMAL", "TEXCOORD_0", "COLOR_0"}
    require(
        required_attributes.issubset(attributes),
        f"Sword primitive is missing attributes {required_attributes - attributes}",
    )

    buffer = gltf_data["buffers"][0]
    require(
        buffer.get("uri") == Path(SWORD_BIN_ARCHIVE_PATH).name,
        f"Unexpected sword buffer URI {buffer.get('uri')}",
    )
    require(
        int(buffer.get("byteLength", -1)) == len(binary_data),
        "Sword binary byte count does not match the glTF buffer declaration",
    )

    material = gltf_data["materials"][0]
    require(material.get("doubleSided") is True, "Sword source material is not two-sided")
    pbr = require(material.get("pbrMetallicRoughness"), "Missing sword PBR material data")
    normal_image = texture_image_index(
        gltf_data,
        material.get("normalTexture"),
        "normal texture",
    )
    base_color_image = texture_image_index(
        gltf_data,
        pbr.get("baseColorTexture"),
        "base-color texture",
    )
    orm_image = texture_image_index(
        gltf_data,
        pbr.get("metallicRoughnessTexture"),
        "ORM metallic/roughness texture",
    )
    require(
        len({normal_image, base_color_image, orm_image}) == 3,
        "Sword material must reference three distinct images",
    )

    images = gltf_data["images"]
    require(
        images[normal_image].get("uri") == "T_Trim_Props_Normal.png",
        "Sword normal image reference changed in the source asset",
    )
    require(
        images[base_color_image].get("uri") == "T_Trim_Props_BaseColor.png",
        "Sword base-color image reference changed in the source asset",
    )
    require(
        images[orm_image].get("uri") == "T_Trim_Props_ORM.png",
        "Sword ORM image reference changed in the source asset",
    )

    node["name"] = "SM_Sword_Bronze"
    mesh["name"] = "SM_Sword_Bronze"
    material["name"] = "M_Sword_Bronze"
    buffer["uri"] = WEAPON_SOURCE_FILENAMES["bin"]

    images[base_color_image]["name"] = "T_Sword_Bronze_BaseColor"
    images[base_color_image]["uri"] = WEAPON_SOURCE_FILENAMES["base_color"]
    images[normal_image]["name"] = "T_Sword_Bronze_Normal"
    images[normal_image]["uri"] = WEAPON_SOURCE_FILENAMES["normal"]
    images[orm_image]["name"] = "T_Sword_Bronze_ORM"
    images[orm_image]["uri"] = WEAPON_SOURCE_FILENAMES["orm"]

    # glTF metallicRoughnessTexture already maps G to roughness and B to metallic.
    # Reusing the same texture as occlusionTexture maps its R channel to AO.
    material["occlusionTexture"] = {
        "index": int(pbr["metallicRoughnessTexture"]["index"])
    }
    return gltf_data


def prepare_weapon_source(output_directory: Path, sword_archive_path: Path):
    require(sword_archive_path.is_file(), f"Missing sword archive {sword_archive_path}")
    output_directory.mkdir(parents=True, exist_ok=True)

    expected_output_names = set(WEAPON_SOURCE_FILENAMES.values())
    unexpected_existing = {
        path.name for path in output_directory.iterdir()
    } - expected_output_names
    require(
        not unexpected_existing,
        f"Refusing to reuse weapon source folder with unexpected entries: "
        f"{sorted(unexpected_existing)}",
    )

    with zipfile.ZipFile(sword_archive_path) as archive:
        gltf_source = read_archive_member(
            archive,
            SWORD_GLTF_ARCHIVE_PATH,
            sword_archive_path.name,
        )
        binary_data = read_archive_member(
            archive,
            SWORD_BIN_ARCHIVE_PATH,
            sword_archive_path.name,
        )
        base_color_data = read_archive_member(
            archive,
            SWORD_BASE_COLOR_ARCHIVE_PATH,
            sword_archive_path.name,
        )
        # Deliberately read only the Unreal-normal variant. The OpenGL normal
        # stored next to the exported glTF is not approved for this import.
        normal_data = read_archive_member(
            archive,
            SWORD_NORMAL_ARCHIVE_PATH,
            sword_archive_path.name,
        )
        orm_data = read_archive_member(
            archive,
            SWORD_ORM_ARCHIVE_PATH,
            sword_archive_path.name,
        )

    try:
        gltf_data = json.loads(gltf_source.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"{LOG_PREFIX} Invalid Sword_Bronze glTF JSON") from exc
    validate_and_patch_sword_gltf(gltf_data, binary_data)

    output_paths = {
        key: output_directory / filename
        for key, filename in WEAPON_SOURCE_FILENAMES.items()
    }
    output_paths["gltf"].write_text(
        json.dumps(gltf_data, separators=(",", ":")),
        encoding="utf-8",
        newline="\n",
    )
    output_paths["bin"].write_bytes(binary_data)
    output_paths["base_color"].write_bytes(base_color_data)
    output_paths["normal"].write_bytes(normal_data)
    output_paths["orm"].write_bytes(orm_data)

    actual_output_names = {path.name for path in output_directory.iterdir()}
    require(
        actual_output_names == expected_output_names,
        f"Weapon source folder must contain exactly {sorted(expected_output_names)}; "
        f"found {sorted(actual_output_names)}",
    )
    unreal.log(
        f"{LOG_PREFIX} Prepared one Sword_Bronze mesh and its three approved "
        f"textures ({SWORD_SOURCE_LICENSE}); Unreal normal selected"
    )
    return output_paths["gltf"]


def make_weapon_pipeline():
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    pipeline.set_editor_property("use_source_name_for_asset", False)
    pipeline.set_editor_property("asset_type_sub_folders", False)

    mesh_pipeline = pipeline.get_editor_property("mesh_pipeline")
    mesh_pipeline.set_editor_property("import_static_meshes", True)
    mesh_pipeline.set_editor_property("import_skeletal_meshes", False)
    mesh_pipeline.set_editor_property("create_physics_asset", False)

    animation_pipeline = pipeline.get_editor_property("animation_pipeline")
    animation_pipeline.set_editor_property("import_animations", False)

    material_pipeline = pipeline.get_editor_property("material_pipeline")
    material_pipeline.set_editor_property("import_materials", True)
    material_pipeline.get_editor_property("texture_pipeline").set_editor_property(
        "import_textures",
        True,
    )

    common_meshes = pipeline.get_editor_property("common_meshes_properties")
    common_meshes.set_editor_property("bake_meshes", True)
    common_meshes.set_editor_property("bake_pivot_meshes", False)
    vertex_color_option = common_meshes.get_editor_property(
        "vertex_color_import_option"
    )
    require(
        "replace" in str(vertex_color_option).lower(),
        f"Weapon pipeline must replace vertex colors, got {vertex_color_option}",
    )
    require(common_meshes.get_editor_property("bake_meshes"), "Mesh baking disabled")
    require(
        not common_meshes.get_editor_property("bake_pivot_meshes"),
        "Pivot baking must stay disabled so the authored grip pivot is preserved",
    )

    stack = unreal.InterchangePipelineStackOverride()
    stack.add_pipeline(pipeline)
    return stack


def import_source(asset_tools, source_path: Path, destination: str, options):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_path))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])
    unreal.InterchangeManager.get_interchange_manager_scripted().wait_until_all_tasks_done(
        False
    )
    imported = list(task.get_editor_property("imported_object_paths"))
    require(imported, f"Import produced no assets for {source_path.name}")
    unreal.log(f"{LOG_PREFIX} Imported {source_path.name}: {imported}")
    return imported


def recover_weapon_asset(
    asset_subsystem,
    expected_path: str,
    expected_type,
    candidate_paths,
    name_token: str,
):
    existing = load_existing(expected_path)
    if existing:
        require(
            isinstance(existing, expected_type),
            f"Expected {expected_type.__name__} at {expected_path}",
        )
        return existing

    candidates = []
    for candidate_path in candidate_paths:
        candidate = load_existing(candidate_path)
        if not isinstance(candidate, expected_type):
            continue
        if name_token.lower() not in candidate.get_name().lower():
            continue
        candidates.append(candidate)
    require(
        len(candidates) == 1,
        f"Expected one {expected_type.__name__} candidate for {expected_path}; "
        f"found {[asset.get_path_name() for asset in candidates]}",
    )
    return rename_asset(candidates[0], expected_path)


def validate_weapon_asset_types():
    resolved = {}
    for asset_path, expected_type in EXPECTED_WEAPON_ASSETS.items():
        asset = require(load_existing(asset_path), f"Missing weapon asset {asset_path}")
        require(
            isinstance(asset, expected_type),
            f"Expected {expected_type.__name__} at {asset_path}, got {type(asset).__name__}",
        )
        require(
            asset.get_name() == asset_path.rsplit("/", maxsplit=1)[-1],
            f"Unexpected object name at {asset_path}",
        )
        resolved[asset_path] = asset
    require(
        sum(isinstance(asset, unreal.StaticMesh) for asset in resolved.values()) == 1,
        "Weapon setup must resolve exactly one StaticMesh",
    )
    require(
        sum(isinstance(asset, unreal.Material) for asset in resolved.values()) == 1,
        "Weapon setup must resolve exactly one Material",
    )
    require(
        sum(isinstance(asset, unreal.Texture2D) for asset in resolved.values()) == 3,
        "Weapon setup must resolve exactly three Texture2D assets",
    )
    return resolved


def import_weapon_assets(
    asset_tools,
    asset_subsystem,
    weapon_source_path: Path,
):
    expected_paths = set(EXPECTED_WEAPON_ASSETS)
    complete = all(load_existing(path) for path in expected_paths)
    if not complete:
        before_paths = list_asset_packages(asset_subsystem, WEAPON_ROOT)
        import_source(
            asset_tools,
            weapon_source_path,
            WEAPON_ROOT,
            make_weapon_pipeline(),
        )
        after_import_paths = list_asset_packages(asset_subsystem, WEAPON_ROOT)
        candidate_paths = (after_import_paths - before_paths) | (
            after_import_paths & expected_paths
        )

        recover_weapon_asset(
            asset_subsystem,
            SWORD_MESH_ASSET,
            unreal.StaticMesh,
            candidate_paths,
            "sword_bronze",
        )
        recover_weapon_asset(
            asset_subsystem,
            SWORD_MATERIAL_ASSET,
            unreal.Material,
            candidate_paths,
            "sword_bronze",
        )
        recover_weapon_asset(
            asset_subsystem,
            SWORD_BASE_COLOR_ASSET,
            unreal.Texture2D,
            candidate_paths,
            "basecolor",
        )
        recover_weapon_asset(
            asset_subsystem,
            SWORD_NORMAL_ASSET,
            unreal.Texture2D,
            candidate_paths,
            "normal",
        )
        recover_weapon_asset(
            asset_subsystem,
            SWORD_ORM_ASSET,
            unreal.Texture2D,
            candidate_paths,
            "orm",
        )

        after_recovery_paths = list_asset_packages(asset_subsystem, WEAPON_ROOT)
        unexpected_new_paths = (after_recovery_paths - before_paths) - expected_paths
        require(
            not unexpected_new_paths,
            f"Weapon import created unexpected assets: {sorted(unexpected_new_paths)}",
        )

    return validate_weapon_asset_types()


def set_texture_settings(texture, *, srgb: bool, compression, flip_green=False):
    texture.modify()
    texture.set_editor_property("srgb", srgb)
    texture.set_editor_property("compression_settings", compression)
    texture.set_editor_property("flip_green_channel", flip_green)


def ensure_vertex_color_material_graph(material, base_color):
    expressions = list(
        unreal.MaterialEditingLibrary.get_material_expressions(material) or []
    )
    if any(
        isinstance(expression, unreal.MaterialExpressionVertexColor)
        for expression in expressions
    ):
        return

    # UE 5.8's glTF Interchange pipeline routes the imported texture objects
    # through MF_Default_Body instead of creating ordinary texture-sample
    # expressions.  Preserve that complete PBR graph and insert COLOR_0 only
    # between its BaseColor output and the material's Base Color property.
    base_color_source = require(
        unreal.MaterialEditingLibrary.get_material_property_input_node(
            material,
            unreal.MaterialProperty.MP_BASE_COLOR,
        ),
        "Imported sword material has no Base Color source",
    )
    base_color_output = (
        unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
            material,
            unreal.MaterialProperty.MP_BASE_COLOR,
        )
    )
    require(base_color_output, "Imported sword Base Color source has no output name")

    vertex_color = require(
        unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionVertexColor,
            -650,
            -150,
        ),
        "Failed to create the sword vertex-color expression",
    )
    multiply = require(
        unreal.MaterialEditingLibrary.create_material_expression(
            material,
            unreal.MaterialExpressionMultiply,
            -300,
            -100,
        ),
        "Failed to create the sword BaseColor/COLOR_0 multiply expression",
    )
    require(
        unreal.MaterialEditingLibrary.connect_material_expressions(
            base_color_source,
            base_color_output,
            multiply,
            "A",
        ),
        "Failed to connect the imported sword BaseColor to COLOR_0 multiply",
    )
    require(
        unreal.MaterialEditingLibrary.connect_material_expressions(
            vertex_color,
            "",
            multiply,
            "B",
        ),
        "Failed to connect the sword vertex color to BaseColor multiply",
    )
    require(
        unreal.MaterialEditingLibrary.connect_material_property(
            multiply,
            "",
            unreal.MaterialProperty.MP_BASE_COLOR,
        ),
        "Failed to connect the sword COLOR_0 multiply to Base Color",
    )
    unreal.log(
        f"{LOG_PREFIX} Added explicit BaseColor * COLOR_0 material composition"
    )


def validate_material_graph(material, textures):
    expressions = list(
        unreal.MaterialEditingLibrary.get_material_expressions(material) or []
    )
    require(
        any(
            isinstance(expression, unreal.MaterialExpressionVertexColor)
            for expression in expressions
        ),
        "M_Sword_Bronze does not preserve the glTF COLOR_0 vertex-color input",
    )

    sampled_texture_paths = set()
    for expression in expressions:
        if not isinstance(
            expression,
            (unreal.MaterialExpressionTextureSample, unreal.MaterialExpressionTextureObject),
        ):
            continue
        texture = expression.get_editor_property("texture")
        if texture:
            sampled_texture_paths.add(package_path(texture.get_path_name()))
    expected_texture_paths = {
        package_path(texture.get_path_name()) for texture in textures
    }
    require(
        expected_texture_paths.issubset(sampled_texture_paths),
        f"M_Sword_Bronze is missing texture samples for "
        f"{sorted(expected_texture_paths - sampled_texture_paths)}",
    )
    base_color_input = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material,
        unreal.MaterialProperty.MP_BASE_COLOR,
    )
    require(
        isinstance(base_color_input, unreal.MaterialExpressionMultiply),
        "M_Sword_Bronze Base Color is not composed with COLOR_0",
    )


def configure_weapon_assets(asset_subsystem, assets):
    static_mesh = assets[SWORD_MESH_ASSET]
    material = assets[SWORD_MATERIAL_ASSET]
    base_color = assets[SWORD_BASE_COLOR_ASSET]
    normal = assets[SWORD_NORMAL_ASSET]
    orm = assets[SWORD_ORM_ASSET]

    set_texture_settings(
        base_color,
        srgb=True,
        compression=unreal.TextureCompressionSettings.TC_DEFAULT,
    )
    # The selected source is already the Unreal Engine normal-map variant. The
    # glTF translator may enable green-channel flipping automatically, so force
    # it off after import to avoid inverting the channel twice.
    set_texture_settings(
        normal,
        srgb=False,
        compression=unreal.TextureCompressionSettings.TC_NORMALMAP,
        flip_green=False,
    )
    set_texture_settings(
        orm,
        srgb=False,
        # MF_Default_Body samples the packed glTF texture as Linear Color.
        # Keeping TC_DEFAULT avoids a sampler-type mismatch while sRGB=false
        # still preserves the authored R=AO/G=Roughness/B=Metallic values.
        compression=unreal.TextureCompressionSettings.TC_DEFAULT,
    )
    require(
        asset_subsystem.save_loaded_assets([base_color, normal, orm]),
        "Failed to save sword textures",
    )

    require(base_color.get_editor_property("srgb"), "BaseColor must use sRGB")
    require(not normal.get_editor_property("srgb"), "Normal texture must be linear")
    require(
        normal.get_editor_property("compression_settings")
        == unreal.TextureCompressionSettings.TC_NORMALMAP,
        "Normal texture compression is incorrect",
    )
    require(
        not normal.get_editor_property("flip_green_channel"),
        "Unreal normal texture must not flip its green channel again",
    )
    require(not orm.get_editor_property("srgb"), "ORM texture must be linear")
    require(
        orm.get_editor_property("compression_settings")
        == unreal.TextureCompressionSettings.TC_DEFAULT,
        "ORM texture compression is incorrect",
    )

    material.modify()
    material.set_editor_property("two_sided", True)
    ensure_vertex_color_material_graph(material, base_color)
    unreal.MaterialEditingLibrary.recompile_material(material)
    validate_material_graph(material, (base_color, normal, orm))
    require(
        asset_subsystem.save_loaded_asset(material),
        f"Failed to save {SWORD_MATERIAL_ASSET}",
    )

    static_materials = list(static_mesh.get_editor_property("static_materials") or [])
    require(
        len(static_materials) == 1,
        f"SM_Sword_Bronze must have one material slot, found {len(static_materials)}",
    )
    static_mesh.modify()
    static_mesh.set_material(0, material)
    nanite_settings = static_mesh.get_editor_property("nanite_settings")
    nanite_settings.set_editor_property("enabled", False)
    static_mesh.set_editor_property("nanite_settings", nanite_settings)
    static_materials = list(static_mesh.get_editor_property("static_materials") or [])
    assigned_material = require(
        static_materials[0].get_editor_property("material_interface"),
        "SM_Sword_Bronze material slot is empty",
    )
    require(
        same_asset(assigned_material, material),
        "SM_Sword_Bronze does not use M_Sword_Bronze",
    )
    require(
        not static_mesh.get_editor_property("nanite_settings").get_editor_property(
            "enabled"
        ),
        "Nanite must stay disabled for the low-poly sword",
    )

    static_mesh_subsystem = unreal.get_editor_subsystem(
        unreal.StaticMeshEditorSubsystem
    )
    if static_mesh_subsystem is not None:
        require(
            static_mesh_subsystem.has_vertex_colors(static_mesh),
            "SM_Sword_Bronze lost its authored COLOR_0 vertex colors",
        )
    else:
        # PythonScriptCommandlet does not instantiate this editor subsystem.
        # The source validator and Interchange replace policy above still make
        # COLOR_0 mandatory and the material graph consumes it explicitly.
        unreal.log(
            f"{LOG_PREFIX} StaticMeshEditorSubsystem unavailable in commandlet; "
            "COLOR_0 validated from source and import policy"
        )
    require(
        asset_subsystem.save_loaded_asset(static_mesh),
        f"Failed to save {SWORD_MESH_ASSET}",
    )
    return static_mesh


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
        source = read_archive_member(
            archive,
            BASE_CHARACTER_GLTF,
            base_archive.name,
        )
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
        f"Accessor {accessor_index} must use GLB buffer zero",
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
        gltf_data,
        accessor_index,
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
        gltf_data,
        accessor_index,
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
        source = bytearray(
            read_archive_member(archive, archive_entry, archive_path.name)
        )

    require(len(source) >= 28, f"Animation source {archive_entry} is too small")
    magic, version, _ = struct.unpack_from("<III", source, 0)
    require(magic == 0x46546C67 and version == 2, f"{archive_entry} is not GLB 2.0")
    json_length, json_type = struct.unpack_from("<II", source, 12)
    require(json_type == 0x4E4F534A, f"{archive_entry} has no leading JSON chunk")

    json_start = 20
    json_end = json_start + json_length
    gltf_data = json.loads(source[json_start:json_end].decode("utf-8").rstrip(" \x00"))
    require(json_end + 8 <= len(source), f"{archive_entry} has no BIN chunk header")
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


def require_unique_animation_names(gltf_data, expected_names, archive_entry: str):
    expected_names = tuple(expected_names)
    require(
        len(expected_names) == len(set(expected_names)),
        f"Duplicate requested animation name for {archive_entry}",
    )
    source_names = [
        animation.get("name") for animation in gltf_data.get("animations", [])
    ]
    for expected_name in expected_names:
        require(
            source_names.count(expected_name) == 1,
            f"Expected exactly one {expected_name} in {archive_entry}; "
            f"found {source_names.count(expected_name)}",
        )


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
    require_unique_animation_names(gltf_data, selected_animations, archive_entry)

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

        # In-place clips retain authored rotations plus the pelvis translation
        # aligned to the temporary project skeleton. All other translations and
        # scales, including root displacement, are intentionally removed.
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

    selected_names = [animation.get("name") for animation in selected]
    require(
        set(selected_names) == set(selected_animations)
        and len(selected_names) == len(selected_animations),
        f"Unexpected filtered animation names in {archive_entry}: {selected_names}",
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
    source_animation_name: str,
    final_animation_name: str,
    target_pelvis_translation,
):
    require(
        "_RM" in Path(archive_entry).stem,
        f"Expected a root-motion archive, got {archive_entry}",
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
    require_unique_animation_names(
        gltf_data,
        (source_animation_name,),
        archive_entry,
    )

    node_indices = {}
    for node_name in ("root", "pelvis"):
        indices = [
            index
            for index, node in enumerate(gltf_data.get("nodes", []))
            if node.get("name") == node_name
        ]
        require(len(indices) == 1, f"Expected one {node_name} node in {archive_entry}")
        node_indices[node_name] = indices[0]

    animation = next(
        animation
        for animation in gltf_data.get("animations", [])
        if animation.get("name") == source_animation_name
    )

    def find_translation_channel(node_name: str):
        channels = [
            channel
            for channel in animation.get("channels", [])
            if channel.get("target", {}).get("node") == node_indices[node_name]
            and channel.get("target", {}).get("path") == "translation"
        ]
        require(
            len(channels) == 1,
            f"{source_animation_name} must contain one {node_name} translation channel",
        )
        channel = channels[0]
        sampler = animation["samplers"][channel["sampler"]]
        require(
            sampler.get("interpolation", "LINEAR") in ("LINEAR", "STEP"),
            f"Unsupported {node_name} interpolation in {source_animation_name}",
        )
        return channel, sampler

    root_translation_channel, root_translation_sampler = find_translation_channel(
        "root"
    )
    pelvis_translation_channel, pelvis_translation_sampler = find_translation_channel(
        "pelvis"
    )
    root_delta = vec3_accessor_delta(
        source,
        gltf_data,
        int(root_translation_sampler["output"]),
        bin_start,
        bin_length,
    )
    require(
        all(math.isfinite(component) for component in root_delta),
        f"{source_animation_name} root delta is not finite: {root_delta}",
    )
    require(
        sum(component * component for component in root_delta) > 0.01,
        f"{source_animation_name} root translation contains no authored displacement",
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

    # Root-motion clips retain rotations, authored root displacement and the
    # pelvis translation adjusted to the temporary player skeleton.
    kept_channels = [
        channel
        for channel in animation.get("channels", [])
        if channel.get("target", {}).get("path") == "rotation"
        or channel is root_translation_channel
        or channel is pelvis_translation_channel
    ]
    require(kept_channels, f"{source_animation_name} has no supported channels")
    used_samplers = sorted({channel["sampler"] for channel in kept_channels})
    sampler_remap = {
        old_index: new_index for new_index, old_index in enumerate(used_samplers)
    }
    for channel in kept_channels:
        channel["sampler"] = sampler_remap[channel["sampler"]]
    animation["channels"] = kept_channels
    animation["samplers"] = [
        animation["samplers"][index] for index in used_samplers
    ]
    animation["name"] = final_animation_name
    gltf_data["animations"] = [animation]
    require(
        [item.get("name") for item in gltf_data["animations"]]
        == [final_animation_name],
        f"Failed to rename filtered root-motion clip to {final_animation_name}",
    )
    unreal.log(
        f"{LOG_PREFIX} {source_animation_name} root delta confirmed as {root_delta}; "
        f"filtered name is {final_animation_name}"
    )
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
        "import_textures",
        False,
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
    require(
        len(animation_names) == len(set(animation_names)),
        f"Duplicate final animation name requested for {destination}",
    )
    expected = {name: f"{destination}/{name}" for name in animation_names}
    if not all(load_existing(path) for path in expected.values()):
        before_paths = list_asset_packages(asset_subsystem, destination)
        import_source(
            asset_tools,
            source_path,
            destination,
            make_animation_pipeline(skeleton),
        )
        after_paths = list_asset_packages(asset_subsystem, destination)
        unexpected_new_paths = (after_paths - before_paths) - set(expected.values())
        require(
            not unexpected_new_paths,
            f"Animation import created unexpected assets: {sorted(unexpected_new_paths)}",
        )

    animations = {}
    expected_skeleton_path = package_path(skeleton.get_path_name())
    for name, path in expected.items():
        animation = require(load_existing(path), f"Missing imported animation {path}")
        require(
            isinstance(animation, unreal.AnimSequence),
            f"Expected AnimSequence at {path}",
        )
        require(animation.get_name() == name, f"Unexpected animation object name at {path}")
        animation_skeleton = require(
            animation.get_editor_property("skeleton"),
            f"Imported {name} has no skeleton",
        )
        require(
            package_path(animation_skeleton.get_path_name()) == expected_skeleton_path,
            f"Imported {name} uses an unexpected skeleton",
        )
        require(animation.get_play_length() > 0.0, f"Imported {name} has no duration")

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
    require(
        set(animations) == set(animation_names),
        f"Animation group at {destination} did not resolve exactly its requested clips",
    )
    return animations


def mapping_matches(mapping, action, key_name: str) -> bool:
    mapped_action = mapping.get_editor_property("action")
    mapped_key = mapping.get_editor_property("key")
    if not mapped_action or not mapped_key:
        return False
    return (
        same_asset(mapped_action, action)
        and str(mapped_key.get_editor_property("key_name")) == key_name
    )


def validate_existing_action(action_path: str, description: str):
    action = require(load_existing(action_path), f"Missing existing {description} {action_path}")
    require(
        isinstance(action, unreal.InputAction),
        f"Expected InputAction at {action_path}",
    )
    require(
        action.get_editor_property("value_type")
        == unreal.InputActionValueType.BOOLEAN,
        f"{description} must be a Boolean InputAction",
    )
    return action


def validate_existing_combat_input():
    toggle_combat_action = validate_existing_action(
        TOGGLE_COMBAT_ACTION,
        "toggle-combat action",
    )
    primary_attack_action = validate_existing_action(
        PRIMARY_ATTACK_ACTION,
        "primary-attack action",
    )
    mapping_context = require(
        load_existing(MAPPING_CONTEXT),
        f"Missing existing mapping context {MAPPING_CONTEXT}",
    )
    require(
        isinstance(mapping_context, unreal.InputMappingContext),
        f"Expected InputMappingContext at {MAPPING_CONTEXT}",
    )
    mapping_data = require(
        mapping_context.get_editor_property("default_key_mappings"),
        f"{MAPPING_CONTEXT} has no default key mapping data",
    )
    mappings = list(mapping_data.get_editor_property("mappings") or [])

    for action, key_name, description in (
        (toggle_combat_action, "Z", "toggle combat"),
        (primary_attack_action, "LeftMouseButton", "primary attack"),
    ):
        matches = [
            mapping for mapping in mappings if mapping_matches(mapping, action, key_name)
        ]
        require(
            len(matches) == 1,
            f"Expected exactly one existing {description} mapping on {key_name}; "
            f"found {len(matches)}. This script will not recreate input mappings.",
        )
        mapping = matches[0]
        require(
            not list(mapping.get_editor_property("modifiers") or []),
            f"Existing {description} mapping on {key_name} has modifiers",
        )
        require(
            not list(mapping.get_editor_property("triggers") or []),
            f"Existing {description} mapping on {key_name} has triggers",
        )

    unreal.log(
        f"{LOG_PREFIX} Confirmed existing Z and LeftMouseButton mappings without changes"
    )
    return toggle_combat_action, primary_attack_action


def set_required_property(instance, property_name: str, value, description: str):
    try:
        instance.set_editor_property(property_name, value)
    except Exception as exc:
        raise RuntimeError(
            f"{LOG_PREFIX} Failed to set {description}.{property_name}; "
            "rebuild the native ProjectRequiem module and reopen the Editor"
        ) from exc


def compile_blueprint_no_warnings(blueprint, description: str):
    require(
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint),
        f"Failed to compile {description}; rebuild the native ProjectRequiem "
        "module and reopen the Editor",
    )
    status = blueprint.get_editor_property("status")
    require(
        status == unreal.BlueprintStatus.BS_UP_TO_DATE,
        f"{description} compiled with status {status}; warnings and errors are not allowed",
    )


def get_blueprint_cdo(blueprint, description: str):
    generated_class = require(
        blueprint.generated_class(),
        f"{description} has no generated class",
    )
    return require(
        unreal.get_default_object(generated_class),
        f"{description} has no class default object",
    )


def validate_cdo_asset_property(cdo, property_name: str, expected_asset, description: str):
    actual_asset = require(
        cdo.get_editor_property(property_name),
        f"{description}.{property_name} is empty after compilation",
    )
    require(
        same_asset(actual_asset, expected_asset),
        f"{description}.{property_name} did not retain its configured asset",
    )


def configure_blueprints(
    asset_subsystem,
    animations,
    sword_mesh,
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

    compile_blueprint_no_warnings(
        animation_blueprint,
        "ABP_PR_PlayerLocomotion before sword setup",
    )
    compile_blueprint_no_warnings(
        character_blueprint,
        "BP_CH_Player before sword setup",
    )

    animation_cdo = get_blueprint_cdo(
        animation_blueprint,
        "ABP_PR_PlayerLocomotion",
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

    character_cdo = get_blueprint_cdo(character_blueprint, "BP_CH_Player")
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
    sword_component = require(
        character_cdo.get_editor_property("sword_mesh"),
        "BP_CH_Player CDO.sword_mesh is unavailable; rebuild the native "
        "ProjectRequiem module and reopen the Editor",
    )
    require(
        isinstance(sword_component, unreal.StaticMeshComponent),
        "BP_CH_Player CDO.sword_mesh is not a StaticMeshComponent",
    )
    sword_component.modify()
    set_required_property(
        sword_component,
        "static_mesh",
        sword_mesh,
        "BP_CH_Player CDO.sword_mesh component",
    )

    compile_blueprint_no_warnings(
        animation_blueprint,
        "ABP_PR_PlayerLocomotion after sword setup",
    )
    compile_blueprint_no_warnings(
        character_blueprint,
        "BP_CH_Player after sword setup",
    )
    require(
        asset_subsystem.save_loaded_assets(
            [animation_blueprint, character_blueprint]
        ),
        "Failed to save sword-configured Blueprints",
    )

    # Reacquire both CDOs after compilation and verify that every assignment was
    # serialized instead of trusting the pre-compile object references.
    animation_cdo = get_blueprint_cdo(
        animation_blueprint,
        "ABP_PR_PlayerLocomotion after compilation",
    )
    for animation_name, property_name in ANIMATION_PROPERTIES.items():
        validate_cdo_asset_property(
            animation_cdo,
            property_name,
            animations[animation_name],
            "ABP_PR_PlayerLocomotion CDO",
        )
    require(
        animation_cdo.get_editor_property("root_motion_mode")
        == unreal.RootMotionMode.IGNORE_ROOT_MOTION,
        "ABP_PR_PlayerLocomotion did not retain Ignore Root Motion",
    )

    character_cdo = get_blueprint_cdo(
        character_blueprint,
        "BP_CH_Player after compilation",
    )
    validate_cdo_asset_property(
        character_cdo,
        "toggle_combat_action",
        toggle_combat_action,
        "BP_CH_Player CDO",
    )
    validate_cdo_asset_property(
        character_cdo,
        "primary_attack_action",
        primary_attack_action,
        "BP_CH_Player CDO",
    )
    sword_component = require(
        character_cdo.get_editor_property("sword_mesh"),
        "BP_CH_Player CDO.sword_mesh disappeared after compilation",
    )
    assigned_sword = require(
        sword_component.get_editor_property("static_mesh"),
        "BP_CH_Player sword component has no StaticMesh after compilation",
    )
    require(
        same_asset(assigned_sword, sword_mesh),
        "BP_CH_Player sword component did not retain SM_Sword_Bronze",
    )


def main():
    unreal.log(f"{LOG_PREFIX} Preparing first player sword-combat assets")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)

    for folder in (
        WEAPON_ROOT,
        SWORD_ANIMATION_ROOT,
        SWORD_UAL1_PATH,
        SWORD_UAL2_PATH,
        SWORD_UAL1_RM_PATH,
    ):
        ensure_content_directory(asset_subsystem, folder)

    downloads = Path.home() / "Downloads"
    saved_source_root = (
        Path(unreal.Paths.project_saved_dir())
        / "ImportSources"
        / "PlayerSword"
    )
    weapon_source = prepare_weapon_source(
        saved_source_root / "Weapon",
        downloads / SWORD_ZIP_NAME,
    )
    weapon_assets = import_weapon_assets(
        asset_tools,
        asset_subsystem,
        weapon_source,
    )
    sword_mesh = configure_weapon_assets(asset_subsystem, weapon_assets)

    target_pelvis_translation = read_target_pelvis_translation(
        downloads / BASE_ZIP_NAME
    )
    animation_source_root = saved_source_root / "Animations"
    ual1_source = prepare_in_place_source(
        animation_source_root / "UAL1_Sword.glb",
        downloads / UAL1_ZIP_NAME,
        UAL1_ARCHIVE_PATH,
        UAL1_ANIMATIONS,
        target_pelvis_translation,
    )
    ual2_source = prepare_in_place_source(
        animation_source_root / "UAL2_Sword.glb",
        downloads / UAL2_ZIP_NAME,
        UAL2_ARCHIVE_PATH,
        UAL2_ANIMATIONS,
        target_pelvis_translation,
    )
    heavy_source = prepare_root_motion_source(
        animation_source_root / f"{ROOT_MOTION_FINAL_ANIMATION}.glb",
        downloads / UAL1_ZIP_NAME,
        UAL1_ROOT_MOTION_ARCHIVE_PATH,
        ROOT_MOTION_SOURCE_ANIMATION,
        ROOT_MOTION_FINAL_ANIMATION,
        target_pelvis_translation,
    )

    skeleton = require(load_existing(SKELETON_ASSET), f"Missing {SKELETON_ASSET}")
    animations = import_animation_group(
        asset_tools,
        asset_subsystem,
        ual1_source,
        SWORD_UAL1_PATH,
        UAL1_ANIMATIONS,
        skeleton,
        root_motion=False,
    )
    animations.update(
        import_animation_group(
            asset_tools,
            asset_subsystem,
            ual2_source,
            SWORD_UAL2_PATH,
            UAL2_ANIMATIONS,
            skeleton,
            root_motion=False,
        )
    )
    animations.update(
        import_animation_group(
            asset_tools,
            asset_subsystem,
            heavy_source,
            SWORD_UAL1_RM_PATH,
            (ROOT_MOTION_FINAL_ANIMATION,),
            skeleton,
            root_motion=True,
        )
    )
    require(
        set(animations) == set(ANIMATION_PROPERTIES),
        f"Resolved animation set does not match ABP properties: {sorted(animations)}",
    )

    toggle_combat_action, primary_attack_action = validate_existing_combat_input()
    configure_blueprints(
        asset_subsystem,
        animations,
        sword_mesh,
        toggle_combat_action,
        primary_attack_action,
    )
    unreal.log(
        f"{LOG_PREFIX} Sword combat assets configured: mesh "
        f"{package_path(sword_mesh.get_path_name())}, animations {sorted(animations)}"
    )


if __name__ == "__main__":
    main()
