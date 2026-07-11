using System;
using System.Collections.Generic;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem.UI;
using UnityEngine.Rendering.Universal;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

public static class CharacterInventoryUiBootstrap
{
    private const string ScenePath = "Assets/_Project/Scenes/CombatSandbox.unity";
    private const string UiFolder = "Assets/_Project/Prefabs/UI";
    private const string RuntimeUiFolder = "Assets/_Project/UI";
    private const string ItemSlotPrefabPath = UiFolder + "/InventoryItemSlot.prefab";
    private const string ScreenPrefabPath = UiFolder + "/CharacterInventoryScreen.prefab";
    private const string PreviewPrefabPath = UiFolder + "/CharacterPreviewRig.prefab";
    private const string RenderTexturePath = RuntimeUiFolder + "/CharacterPreview.renderTexture";
    private const string PlayerPrefabPath = "Assets/_Project/Prefabs/PlayerPlaceholder.prefab";
    private const string PlayerModelPath = "Assets/_Project/Art/Mixamo/ActionAdventurePack/Y Bot.fbx";
    private const string PlayerControllerPath = "Assets/_Project/Animations/PlayerThirdPerson.controller";
    private const string SwordDataPath = "Assets/_Project/Data/Weapons/BronzeSword.asset";
    private const string AxeDataPath = "Assets/_Project/Data/Weapons/BronzeAxe.asset";
    private const string PreviewLayerName = "CharacterPreview";
    private const int PreferredPreviewLayer = 30;

    private static readonly Color ScreenColor = new(0.018f, 0.018f, 0.021f, 1f);
    private static readonly Color PanelColor = new(0.045f, 0.042f, 0.039f, 0.98f);
    private static readonly Color PanelRaisedColor = new(0.07f, 0.062f, 0.054f, 0.98f);
    private static readonly Color BronzeColor = new(0.58f, 0.4f, 0.19f, 1f);
    private static readonly Color BronzeMutedColor = new(0.28f, 0.22f, 0.15f, 1f);
    private static readonly Color IvoryColor = new(0.9f, 0.86f, 0.76f, 1f);
    private static readonly Color MutedTextColor = new(0.57f, 0.55f, 0.5f, 1f);
    private static readonly Color DangerColor = new(0.38f, 0.09f, 0.075f, 1f);

    private static Font Font => Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
    private static Sprite UiSprite => AssetDatabase.GetBuiltinExtraResource<Sprite>("UI/Skin/UISprite.psd");

    [MenuItem("Tools/Combat Sandbox/Build Character Inventory UI")]
    public static void Build()
    {
        EnsureFolder("Assets/_Project", "Prefabs");
        EnsureFolder("Assets/_Project/Prefabs", "UI");
        EnsureFolder("Assets/_Project", "UI");

        int previewLayer = EnsurePreviewLayer();
        RenderTexture renderTexture = BuildRenderTexture();
        InventoryItemSlotView itemSlotPrefab = BuildInventoryItemSlotPrefab();
        GameObject previewPrefab = BuildPreviewRigPrefab(previewLayer, renderTexture);
        GameObject screenPrefab = BuildScreenPrefab(renderTexture, itemSlotPrefab);

        UpdateWeaponDefinitions();
        UpdatePlayerPrefabSlots();
        IntegrateScene(screenPrefab, previewPrefab, previewLayer);

        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();
        Debug.Log("Character inventory UI, preview rig, item metadata, prefabs and CombatSandbox integration rebuilt.");
    }

    private static RenderTexture BuildRenderTexture()
    {
        RenderTexture texture = AssetDatabase.LoadAssetAtPath<RenderTexture>(RenderTexturePath);
        if (texture == null)
        {
            texture = new RenderTexture(640, 900, 24, RenderTextureFormat.ARGB32)
            {
                name = "CharacterPreview",
                antiAliasing = 2,
                filterMode = FilterMode.Bilinear,
                wrapMode = TextureWrapMode.Clamp,
                useMipMap = false,
                autoGenerateMips = false
            };
            AssetDatabase.CreateAsset(texture, RenderTexturePath);
        }
        else
        {
            texture.Release();
            texture.width = 640;
            texture.height = 900;
            texture.depth = 24;
            texture.format = RenderTextureFormat.ARGB32;
            texture.antiAliasing = 2;
            texture.filterMode = FilterMode.Bilinear;
            texture.wrapMode = TextureWrapMode.Clamp;
            texture.useMipMap = false;
            texture.autoGenerateMips = false;
            EditorUtility.SetDirty(texture);
        }

        return texture;
    }

