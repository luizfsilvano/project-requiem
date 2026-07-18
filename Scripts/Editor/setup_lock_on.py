"""Configure the basic Project Requiem lock-on input and ground-ring indicator."""

from __future__ import annotations

import unreal


LOG_PREFIX = "[ProjectRequiem.LockOn]"
PROJECT_ROOT = "/Game/ProjectRequiem"
INPUT_PATH = f"{PROJECT_ROOT}/Core/Input"
ACTION_PATH = f"{INPUT_PATH}/Actions"
LOCK_ON_ACTION = f"{ACTION_PATH}/IA_LockOn"
MAPPING_CONTEXT = f"{INPUT_PATH}/IMC_Exploration"
CHARACTER_BLUEPRINT = (
    f"{PROJECT_ROOT}/Characters/Player/Blueprints/BP_CH_Player.BP_CH_Player"
)
INDICATOR_PATH = f"{PROJECT_ROOT}/UI/HUD/Temporary"
INDICATOR_ASSET = f"{INDICATOR_PATH}/M_LockOnGroundRing.M_LockOnGroundRing"
LEGACY_INDICATOR_ASSET = f"{INDICATOR_PATH}/T_LockOnTarget.T_LockOnTarget"


def require(value, message: str):
    if not value:
        raise RuntimeError(f"{LOG_PREFIX} {message}")
    return value


def load_existing(asset_path: str):
    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return None
    return unreal.load_asset(asset_path)


def create_data_asset(asset_tools, asset_name: str, package_path: str, asset_class):
    object_path = f"{package_path}/{asset_name}.{asset_name}"
    existing = load_existing(object_path)
    if existing:
        return existing

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    return require(
        asset_tools.create_asset(asset_name, package_path, asset_class, factory),
        f"Failed to create {object_path}",
    )


def make_mapping(action, key_name: str):
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


def ensure_unique_clean_mapping(mappings, action, key_name: str):
    result = []
    inserted_clean_mapping = False
    for mapping in mappings:
        if not mapping_matches(mapping, action, key_name):
            result.append(mapping)
            continue
        if not inserted_clean_mapping:
            # Normalize the first matching entry while preserving unrelated
            # player-mapping metadata. All later duplicates are intentionally dropped.
            mapping.set_editor_property("modifiers", [])
            mapping.set_editor_property("triggers", [])
            result.append(mapping)
            inserted_clean_mapping = True

    if not inserted_clean_mapping:
        result.append(make_mapping(action, key_name))
    return result


def configure_input(asset_tools, asset_subsystem):
    action = create_data_asset(
        asset_tools,
        "IA_LockOn",
        ACTION_PATH,
        unreal.InputAction,
    )
    mapping_context = require(
        load_existing(MAPPING_CONTEXT),
        f"Missing existing mapping context {MAPPING_CONTEXT}",
    )

    action.modify()
    mapping_context.modify()
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)

    mapping_data = mapping_context.get_editor_property("default_key_mappings")
    if not mapping_data:
        mapping_data = unreal.InputMappingContextMappingData()
    mappings = list(mapping_data.get_editor_property("mappings") or [])
    mappings = ensure_unique_clean_mapping(mappings, action, "MiddleMouseButton")
    mapping_data.set_editor_property("mappings", mappings)
    mapping_context.set_editor_property("default_key_mappings", mapping_data)

    require(
        asset_subsystem.save_loaded_assets([action, mapping_context]),
        "Failed to save lock-on input assets",
    )
    return action