    private static InventoryItemSlotView BuildInventoryItemSlotPrefab()
    {
        GameObject root = CreateRectObject("InventoryItemSlot", null);
        RectTransform rootRect = root.GetComponent<RectTransform>();
        rootRect.sizeDelta = new Vector2(82f, 82f);

        LayoutElement layout = root.AddComponent<LayoutElement>();
        layout.preferredWidth = 82f;
        layout.preferredHeight = 82f;
        root.AddComponent<CanvasGroup>();
        Image background = root.AddComponent<Image>();
        background.sprite = UiSprite;
        background.type = Image.Type.Sliced;
        background.color = new Color(0.095f, 0.085f, 0.075f, 0.98f);

        Image border = CreateImage("RarityBorder", root.transform, Stretch(), Color.white, UiSprite, true);
        border.raycastTarget = false;

        Image icon = CreateImage(
            "Icon",
            root.transform,
            new RectSpec(new Vector2(0.18f, 0.2f), new Vector2(0.82f, 0.84f), Vector2.zero, Vector2.zero),
            Color.white);
        icon.preserveAspect = true;
        icon.raycastTarget = false;

        Text fallback = CreateText(
            "IconFallback",
            root.transform,
            new RectSpec(new Vector2(0.12f, 0.16f), new Vector2(0.88f, 0.88f), Vector2.zero, Vector2.zero),
            "SW",
            27,
            TextAnchor.MiddleCenter,
            new Color(0.82f, 0.68f, 0.4f, 1f),
            FontStyle.Bold);

        Text rarity = CreateText(
            "Rarity",
            root.transform,
            new RectSpec(new Vector2(0.04f, 0.77f), new Vector2(0.62f, 0.98f), Vector2.zero, Vector2.zero),
            "COMMON",
            9,
            TextAnchor.UpperLeft,
            MutedTextColor,
            FontStyle.Bold);

        Text quality = CreateText(
            "Quality",
            root.transform,
            new RectSpec(new Vector2(0.45f, 0.77f), new Vector2(0.96f, 0.98f), Vector2.zero, Vector2.zero),
            "STANDARD",
            9,
            TextAnchor.UpperRight,
            IvoryColor);

        Text quantity = CreateText(
            "Quantity",
            root.transform,
            new RectSpec(new Vector2(0.55f, 0.04f), new Vector2(0.96f, 0.28f), Vector2.zero, Vector2.zero),
            "x1",
            12,
            TextAnchor.LowerRight,
            IvoryColor,
            FontStyle.Bold);

        Text equipped = CreateText(
            "EquippedBadge",
            root.transform,
            new RectSpec(new Vector2(0.04f, 0.08f), new Vector2(0.66f, 0.28f), Vector2.zero, Vector2.zero),
            "EQUIPPED",
            9,
            TextAnchor.LowerLeft,
            new Color(0.98f, 0.78f, 0.3f, 1f),
            FontStyle.Bold);

        Image durabilityTrack = CreateImage(
            "DurabilityTrack",
            root.transform,
            new RectSpec(new Vector2(0.05f, 0.025f), new Vector2(0.95f, 0.075f), Vector2.zero, Vector2.zero),
            new Color(0.04f, 0.035f, 0.03f, 1f));
        durabilityTrack.raycastTarget = false;
        Image durabilityFill = CreateImage("DurabilityFill", durabilityTrack.transform, Stretch(), BronzeColor);
        durabilityFill.type = Image.Type.Filled;
        durabilityFill.fillMethod = Image.FillMethod.Horizontal;
        durabilityFill.fillOrigin = 0;
        durabilityFill.fillAmount = 1f;
        durabilityFill.raycastTarget = false;

        Image selection = CreateImage("SelectionFrame", root.transform, Stretch(), new Color(0.95f, 0.72f, 0.28f, 0.8f), UiSprite, true);
        selection.raycastTarget = false;

        InventoryItemSlotView view = root.AddComponent<InventoryItemSlotView>();
        SetSerializedObject(view, "background", background);
        SetSerializedObject(view, "rarityBorder", border);
        SetSerializedObject(view, "icon", icon);
        SetSerializedObject(view, "iconFallback", fallback);
        SetSerializedObject(view, "quantityText", quantity);
        SetSerializedObject(view, "rarityText", rarity);
        SetSerializedObject(view, "qualityText", quality);
        SetSerializedObject(view, "equippedText", equipped);
        SetSerializedObject(view, "durabilityTrack", durabilityTrack);
        SetSerializedObject(view, "durabilityFill", durabilityFill);
        SetSerializedObject(view, "selectionFrame", selection.gameObject);

        GameObject prefab = PrefabUtility.SaveAsPrefabAsset(root, ItemSlotPrefabPath);
        UnityEngine.Object.DestroyImmediate(root);
        return prefab.GetComponent<InventoryItemSlotView>();
    }

    private static GameObject BuildPreviewRigPrefab(int previewLayer, RenderTexture renderTexture)
    {
        GameObject root = new("CharacterPreviewRig");
        root.layer = previewLayer;
        CharacterPreviewController controller = root.AddComponent<CharacterPreviewController>();

        GameObject pivotObject = new("CharacterPivot");
        pivotObject.transform.SetParent(root.transform, false);
        pivotObject.layer = previewLayer;

        GameObject playerModel = AssetDatabase.LoadAssetAtPath<GameObject>(PlayerModelPath);
        if (playerModel == null)
        {
            throw new InvalidOperationException($"Player model was not found at {PlayerModelPath}.");
        }

        GameObject body = (GameObject)PrefabUtility.InstantiatePrefab(playerModel);
        body.name = "PreviewBody";
        body.transform.SetParent(pivotObject.transform, false);
        body.transform.localPosition = new Vector3(0f, -0.08f, 0f);
        body.transform.localRotation = Quaternion.identity;
        body.transform.localScale = Vector3.one * 1.12f;
        Animator animator = body.GetComponent<Animator>();
        if (animator == null)
        {
            throw new InvalidOperationException("The Y Bot preview model has no Animator.");
        }

        animator.runtimeAnimatorController = AssetDatabase.LoadAssetAtPath<RuntimeAnimatorController>(PlayerControllerPath);
        animator.applyRootMotion = false;

        GameObject cameraObject = new("PreviewCamera");
        cameraObject.transform.SetParent(root.transform, false);
        cameraObject.transform.localPosition = new Vector3(0f, 1.08f, 4.35f);
        cameraObject.transform.localRotation = Quaternion.Euler(0f, 180f, 0f);
        cameraObject.layer = previewLayer;
        Camera previewCamera = cameraObject.AddComponent<Camera>();
        previewCamera.clearFlags = CameraClearFlags.SolidColor;
        previewCamera.backgroundColor = new Color(0.025f, 0.025f, 0.03f, 1f);
        previewCamera.cullingMask = 1 << previewLayer;
        previewCamera.nearClipPlane = 0.1f;
        previewCamera.farClipPlane = 20f;
        previewCamera.fieldOfView = 28f;
        previewCamera.allowHDR = true;
        previewCamera.allowMSAA = true;
        previewCamera.targetTexture = renderTexture;
        previewCamera.enabled = false;
        UniversalAdditionalCameraData cameraData = cameraObject.AddComponent<UniversalAdditionalCameraData>();
        cameraData.renderPostProcessing = false;
        cameraData.renderShadows = true;

        CreatePreviewLight(root.transform, "KeyLight", new Vector3(32f, 205f, 8f), new Color(1f, 0.85f, 0.65f), 1.15f, previewLayer);
        CreatePreviewLight(root.transform, "FillLight", new Vector3(18f, 25f, 0f), new Color(0.42f, 0.52f, 0.68f), 0.52f, previewLayer);
        CreatePreviewLight(root.transform, "RimLight", new Vector3(12f, 150f, 0f), new Color(0.88f, 0.48f, 0.2f), 0.7f, previewLayer);

        SetLayerRecursively(root, previewLayer);
        SetSerializedObject(controller, "previewCamera", previewCamera);
        SetSerializedObject(controller, "characterPivot", pivotObject.transform);
        SetSerializedObject(controller, "previewAnimator", animator);
        SetSerializedObject(controller, "renderTexture", renderTexture);
        SetSerializedInt(controller, "previewLayer", previewLayer);
        SetSerializedFloat(controller, "cameraHeight", 1.08f);
        SetSerializedFloat(controller, "minCameraDistance", 3.25f);
        SetSerializedFloat(controller, "maxCameraDistance", 5.5f);
        SetSerializedFloat(controller, "cameraDistance", 4.35f);

        GameObject prefab = PrefabUtility.SaveAsPrefabAsset(root, PreviewPrefabPath);
        UnityEngine.Object.DestroyImmediate(root);
        return prefab;
    }

    private static GameObject BuildScreenPrefab(RenderTexture renderTexture, InventoryItemSlotView itemSlotPrefab)
    {
        GameObject root = CreateRectObject("CharacterInventoryScreen", null);
        Canvas canvas = root.AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 40;
        CanvasScaler scaler = root.AddComponent<CanvasScaler>();
        scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
        scaler.referenceResolution = new Vector2(1920f, 1080f);
        scaler.screenMatchMode = CanvasScaler.ScreenMatchMode.MatchWidthOrHeight;
        scaler.matchWidthOrHeight = 0.5f;
        root.AddComponent<GraphicRaycaster>();
        CharacterInventoryScreen screen = root.AddComponent<CharacterInventoryScreen>();

        GameObject screenRoot = CreateRectObject("ScreenRoot", root.transform);
        SetRect(screenRoot.GetComponent<RectTransform>(), Stretch());
        Image overlay = screenRoot.AddComponent<Image>();
        overlay.color = ScreenColor;
        overlay.raycastTarget = true;

        CreateImage(
            "TopRule",
            screenRoot.transform,
            new RectSpec(new Vector2(0.02f, 0.925f), new Vector2(0.98f, 0.929f), Vector2.zero, Vector2.zero),
            BronzeColor).raycastTarget = false;

        CreateText(
            "Title",
            screenRoot.transform,
            new RectSpec(new Vector2(0.025f, 0.937f), new Vector2(0.55f, 0.99f), Vector2.zero, Vector2.zero),
            "CHARACTER  /  INVENTORY",
            30,
            TextAnchor.MiddleLeft,
            IvoryColor,
            FontStyle.Bold);
        CreateText(
            "Subtitle",
            screenRoot.transform,
            new RectSpec(new Vector2(0.47f, 0.945f), new Vector2(0.88f, 0.982f), Vector2.zero, Vector2.zero),
            "LOADOUT • POSSESSIONS • EQUIPMENT",
            12,
            TextAnchor.MiddleRight,
            MutedTextColor);
        Button closeButton = CreateButton(
            "CloseButton",
            screenRoot.transform,
            new RectSpec(new Vector2(0.9f, 0.94f), new Vector2(0.975f, 0.985f), Vector2.zero, Vector2.zero),
            "CLOSE  [ I ]",
            PanelRaisedColor,
            BronzeColor,
            12);

        GameObject columns = CreateRectObject("Columns", screenRoot.transform);
        SetRect(columns.GetComponent<RectTransform>(), new RectSpec(new Vector2(0.02f, 0.265f), new Vector2(0.98f, 0.915f), Vector2.zero, Vector2.zero));
        HorizontalLayoutGroup columnsLayout = columns.AddComponent<HorizontalLayoutGroup>();
        columnsLayout.spacing = 16f;
        columnsLayout.childAlignment = TextAnchor.UpperLeft;
        columnsLayout.childControlWidth = true;
        columnsLayout.childControlHeight = true;
        columnsLayout.childForceExpandWidth = false;
        columnsLayout.childForceExpandHeight = true;

        List<InventoryFilterTabView> filterViews = new();
        Transform inventoryGrid = BuildInventoryPanel(columns.transform, filterViews);
        BuildPreviewPanel(columns.transform, renderTexture);
        List<EquipmentSlotView> equipmentViews = BuildEquipmentPanel(columns.transform);

        DetailsReferences details = BuildDetailsPanel(screenRoot.transform);
        GameObject ghostObject = BuildDragGhost(screenRoot.transform, out Image ghostBackground, out Text ghostLabel);

        SetSerializedObject(screen, "screenRoot", screenRoot);
        SetSerializedObject(screen, "inventoryGrid", inventoryGrid);
        SetSerializedObject(screen, "itemSlotPrefab", itemSlotPrefab);
        SetSerializedInt(screen, "minimumGridSlots", 20);
        SetSerializedArray(screen, "filterTabs", filterViews);
        SetSerializedArray(screen, "equipmentSlotViews", equipmentViews);
        SetSerializedObject(screen, "detailName", details.Name);
        SetSerializedObject(screen, "detailDescription", details.Description);
        SetSerializedObject(screen, "detailType", details.Type);
        SetSerializedObject(screen, "detailRarity", details.Rarity);
        SetSerializedObject(screen, "detailQuantity", details.Quantity);
        SetSerializedObject(screen, "detailDurability", details.Durability);
        SetSerializedObject(screen, "detailDamage", details.Damage);
        SetSerializedObject(screen, "detailMetadata", details.Metadata);
        SetSerializedObject(screen, "feedbackText", details.Feedback);
        SetSerializedObject(screen, "equipButton", details.EquipButton);
        SetSerializedObject(screen, "unequipButton", details.UnequipButton);
        SetSerializedObject(screen, "activateButton", details.ActivateButton);
        SetSerializedObject(screen, "discardButton", details.DiscardButton);
        SetSerializedObject(screen, "closeButton", closeButton);
        SetSerializedObject(screen, "dragGhost", ghostObject.GetComponent<RectTransform>());
        SetSerializedObject(screen, "dragGhostBackground", ghostBackground);
        SetSerializedObject(screen, "dragGhostLabel", ghostLabel);

        ghostObject.SetActive(false);
        screenRoot.SetActive(false);
        GameObject prefab = PrefabUtility.SaveAsPrefabAsset(root, ScreenPrefabPath);
        UnityEngine.Object.DestroyImmediate(root);
        return prefab;
    }