def create_ground_ring_material(asset_tools, asset_subsystem):
    indicator = load_existing(INDICATOR_ASSET)
    if indicator:
        require(
            isinstance(indicator, unreal.Material),
            f"Expected Material at {INDICATOR_ASSET}",
        )
    else:
        if not asset_subsystem.does_directory_exist(INDICATOR_PATH):
            asset_subsystem.make_directory(INDICATOR_PATH)
        require(
            asset_subsystem.does_directory_exist(INDICATOR_PATH),
            f"Failed to create {INDICATOR_PATH}",
        )
        indicator = require(
            asset_tools.create_asset(
                "M_LockOnGroundRing",
                INDICATOR_PATH,
                unreal.Material,
                unreal.MaterialFactoryNew(),
            ),
            f"Failed to create {INDICATOR_ASSET}",
        )

    indicator.modify()
    indicator.set_editor_properties(
        {
            "material_domain": unreal.MaterialDomain.MD_DEFERRED_DECAL,
            "blend_mode": unreal.BlendMode.BLEND_TRANSLUCENT,
        }
    )

    material_library = unreal.MaterialEditingLibrary
    material_library.delete_all_material_expressions(indicator)

    texture_coordinates = material_library.create_material_expression(
        indicator,
        unreal.MaterialExpressionTextureCoordinate,
        -600,
        0,
    )
    center = material_library.create_material_expression(
        indicator,
        unreal.MaterialExpressionConstant2Vector,
        -600,
        180,
    )
    center.set_editor_properties({"r": 0.5, "g": 0.5})

    outer_mask = material_library.create_material_expression(
        indicator,
        unreal.MaterialExpressionSphereMask,
        -300,
        -60,
    )
    outer_mask.set_editor_properties(
        {"attenuation_radius": 0.48, "hardness_percent": 98.0}
    )
    inner_mask = material_library.create_material_expression(
        indicator,
        unreal.MaterialExpressionSphereMask,
        -300,
        120,
    )
    inner_mask.set_editor_properties(
        {"attenuation_radius": 0.455, "hardness_percent": 98.0}
    )

    ring_mask = material_library.create_material_expression(
        indicator,
        unreal.MaterialExpressionSubtract,
        0,
        20,
    )
    decal_color = material_library.create_material_expression(
        indicator,
        unreal.MaterialExpressionDecalColor,
        0,
        -180,
    )

    connections = [
        material_library.connect_material_expressions(
            texture_coordinates, "", outer_mask, "A"
        ),
        material_library.connect_material_expressions(center, "", outer_mask, "B"),
        material_library.connect_material_expressions(
            texture_coordinates, "", inner_mask, "A"
        ),
        material_library.connect_material_expressions(center, "", inner_mask, "B"),
        material_library.connect_material_expressions(outer_mask, "", ring_mask, "A"),
        material_library.connect_material_expressions(inner_mask, "", ring_mask, "B"),
        material_library.connect_material_property(
            decal_color,
            "",
            unreal.MaterialProperty.MP_BASE_COLOR,
        ),
        material_library.connect_material_property(
            ring_mask,
            "",
            unreal.MaterialProperty.MP_OPACITY,
        ),
    ]
    require(all(connections), "Failed to connect the ground-ring material graph")
    material_library.layout_material_expressions(indicator)
    material_library.recompile_material(indicator)
    require(
        asset_subsystem.save_loaded_asset(indicator),
        f"Failed to save {INDICATOR_ASSET}",
    )
    return indicator


def set_required_property(instance, property_name: str, value, description: str):
    try:
        instance.set_editor_property(property_name, value)
    except Exception as exc:
        raise RuntimeError(
            f"{LOG_PREFIX} Failed to set {description}.{property_name}; "
            "rebuild the native ProjectRequiem Editor target before running this script"
        ) from exc


def configure_character(asset_subsystem, action, indicator_material):
    blueprint = require(
        load_existing(CHARACTER_BLUEPRINT),
        f"Missing {CHARACTER_BLUEPRINT}",
    )
    require(
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint),
        "Failed to compile BP_CH_Player before lock-on setup",
    )
    generated_class = require(
        blueprint.generated_class(),
        "BP_CH_Player has no generated class",
    )
    character_cdo = require(
        unreal.get_default_object(generated_class),
        "BP_CH_Player has no class default object",
    )
    character_cdo.modify()
    set_required_property(
        character_cdo,
        "lock_on_action",
        action,
        "BP_CH_Player CDO",
    )
    try:
        indicator_component = character_cdo.get_editor_property(
            "lock_on_ground_indicator"
        )
    except Exception as exc:
        raise RuntimeError(
            f"{LOG_PREFIX} BP_CH_Player is missing its native LockOnGroundIndicator "
            "component; "
            "rebuild the Editor target and reopen Unreal"
        ) from exc
    require(
        isinstance(indicator_component, unreal.DecalComponent),
        "BP_CH_Player LockOnGroundIndicator is not a DecalComponent",
    )
    indicator_component.modify()
    set_required_property(
        indicator_component,
        "decal_material",
        indicator_material,
        "BP_CH_Player LockOnGroundIndicator",
    )

    require(
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint),
        "Failed to compile BP_CH_Player after lock-on setup",
    )
    require(
        asset_subsystem.save_loaded_assets([blueprint, indicator_material]),
        "Failed to save the lock-on character composition",
    )


def remove_legacy_indicator():
    if not unreal.EditorAssetLibrary.does_asset_exist(LEGACY_INDICATOR_ASSET):
        return
    referencers = unreal.EditorAssetLibrary.find_package_referencers_for_asset(
        LEGACY_INDICATOR_ASSET,
        True,
    )
    require(
        not referencers,
        f"Cannot remove legacy indicator; still referenced by {referencers}",
    )
    require(
        unreal.EditorAssetLibrary.delete_asset(LEGACY_INDICATOR_ASSET),
        f"Failed to remove legacy indicator {LEGACY_INDICATOR_ASSET}",
    )


def main():
    unreal.log(f"{LOG_PREFIX} Configuring lock-on input and presentation")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    action = configure_input(asset_tools, asset_subsystem)
    indicator_material = create_ground_ring_material(asset_tools, asset_subsystem)
    configure_character(asset_subsystem, action, indicator_material)
    remove_legacy_indicator()
    unreal.log(
        f"{LOG_PREFIX} Configured {LOCK_ON_ACTION}, MiddleMouseButton and {INDICATOR_ASSET}"
    )


if __name__ == "__main__":
    main()