    private static Transform BuildInventoryPanel(Transform parent, List<InventoryFilterTabView> filterViews)
    {
        GameObject panel = CreatePanel("InventoryPanel", parent, 470f, 405f, 0f);
        CreateText(
            "PanelTitle",
            panel.transform,
            new RectSpec(new Vector2(0.045f, 0.91f), new Vector2(0.955f, 0.985f), Vector2.zero, Vector2.zero),
            "POSSESSIONS",
            20,
            TextAnchor.MiddleLeft,
            IvoryColor,
            FontStyle.Bold);
        CreateText(
            "PanelHint",
            panel.transform,
            new RectSpec(new Vector2(0.5f, 0.92f), new Vector2(0.95f, 0.975f), Vector2.zero, Vector2.zero),
            "CLICK TO INSPECT",
            9,
            TextAnchor.MiddleRight,
            MutedTextColor);

        GameObject tabGrid = CreateRectObject("FilterTabs", panel.transform);
        SetRect(tabGrid.GetComponent<RectTransform>(), new RectSpec(new Vector2(0.045f, 0.775f), new Vector2(0.955f, 0.905f), Vector2.zero, Vector2.zero));
        GridLayoutGroup tabLayout = tabGrid.AddComponent<GridLayoutGroup>();
        tabLayout.cellSize = new Vector2(100f, 32f);
        tabLayout.spacing = new Vector2(6f, 6f);
        tabLayout.constraint = GridLayoutGroup.Constraint.FixedColumnCount;
        tabLayout.constraintCount = 4;
        tabLayout.childAlignment = TextAnchor.UpperLeft;

        string[] labels = { "ALL", "WEAPONS", "ARMOR", "CONSUMABLES", "MATERIALS", "TOOLS", "OTHER" };
        foreach (InventoryFilterType filterType in Enum.GetValues(typeof(InventoryFilterType)))
        {
            GameObject tabObject = CreateRectObject(filterType.ToString(), tabGrid.transform);
            Image tabImage = tabObject.AddComponent<Image>();
            tabImage.sprite = UiSprite;
            tabImage.type = Image.Type.Sliced;
            tabImage.color = PanelRaisedColor;
            Button button = tabObject.AddComponent<Button>();
            button.targetGraphic = tabImage;
            Text label = CreateText("Label", tabObject.transform, Stretch(), labels[(int)filterType], 11, TextAnchor.MiddleCenter, MutedTextColor, FontStyle.Bold);
            InventoryFilterTabView tabView = tabObject.AddComponent<InventoryFilterTabView>();
            SetSerializedEnum(tabView, "filterType", (int)filterType);
            SetSerializedObject(tabView, "background", tabImage);
            SetSerializedObject(tabView, "label", label);
            filterViews.Add(tabView);
        }

        GameObject viewport = CreateRectObject("InventoryViewport", panel.transform);
        SetRect(viewport.GetComponent<RectTransform>(), new RectSpec(new Vector2(0.045f, 0.045f), new Vector2(0.955f, 0.75f), Vector2.zero, Vector2.zero));
        Image viewportImage = viewport.AddComponent<Image>();
        viewportImage.color = new Color(0.027f, 0.026f, 0.025f, 0.98f);
        viewportImage.raycastTarget = true;
        Mask mask = viewport.AddComponent<Mask>();
        mask.showMaskGraphic = true;
        viewport.AddComponent<InventoryDropTarget>();

        GameObject content = CreateRectObject("ItemGrid", viewport.transform);
        RectTransform contentRect = content.GetComponent<RectTransform>();
        contentRect.anchorMin = new Vector2(0f, 1f);
        contentRect.anchorMax = new Vector2(1f, 1f);
        contentRect.pivot = new Vector2(0.5f, 1f);
        contentRect.anchoredPosition = new Vector2(0f, -10f);
        contentRect.sizeDelta = new Vector2(-20f, 0f);
        GridLayoutGroup grid = content.AddComponent<GridLayoutGroup>();
        grid.cellSize = new Vector2(75f, 75f);
        grid.spacing = new Vector2(7f, 7f);
        grid.padding = new RectOffset(4, 4, 4, 10);
        grid.constraint = GridLayoutGroup.Constraint.FixedColumnCount;
        grid.constraintCount = 5;
        grid.childAlignment = TextAnchor.UpperLeft;
        ContentSizeFitter fitter = content.AddComponent<ContentSizeFitter>();
        fitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;
        content.AddComponent<InventoryDropTarget>();

        ScrollRect scroll = viewport.AddComponent<ScrollRect>();
        scroll.content = contentRect;
        scroll.viewport = viewport.GetComponent<RectTransform>();
        scroll.horizontal = false;
        scroll.vertical = true;
        scroll.movementType = ScrollRect.MovementType.Clamped;
        scroll.scrollSensitivity = 24f;
        return content.transform;
    }

    private static void BuildPreviewPanel(Transform parent, RenderTexture renderTexture)
    {
        GameObject panel = CreatePanel("CharacterPreviewPanel", parent, 500f, 440f, 1f);
        LayoutElement layout = panel.GetComponent<LayoutElement>();
        layout.flexibleWidth = 1f;

        CreateText(
            "PanelTitle",
            panel.transform,
            new RectSpec(new Vector2(0.05f, 0.91f), new Vector2(0.95f, 0.985f), Vector2.zero, Vector2.zero),
            "CHARACTER",
            20,
            TextAnchor.MiddleCenter,
            IvoryColor,
            FontStyle.Bold);
        CreateImage(
            "TitleRule",
            panel.transform,
            new RectSpec(new Vector2(0.2f, 0.905f), new Vector2(0.8f, 0.909f), Vector2.zero, Vector2.zero),
            BronzeMutedColor).raycastTarget = false;

        GameObject rawObject = CreateRectObject("PreviewViewport", panel.transform);
        SetRect(rawObject.GetComponent<RectTransform>(), new RectSpec(new Vector2(0.05f, 0.08f), new Vector2(0.95f, 0.89f), Vector2.zero, Vector2.zero));
        RawImage rawImage = rawObject.AddComponent<RawImage>();
        rawImage.texture = renderTexture;
        rawImage.color = Color.white;
        AspectRatioFitter aspect = rawObject.AddComponent<AspectRatioFitter>();
        aspect.aspectMode = AspectRatioFitter.AspectMode.FitInParent;
        aspect.aspectRatio = 640f / 900f;
        rawObject.AddComponent<CharacterPreviewDragArea>();

        CreateText(
            "PreviewInstruction",
            panel.transform,
            new RectSpec(new Vector2(0.08f, 0.015f), new Vector2(0.92f, 0.075f), Vector2.zero, Vector2.zero),
            "DRAG TO ROTATE   •   WHEEL TO ZOOM",
            11,
            TextAnchor.MiddleCenter,
            MutedTextColor);
    }

    private static List<EquipmentSlotView> BuildEquipmentPanel(Transform parent)
    {
        GameObject panel = CreatePanel("EquipmentPanel", parent, 450f, 390f, 0f);
        CreateText(
            "PanelTitle",
            panel.transform,
            new RectSpec(new Vector2(0.045f, 0.91f), new Vector2(0.955f, 0.985f), Vector2.zero, Vector2.zero),
            "EQUIPMENT",
            20,
            TextAnchor.MiddleLeft,
            IvoryColor,
            FontStyle.Bold);
        CreateText(
            "PanelHint",
            panel.transform,
            new RectSpec(new Vector2(0.5f, 0.92f), new Vector2(0.95f, 0.975f), Vector2.zero, Vector2.zero),
            "ACTIVE LOADOUT",
            9,
            TextAnchor.MiddleRight,
            MutedTextColor);

        GameObject content = CreateRectObject("EquipmentGroups", panel.transform);
        SetRect(content.GetComponent<RectTransform>(), new RectSpec(new Vector2(0.045f, 0.035f), new Vector2(0.955f, 0.9f), Vector2.zero, Vector2.zero));
        VerticalLayoutGroup vertical = content.AddComponent<VerticalLayoutGroup>();
        vertical.spacing = 4f;
        vertical.childAlignment = TextAnchor.UpperLeft;
        vertical.childControlWidth = true;
        vertical.childControlHeight = true;
        vertical.childForceExpandWidth = true;
        vertical.childForceExpandHeight = false;

        List<EquipmentSlotView> views = new();
        AddEquipmentGroup(content.transform, "BODY", new[]
        {
            EquipmentSlotType.Head,
            EquipmentSlotType.Chest,
            EquipmentSlotType.Hands,
            EquipmentSlotType.Legs,
            EquipmentSlotType.Feet
        }, views);
        AddEquipmentGroup(content.transform, "WEAPONS", new[]
        {
            EquipmentSlotType.MainHand,
            EquipmentSlotType.OffHand
        }, views);
        AddEquipmentGroup(content.transform, "ACCESSORIES", new[]
        {
            EquipmentSlotType.Accessory1,
            EquipmentSlotType.Accessory2
        }, views);
        AddEquipmentGroup(content.transform, "TOOLS", new[]
        {
            EquipmentSlotType.AxeTool,
            EquipmentSlotType.PickaxeTool,
            EquipmentSlotType.KnifeTool
        }, views);
        return views;
    }

    private static void AddEquipmentGroup(
        Transform parent,
        string title,
        EquipmentSlotType[] slotTypes,
        List<EquipmentSlotView> views)
    {
        Text groupTitle = CreateText(title + "Title", parent, Stretch(), title, 12, TextAnchor.MiddleLeft, BronzeColor, FontStyle.Bold);
        LayoutElement titleLayout = groupTitle.gameObject.AddComponent<LayoutElement>();
        titleLayout.preferredHeight = 20f;

        GameObject gridObject = CreateRectObject(title + "Grid", parent);
        GridLayoutGroup grid = gridObject.AddComponent<GridLayoutGroup>();
        grid.cellSize = new Vector2(194f, 50f);
        grid.spacing = new Vector2(8f, 6f);
        grid.constraint = GridLayoutGroup.Constraint.FixedColumnCount;
        grid.constraintCount = 2;
        grid.childAlignment = TextAnchor.UpperLeft;
        int rows = Mathf.CeilToInt(slotTypes.Length / 2f);
        LayoutElement gridLayout = gridObject.AddComponent<LayoutElement>();
        gridLayout.preferredHeight = rows * 50f + Mathf.Max(0, rows - 1) * 6f;

        foreach (EquipmentSlotType slotType in slotTypes)
        {
            views.Add(CreateEquipmentSlot(gridObject.transform, slotType));
        }
    }

    private static EquipmentSlotView CreateEquipmentSlot(Transform parent, EquipmentSlotType slotType)
    {
        GameObject root = CreateRectObject(slotType.ToString(), parent);
        root.AddComponent<CanvasGroup>();
        Image background = root.AddComponent<Image>();
        background.sprite = UiSprite;
        background.type = Image.Type.Sliced;
        background.color = new Color(0.055f, 0.052f, 0.05f, 0.9f);
        Image border = CreateImage("Border", root.transform, Stretch(), BronzeMutedColor, UiSprite, true);
        border.raycastTarget = false;

        Image icon = CreateImage(
            "Icon",
            root.transform,
            new RectSpec(new Vector2(0.025f, 0.12f), new Vector2(0.25f, 0.88f), Vector2.zero, Vector2.zero),
            Color.white);
        icon.preserveAspect = true;
        icon.raycastTarget = false;
        Text fallback = CreateText(
            "IconFallback",
            root.transform,
            new RectSpec(new Vector2(0.025f, 0.12f), new Vector2(0.25f, 0.88f), Vector2.zero, Vector2.zero),
            CharacterInventoryScreen.GetSlotMonogram(slotType),
            15,
            TextAnchor.MiddleCenter,
            MutedTextColor,
            FontStyle.Bold);
        Text slotLabel = CreateText(
            "SlotLabel",
            root.transform,
            new RectSpec(new Vector2(0.29f, 0.5f), new Vector2(0.96f, 0.92f), Vector2.zero, Vector2.zero),
            CharacterInventoryScreen.GetSlotDisplayName(slotType).ToUpperInvariant(),
            11,
            TextAnchor.MiddleLeft,
            BronzeColor,
            FontStyle.Bold);
        Text itemLabel = CreateText(
            "ItemLabel",
            root.transform,
            new RectSpec(new Vector2(0.29f, 0.08f), new Vector2(0.96f, 0.54f), Vector2.zero, Vector2.zero),
            "Empty",
            13,
            TextAnchor.MiddleLeft,
            MutedTextColor);
        Text active = CreateText(
            "ActiveBadge",
            root.transform,
            new RectSpec(new Vector2(0.68f, 0.58f), new Vector2(0.96f, 0.92f), Vector2.zero, Vector2.zero),
            "ACTIVE",
            9,
            TextAnchor.MiddleRight,
            new Color(0.98f, 0.78f, 0.3f, 1f),
            FontStyle.Bold);
        Image selection = CreateImage("SelectionFrame", root.transform, Stretch(), new Color(0.95f, 0.72f, 0.28f, 0.72f), UiSprite, true);
        selection.raycastTarget = false;
        Image highlight = CreateImage("DragHighlight", root.transform, Stretch(), new Color(0.55f, 0.42f, 0.16f, 0.58f), UiSprite, true);
        highlight.raycastTarget = false;
        highlight.gameObject.SetActive(false);

        EquipmentSlotView view = root.AddComponent<EquipmentSlotView>();
        SetSerializedEnum(view, "slotType", (int)slotType);
        SetSerializedObject(view, "background", background);
        SetSerializedObject(view, "border", border);
        SetSerializedObject(view, "icon", icon);
        SetSerializedObject(view, "iconFallback", fallback);
        SetSerializedObject(view, "slotLabel", slotLabel);
        SetSerializedObject(view, "itemLabel", itemLabel);
        SetSerializedObject(view, "activeBadge", active);
        SetSerializedObject(view, "selectionFrame", selection.gameObject);
        SetSerializedObject(view, "dragHighlight", highlight);
        return view;
    }

    private static DetailsReferences BuildDetailsPanel(Transform parent)
    {
        GameObject panel = CreateRectObject("ItemDetailsPanel", parent);
        SetRect(panel.GetComponent<RectTransform>(), new RectSpec(new Vector2(0.02f, 0.025f), new Vector2(0.98f, 0.245f), Vector2.zero, Vector2.zero));
        Image background = panel.AddComponent<Image>();
        background.sprite = UiSprite;
        background.type = Image.Type.Sliced;
        background.color = PanelColor;
        Outline outline = panel.AddComponent<Outline>();
        outline.effectColor = BronzeMutedColor;
        outline.effectDistance = new Vector2(1.5f, -1.5f);

        DetailsReferences refs = new();
        refs.Name = CreateText(
            "ItemName",
            panel.transform,
            new RectSpec(new Vector2(0.025f, 0.68f), new Vector2(0.42f, 0.93f), Vector2.zero, Vector2.zero),
            "Select an item",
            24,
            TextAnchor.MiddleLeft,
            IvoryColor,
            FontStyle.Bold);
        refs.Description = CreateText(
            "Description",
            panel.transform,
            new RectSpec(new Vector2(0.025f, 0.2f), new Vector2(0.42f, 0.69f), Vector2.zero, Vector2.zero),
            "Choose an item to inspect it.",
            15,
            TextAnchor.UpperLeft,
            MutedTextColor);
        refs.Description.horizontalOverflow = HorizontalWrapMode.Wrap;
        refs.Description.verticalOverflow = VerticalWrapMode.Truncate;

        GameObject stats = CreateRectObject("Stats", panel.transform);
        SetRect(stats.GetComponent<RectTransform>(), new RectSpec(new Vector2(0.445f, 0.15f), new Vector2(0.72f, 0.9f), Vector2.zero, Vector2.zero));
        VerticalLayoutGroup statsLayout = stats.AddComponent<VerticalLayoutGroup>();
        statsLayout.spacing = 2f;
        statsLayout.childControlWidth = true;
        statsLayout.childControlHeight = true;
        statsLayout.childForceExpandWidth = true;
        statsLayout.childForceExpandHeight = false;
        refs.Type = CreateStatText("Type", stats.transform, "TYPE  —");
        refs.Rarity = CreateStatText("Rarity", stats.transform, "RARITY  —    QUALITY  —");
        refs.Quantity = CreateStatText("Quantity", stats.transform, "QUANTITY  —");
        refs.Durability = CreateStatText("Durability", stats.transform, "DURABILITY  —");
        refs.Damage = CreateStatText("Damage", stats.transform, string.Empty);
        refs.Metadata = CreateStatText("Metadata", stats.transform, string.Empty);

        GameObject actions = CreateRectObject("Actions", panel.transform);
        SetRect(actions.GetComponent<RectTransform>(), new RectSpec(new Vector2(0.745f, 0.23f), new Vector2(0.975f, 0.88f), Vector2.zero, Vector2.zero));
        GridLayoutGroup actionGrid = actions.AddComponent<GridLayoutGroup>();
        actionGrid.cellSize = new Vector2(185f, 43f);
        actionGrid.spacing = new Vector2(9f, 9f);
        actionGrid.constraint = GridLayoutGroup.Constraint.FixedColumnCount;
        actionGrid.constraintCount = 2;
        actionGrid.childAlignment = TextAnchor.UpperRight;
        refs.EquipButton = CreateButton("Equip", actions.transform, Stretch(), "EQUIP", PanelRaisedColor, BronzeColor, 12);
        refs.ActivateButton = CreateButton("Activate", actions.transform, Stretch(), "MAKE ACTIVE", PanelRaisedColor, BronzeColor, 12);
        refs.UnequipButton = CreateButton("Unequip", actions.transform, Stretch(), "UNEQUIP", PanelRaisedColor, BronzeColor, 12);
        refs.DiscardButton = CreateButton("Discard", actions.transform, Stretch(), "DISCARD", DangerColor, new Color(0.65f, 0.18f, 0.14f, 1f), 12);

        refs.Feedback = CreateText(
            "Feedback",
            panel.transform,
            new RectSpec(new Vector2(0.445f, 0.02f), new Vector2(0.975f, 0.17f), Vector2.zero, Vector2.zero),
            "Drag items to move them safely between inventory and compatible equipment slots.",
            12,
            TextAnchor.MiddleRight,
            new Color(0.74f, 0.61f, 0.36f, 1f));
        return refs;
    }

    private static GameObject BuildDragGhost(Transform parent, out Image background, out Text label)
    {
        GameObject ghost = CreateRectObject("DragGhost", parent);
        RectTransform rect = ghost.GetComponent<RectTransform>();
        rect.anchorMin = new Vector2(0.5f, 0.5f);
        rect.anchorMax = new Vector2(0.5f, 0.5f);
        rect.pivot = new Vector2(0.5f, 0.5f);
        rect.sizeDelta = new Vector2(68f, 68f);
        background = ghost.AddComponent<Image>();
        background.sprite = UiSprite;
        background.type = Image.Type.Sliced;
        background.color = BronzeColor;
        background.raycastTarget = false;
        CanvasGroup group = ghost.AddComponent<CanvasGroup>();
        group.blocksRaycasts = false;
        group.alpha = 0.9f;
        label = CreateText("Label", ghost.transform, Stretch(), "IT", 23, TextAnchor.MiddleCenter, Color.white, FontStyle.Bold);
        return ghost;
    }

    private static void IntegrateScene(GameObject screenPrefab, GameObject previewPrefab, int previewLayer)
    {
        Scene scene = SceneManager.GetActiveScene();
        if (scene.path != ScenePath)
        {
            if (scene.isDirty)
            {
                throw new InvalidOperationException("The active scene has unsaved changes. Save it before rebuilding the inventory UI.");
            }

            scene = EditorSceneManager.OpenScene(ScenePath, OpenSceneMode.Single);
        }

        DestroySceneRootIfPresent(scene, "CharacterInventoryScreen");
        DestroySceneRootIfPresent(scene, "CharacterPreviewRig");

        GameObject screenInstance = (GameObject)PrefabUtility.InstantiatePrefab(screenPrefab, scene);
        screenInstance.name = "CharacterInventoryScreen";
        GameObject previewInstance = (GameObject)PrefabUtility.InstantiatePrefab(previewPrefab, scene);
        previewInstance.name = "CharacterPreviewRig";
        previewInstance.transform.position = new Vector3(1000f, -1000f, 1000f);

        PlayerInventory inventory = UnityEngine.Object.FindFirstObjectByType<PlayerInventory>();
        PlayerEquipment equipment = UnityEngine.Object.FindFirstObjectByType<PlayerEquipment>();
        CharacterPreviewController preview = previewInstance.GetComponent<CharacterPreviewController>();
        CharacterInventoryScreen screen = screenInstance.GetComponent<CharacterInventoryScreen>();
        SetSerializedObject(screen, "inventory", inventory);
        SetSerializedObject(screen, "equipment", equipment);
        SetSerializedObject(screen, "previewController", preview);
        SetSerializedObject(preview, "playerEquipment", equipment);

        EnsureEventSystem(scene);

        Camera mainCamera = Camera.main;
        if (mainCamera != null)
        {
            mainCamera.cullingMask &= ~(1 << previewLayer);
            EditorUtility.SetDirty(mainCamera);
        }

        foreach (Light light in UnityEngine.Object.FindObjectsByType<Light>(FindObjectsInactive.Exclude, FindObjectsSortMode.None))
        {
            if (light.transform.IsChildOf(previewInstance.transform))
            {
                continue;
            }

            light.cullingMask &= ~(1 << previewLayer);
            EditorUtility.SetDirty(light);
        }

        EditorSceneManager.MarkSceneDirty(scene);
        EditorSceneManager.SaveScene(scene);
    }

    private static void EnsureEventSystem(Scene scene)
    {
        EventSystem existing = UnityEngine.Object.FindFirstObjectByType<EventSystem>(FindObjectsInactive.Include);
        if (existing != null)
        {
            if (existing.GetComponent<InputSystemUIInputModule>() == null)
            {
                BaseInputModule oldModule = existing.GetComponent<BaseInputModule>();
                if (oldModule != null)
                {
                    UnityEngine.Object.DestroyImmediate(oldModule);
                }

                existing.gameObject.AddComponent<InputSystemUIInputModule>();
                EditorUtility.SetDirty(existing.gameObject);
            }

            return;
        }

        GameObject eventSystemObject = new("UIEventSystem");
        SceneManager.MoveGameObjectToScene(eventSystemObject, scene);
        eventSystemObject.AddComponent<EventSystem>();
        eventSystemObject.AddComponent<InputSystemUIInputModule>();
    }

    private static void UpdateWeaponDefinitions()
    {
        UpdateWeaponDefinition(
            SwordDataPath,
            "A balanced bronze sword. Reliable reach and a quick three-hit combat pattern.");
        UpdateWeaponDefinition(
            AxeDataPath,
            "A heavy bronze axe. Slower than the sword, with stronger damage and poise pressure.");
    }

    private static void UpdateWeaponDefinition(string path, string description)
    {
        WeaponData data = AssetDatabase.LoadAssetAtPath<WeaponData>(path);
        if (data == null)
        {
            throw new InvalidOperationException($"WeaponData was not found at {path}.");
        }

        SerializedObject serialized = new(data);
        serialized.FindProperty("description").stringValue = description;
        serialized.FindProperty("category").enumValueIndex = (int)ItemCategory.Weapon;
        serialized.FindProperty("acceptedEquipmentSlots").intValue = (int)EquipmentSlotMask.WeaponHands;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(data);
    }

    private static void UpdatePlayerPrefabSlots()
    {
        GameObject root = PrefabUtility.LoadPrefabContents(PlayerPrefabPath);
        try
        {
            PlayerEquipment equipment = root.GetComponent<PlayerEquipment>();
            if (equipment == null)
            {
                throw new InvalidOperationException("PlayerPlaceholder.prefab is missing PlayerEquipment.");
            }

            SerializedObject serialized = new(equipment);
            SerializedProperty slots = serialized.FindProperty("slots");
            slots.arraySize = Enum.GetValues(typeof(EquipmentSlotType)).Length;
            for (int i = 0; i < slots.arraySize; i++)
            {
                SerializedProperty element = slots.GetArrayElementAtIndex(i);
                element.FindPropertyRelative("slotType").enumValueIndex = i;
                element.FindPropertyRelative("itemInstanceId").stringValue = string.Empty;
            }

            serialized.FindProperty("activeSlot").enumValueIndex = (int)EquipmentSlotType.MainHand;
            serialized.FindProperty("legacyPrimaryItemInstanceId").stringValue = string.Empty;
            serialized.FindProperty("legacySecondaryItemInstanceId").stringValue = string.Empty;
            serialized.ApplyModifiedPropertiesWithoutUndo();
            PrefabUtility.SaveAsPrefabAsset(root, PlayerPrefabPath);
        }
        finally
        {
            PrefabUtility.UnloadPrefabContents(root);
        }
    }

    private static int EnsurePreviewLayer()
    {
        int existing = LayerMask.NameToLayer(PreviewLayerName);
        if (existing >= 0)
        {
            return existing;
        }

        UnityEngine.Object tagManagerAsset = AssetDatabase.LoadAllAssetsAtPath("ProjectSettings/TagManager.asset")[0];
        SerializedObject tagManager = new(tagManagerAsset);
        SerializedProperty layers = tagManager.FindProperty("layers");
        int targetLayer = PreferredPreviewLayer;
        if (!string.IsNullOrEmpty(layers.GetArrayElementAtIndex(targetLayer).stringValue))
        {
            targetLayer = -1;
            for (int i = 8; i < 32; i++)
            {
                if (string.IsNullOrEmpty(layers.GetArrayElementAtIndex(i).stringValue))
                {
                    targetLayer = i;
                    break;
                }
            }
        }

        if (targetLayer < 0)
        {
            throw new InvalidOperationException("No free Unity layer is available for CharacterPreview.");
        }

        layers.GetArrayElementAtIndex(targetLayer).stringValue = PreviewLayerName;
        tagManager.ApplyModifiedPropertiesWithoutUndo();
        AssetDatabase.SaveAssets();
        return targetLayer;
    }

    private static GameObject CreatePanel(string name, Transform parent, float preferredWidth, float minWidth, float flexibleWidth)
    {
        GameObject panel = CreateRectObject(name, parent);
        Image image = panel.AddComponent<Image>();
        image.sprite = UiSprite;
        image.type = Image.Type.Sliced;
        image.color = PanelColor;
        Outline outline = panel.AddComponent<Outline>();
        outline.effectColor = BronzeMutedColor;
        outline.effectDistance = new Vector2(1.5f, -1.5f);
        LayoutElement layout = panel.AddComponent<LayoutElement>();
        layout.preferredWidth = preferredWidth;
        layout.minWidth = minWidth;
        layout.flexibleWidth = flexibleWidth;
        return panel;
    }

    private static Text CreateStatText(string name, Transform parent, string value)
    {
        Text text = CreateText(name, parent, Stretch(), value, 15, TextAnchor.MiddleLeft, new Color(0.72f, 0.69f, 0.61f, 1f));
        LayoutElement layout = text.gameObject.AddComponent<LayoutElement>();
        layout.preferredHeight = 25f;
        return text;
    }

    private static Button CreateButton(
        string name,
        Transform parent,
        RectSpec rect,
        string text,
        Color backgroundColor,
        Color outlineColor,
        int fontSize)
    {
        GameObject root = CreateRectObject(name, parent);
        SetRect(root.GetComponent<RectTransform>(), rect);
        Image image = root.AddComponent<Image>();
        image.sprite = UiSprite;
        image.type = Image.Type.Sliced;
        image.color = backgroundColor;
        Outline outline = root.AddComponent<Outline>();
        outline.effectColor = outlineColor;
        outline.effectDistance = new Vector2(1f, -1f);
        Button button = root.AddComponent<Button>();
        button.targetGraphic = image;
        ColorBlock colors = button.colors;
        colors.normalColor = Color.white;
        colors.highlightedColor = new Color(1.18f, 1.12f, 0.96f, 1f);
        colors.pressedColor = new Color(0.72f, 0.68f, 0.62f, 1f);
        colors.disabledColor = new Color(0.35f, 0.35f, 0.35f, 0.55f);
        button.colors = colors;
        CreateText("Label", root.transform, Stretch(), text, fontSize, TextAnchor.MiddleCenter, IvoryColor, FontStyle.Bold);
        return button;
    }

    private static Image CreateImage(
        string name,
        Transform parent,
        RectSpec rect,
        Color color,
        Sprite sprite = null,
        bool sliced = false)
    {
        GameObject root = CreateRectObject(name, parent);
        SetRect(root.GetComponent<RectTransform>(), rect);
        Image image = root.AddComponent<Image>();
        image.color = color;
        image.sprite = sprite;
        image.type = sliced ? Image.Type.Sliced : Image.Type.Simple;
        return image;
    }

    private static Text CreateText(
        string name,
        Transform parent,
        RectSpec rect,
        string value,
        int fontSize,
        TextAnchor alignment,
        Color color,
        FontStyle fontStyle = FontStyle.Normal)
    {
        GameObject root = CreateRectObject(name, parent);
        SetRect(root.GetComponent<RectTransform>(), rect);
        Text text = root.AddComponent<Text>();
        text.font = Font;
        text.fontSize = fontSize;
        text.fontStyle = fontStyle;
        text.alignment = alignment;
        text.color = color;
        text.text = value;
        text.raycastTarget = false;
        text.horizontalOverflow = HorizontalWrapMode.Wrap;
        text.verticalOverflow = VerticalWrapMode.Truncate;
        return text;
    }

    private static GameObject CreateRectObject(string name, Transform parent)
    {
        GameObject root = new(name, typeof(RectTransform));
        if (parent != null)
        {
            root.transform.SetParent(parent, false);
        }

        return root;
    }

    private static void SetRect(RectTransform rect, RectSpec spec)
    {
        rect.anchorMin = spec.AnchorMin;
        rect.anchorMax = spec.AnchorMax;
        rect.offsetMin = spec.OffsetMin;
        rect.offsetMax = spec.OffsetMax;
        rect.localScale = Vector3.one;
    }

    private static RectSpec Stretch()
    {
        return new RectSpec(Vector2.zero, Vector2.one, Vector2.zero, Vector2.zero);
    }

    private static void CreatePreviewLight(
        Transform parent,
        string name,
        Vector3 euler,
        Color color,
        float intensity,
        int previewLayer)
    {
        GameObject lightObject = new(name);
        lightObject.transform.SetParent(parent, false);
        lightObject.transform.localRotation = Quaternion.Euler(euler);
        lightObject.layer = previewLayer;
        Light light = lightObject.AddComponent<Light>();
        light.type = LightType.Directional;
        light.color = color;
        light.intensity = intensity;
        light.shadows = LightShadows.Soft;
        light.cullingMask = 1 << previewLayer;
    }

    private static void SetLayerRecursively(GameObject root, int layer)
    {
        foreach (Transform child in root.GetComponentsInChildren<Transform>(true))
        {
            child.gameObject.layer = layer;
        }
    }

    private static void DestroySceneRootIfPresent(Scene scene, string objectName)
    {
        foreach (GameObject root in scene.GetRootGameObjects())
        {
            if (root.name == objectName)
            {
                UnityEngine.Object.DestroyImmediate(root);
            }
        }
    }

    private static void EnsureFolder(string parent, string folderName)
    {
        string path = parent + "/" + folderName;
        if (!AssetDatabase.IsValidFolder(path))
        {
            AssetDatabase.CreateFolder(parent, folderName);
        }
    }

    private static void SetSerializedObject(UnityEngine.Object target, string propertyName, UnityEngine.Object value)
    {
        SerializedObject serialized = new(target);
        SerializedProperty property = serialized.FindProperty(propertyName)
            ?? throw new InvalidOperationException($"Serialized property '{propertyName}' was not found on {target.GetType().Name}.");
        property.objectReferenceValue = value;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(target);
    }

    private static void SetSerializedInt(UnityEngine.Object target, string propertyName, int value)
    {
        SerializedObject serialized = new(target);
        serialized.FindProperty(propertyName).intValue = value;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(target);
    }

    private static void SetSerializedFloat(UnityEngine.Object target, string propertyName, float value)
    {
        SerializedObject serialized = new(target);
        serialized.FindProperty(propertyName).floatValue = value;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(target);
    }

    private static void SetSerializedEnum(UnityEngine.Object target, string propertyName, int value)
    {
        SerializedObject serialized = new(target);
        serialized.FindProperty(propertyName).enumValueIndex = value;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(target);
    }

    private static void SetSerializedArray<T>(UnityEngine.Object target, string propertyName, List<T> values)
        where T : UnityEngine.Object
    {
        SerializedObject serialized = new(target);
        SerializedProperty property = serialized.FindProperty(propertyName);
        property.arraySize = values.Count;
        for (int i = 0; i < values.Count; i++)
        {
            property.GetArrayElementAtIndex(i).objectReferenceValue = values[i];
        }

        serialized.ApplyModifiedPropertiesWithoutUndo();
        EditorUtility.SetDirty(target);
    }

    private readonly struct RectSpec
    {
        public readonly Vector2 AnchorMin;
        public readonly Vector2 AnchorMax;
        public readonly Vector2 OffsetMin;
        public readonly Vector2 OffsetMax;

        public RectSpec(Vector2 anchorMin, Vector2 anchorMax, Vector2 offsetMin, Vector2 offsetMax)
        {
            AnchorMin = anchorMin;
            AnchorMax = anchorMax;
            OffsetMin = offsetMin;
            OffsetMax = offsetMax;
        }
    }

    private sealed class DetailsReferences
    {
        public Text Name;
        public Text Description;
        public Text Type;
        public Text Rarity;
        public Text Quantity;
        public Text Durability;
        public Text Damage;
        public Text Metadata;
        public Text Feedback;
        public Button EquipButton;
        public Button UnequipButton;
        public Button ActivateButton;
        public Button DiscardButton;
    }
}
