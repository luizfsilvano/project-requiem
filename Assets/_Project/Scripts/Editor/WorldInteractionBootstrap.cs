#if UNITY_EDITOR
using System;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem.UI;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

public static class WorldInteractionBootstrap
{
    private const string MenuPath = "Tools/Combat Sandbox/Build World Interaction Foundation";
    private const string ScenePath = "Assets/_Project/Scenes/CombatSandbox.unity";
    private const string PlayerPrefabPath = "Assets/_Project/Prefabs/PlayerPlaceholder.prefab";
    private const string SwordPickupPath = "Assets/_Project/Prefabs/Items/BronzeSwordPickup.prefab";
    private const string AxePickupPath = "Assets/_Project/Prefabs/Items/BronzeAxePickup.prefab";
    private const string ItemSlotPrefabPath = "Assets/_Project/Prefabs/UI/InventoryItemSlot.prefab";
    private const string SwordDataPath = "Assets/_Project/Data/Weapons/BronzeSword.asset";
    private const string AxeDataPath = "Assets/_Project/Data/Weapons/BronzeAxe.asset";
    private const string PromptPrefabPath = "Assets/_Project/Prefabs/UI/InteractionPrompt.prefab";
    private const string ContainerScreenPrefabPath = "Assets/_Project/Prefabs/UI/ContainerTransferScreen.prefab";
    private const string DialoguePrefabPath = "Assets/_Project/Prefabs/UI/DialoguePanel.prefab";
    private const string ChestPrefabPath = "Assets/_Project/Prefabs/World/WorldChest.prefab";
    private const string DoorPrefabPath = "Assets/_Project/Prefabs/World/SimpleDoor.prefab";
    private const string NpcPrefabPath = "Assets/_Project/Prefabs/World/NpcPlaceholder.prefab";
    private const string MaterialFolder = "Assets/_Project/Materials/Interaction";
    private const string InteractionLayerName = "Interactable";
    private const int PreferredInteractionLayer = 29;

    private static readonly Color Background = new(0.025f, 0.023f, 0.021f, 0.96f);
    private static readonly Color Panel = new(0.065f, 0.058f, 0.049f, 0.98f);
    private static readonly Color PanelRaised = new(0.105f, 0.09f, 0.07f, 1f);
    private static readonly Color Bronze = new(0.66f, 0.47f, 0.2f, 1f);
    private static readonly Color BronzeMuted = new(0.36f, 0.27f, 0.15f, 1f);
    private static readonly Color Ivory = new(0.91f, 0.87f, 0.76f, 1f);
    private static readonly Color Muted = new(0.61f, 0.58f, 0.52f, 1f);
    private static Font Font => Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");

    [MenuItem(MenuPath)]
    public static void Build()
    {
        if (EditorApplication.isPlayingOrWillChangePlaymode)
        {
            throw new InvalidOperationException("Exit Play Mode before building the world interaction foundation.");
        }

        Scene activeScene = SceneManager.GetActiveScene();
        if (activeScene.IsValid() && activeScene.isDirty)
        {
            throw new InvalidOperationException(
                "Save or discard the active scene changes before building world interactions.");
        }

        EnsureFolders();
        int interactionLayer = EnsureInteractionLayer();
        Material wood = CreateOrUpdateMaterial(
            $"{MaterialFolder}/DarkWood.mat",
            new Color(0.18f, 0.09f, 0.045f, 1f),
            0.05f,
            0.28f);
        Material bronze = CreateOrUpdateMaterial(
            $"{MaterialFolder}/BronzeTrim.mat",
            new Color(0.42f, 0.24f, 0.08f, 1f),
            0.72f,
            0.46f);
        Material stone = CreateOrUpdateMaterial(
            $"{MaterialFolder}/DoorStone.mat",
            new Color(0.19f, 0.18f, 0.16f, 1f),
            0.02f,
            0.18f);
        Material npcCloth = CreateOrUpdateMaterial(
            $"{MaterialFolder}/NpcCloth.mat",
            new Color(0.12f, 0.24f, 0.2f, 1f),
            0f,
            0.24f);
        Material npcSkin = CreateOrUpdateMaterial(
            $"{MaterialFolder}/NpcSkin.mat",
            new Color(0.62f, 0.42f, 0.29f, 1f),
            0f,
            0.3f);

        InventoryItemSlotView itemSlotPrefab = AssetDatabase.LoadAssetAtPath<InventoryItemSlotView>(ItemSlotPrefabPath);
        WeaponData sword = AssetDatabase.LoadAssetAtPath<WeaponData>(SwordDataPath);
        WeaponData axe = AssetDatabase.LoadAssetAtPath<WeaponData>(AxeDataPath);
        if (itemSlotPrefab == null || sword == null || axe == null)
        {
            throw new InvalidOperationException("The existing inventory card and Bronze weapon definitions are required.");
        }

        GameObject promptPrefab = BuildPromptPrefab();
        GameObject containerScreenPrefab = BuildContainerScreenPrefab(itemSlotPrefab);
        GameObject dialoguePrefab = BuildDialoguePrefab();
        GameObject chestPrefab = BuildChestPrefab(interactionLayer, wood, bronze, sword, axe);
        GameObject doorPrefab = BuildDoorPrefab(interactionLayer, wood, bronze, stone);
        GameObject npcPrefab = BuildNpcPrefab(interactionLayer, npcCloth, npcSkin, bronze);
        QuestSystemBootstrap.RestoreQuestAugmentationsIfAvailable();

        dialoguePrefab = AssetDatabase.LoadAssetAtPath<GameObject>(DialoguePrefabPath);
        npcPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(NpcPrefabPath);

        UpdatePlayerPrefab(interactionLayer);
        UpdatePickupPrefab(SwordPickupPath, interactionLayer, "Bronze Sword", "Coletar a espada de bronze.");
        UpdatePickupPrefab(AxePickupPath, interactionLayer, "Bronze Axe", "Coletar o machado de bronze.");
        IntegrateScene(
            interactionLayer,
            promptPrefab,
            containerScreenPrefab,
            dialoguePrefab,
            chestPrefab,
            doorPrefab,
            npcPrefab);

        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();
        Debug.Log("World interaction foundation built without running CombatSandboxSceneBuilder.");
    }

    private static GameObject BuildPromptPrefab()
    {
        GameObject root = CreateCanvasRoot("InteractionPrompt", 20);
        InteractionPromptView view = root.AddComponent<InteractionPromptView>();
        GameObject promptRoot = CreateUiObject("PromptRoot", root.transform);
        RectTransform promptRect = promptRoot.GetComponent<RectTransform>();
        SetAnchored(promptRect, new Vector2(0.5f, 0f), new Vector2(0.5f, 0f), new Vector2(0f, 135f), new Vector2(590f, 64f));
        Image background = promptRoot.AddComponent<Image>();
        background.color = new Color(0.035f, 0.03f, 0.025f, 0.96f);
        Outline outline = promptRoot.AddComponent<Outline>();
        outline.effectColor = BronzeMuted;
        outline.effectDistance = new Vector2(2f, -2f);
        Text text = CreateText("PromptText", promptRoot.transform, "E — Interagir", 22, TextAnchor.MiddleCenter, Ivory, FontStyle.Bold);
        Stretch(text.rectTransform, 18f, 8f, 18f, 8f);

        SerializedObject serialized = new(view);
        SetObject(serialized, "promptRoot", promptRoot);
        SetObject(serialized, "promptText", text);
        serialized.ApplyModifiedPropertiesWithoutUndo();
        promptRoot.SetActive(false);

        GameObject prefab = SavePrefab(root, PromptPrefabPath);
        UnityEngine.Object.DestroyImmediate(root);
        return prefab;
    }

    private static GameObject BuildDialoguePrefab()
    {
        GameObject root = CreateCanvasRoot("DialoguePanel", 60);
        DialoguePanel dialogue = root.AddComponent<DialoguePanel>();
        GameObject panelRoot = CreatePanel("PanelRoot", root.transform, new Color(0.04f, 0.035f, 0.03f, 0.985f));
        SetAnchored(
            panelRoot.GetComponent<RectTransform>(),
            new Vector2(0.5f, 0f),
            new Vector2(0.5f, 0f),
            new Vector2(0f, 170f),
            new Vector2(980f, 220f));
        Outline outline = panelRoot.AddComponent<Outline>();
        outline.effectColor = BronzeMuted;
        outline.effectDistance = new Vector2(2f, -2f);

        Image topRule = CreateImage("TopRule", panelRoot.transform, Bronze);
        Anchor(topRule.rectTransform, new Vector2(0f, 1f), new Vector2(1f, 1f), Vector2.zero, new Vector2(0f, 4f), new Vector2(0.5f, 1f));
        Text speaker = CreateText("Speaker", panelRoot.transform, "ALDEÃO", 27, TextAnchor.MiddleLeft, Bronze, FontStyle.Bold);
        Anchor(speaker.rectTransform, new Vector2(0f, 0.67f), new Vector2(0.8f, 0.96f), new Vector2(34f, 0f), new Vector2(-10f, 0f));
        Text line = CreateText(
            "Dialogue",
            panelRoot.transform,
            "A floresta está perigosa desde que os esqueletos apareceram.",
            22,
            TextAnchor.UpperLeft,
            Ivory,
            FontStyle.Normal);
        Anchor(line.rectTransform, new Vector2(0f, 0.23f), new Vector2(1f, 0.68f), new Vector2(34f, 0f), new Vector2(-34f, 0f));
        Text hint = CreateText("CloseHint", panelRoot.transform, "E / Esc — Fechar", 15, TextAnchor.MiddleRight, Muted, FontStyle.Italic);
        Anchor(hint.rectTransform, new Vector2(0.55f, 0.04f), new Vector2(0.95f, 0.22f), Vector2.zero, Vector2.zero);
        Button close = CreateButton("Close", panelRoot.transform, "X", 15);
        SetAnchored(close.GetComponent<RectTransform>(), new Vector2(1f, 1f), new Vector2(1f, 1f), new Vector2(-28f, -28f), new Vector2(42f, 42f));

        SerializedObject serialized = new(dialogue);
        SetObject(serialized, "panelRoot", panelRoot);
        SetObject(serialized, "speakerText", speaker);
        SetObject(serialized, "dialogueText", line);
        SetObject(serialized, "closeHintText", hint);
        SetObject(serialized, "closeButton", close);
        SetFloat(serialized, "maxDialogueDistance", 4.5f);
        serialized.ApplyModifiedPropertiesWithoutUndo();
        panelRoot.SetActive(false);

        GameObject prefab = SavePrefab(root, DialoguePrefabPath);
        UnityEngine.Object.DestroyImmediate(root);
        return prefab;
    }

    private static GameObject BuildContainerScreenPrefab(InventoryItemSlotView itemSlotPrefab)
    {
        GameObject root = CreateCanvasRoot("ContainerTransferScreen", 50);
        ContainerTransferScreen screen = root.AddComponent<ContainerTransferScreen>();
        GameObject screenRoot = CreatePanel("ScreenRoot", root.transform, Background);
        Stretch(screenRoot.GetComponent<RectTransform>(), 0f, 0f, 0f, 0f);

        Text title = CreateText("Title", screenRoot.transform, "CONTAINER TRANSFER", 29, TextAnchor.MiddleCenter, Ivory, FontStyle.Bold);
        Anchor(title.rectTransform, new Vector2(0.25f, 0.91f), new Vector2(0.75f, 0.98f), Vector2.zero, Vector2.zero);
        Text subtitle = CreateText(
            "Subtitle",
            screenRoot.transform,
            "Move the same item instance safely between owners",
            14,
            TextAnchor.MiddleCenter,
            Muted,
            FontStyle.Italic);
        Anchor(subtitle.rectTransform, new Vector2(0.25f, 0.875f), new Vector2(0.75f, 0.925f), Vector2.zero, Vector2.zero);
        Button close = CreateButton("CloseButton", screenRoot.transform, "CLOSE  X", 13);
        SetAnchored(close.GetComponent<RectTransform>(), new Vector2(1f, 1f), new Vector2(1f, 1f), new Vector2(-92f, -42f), new Vector2(150f, 46f));

        GameObject playerPanel = CreatePanel("PlayerInventoryPanel", screenRoot.transform, Panel);
        Anchor(playerPanel.GetComponent<RectTransform>(), new Vector2(0.035f, 0.29f), new Vector2(0.485f, 0.875f), Vector2.zero, Vector2.zero);
        Text playerTitle = CreateText("PlayerTitle", playerPanel.transform, "PLAYER INVENTORY", 21, TextAnchor.MiddleLeft, Bronze, FontStyle.Bold);
        Anchor(playerTitle.rectTransform, new Vector2(0.05f, 0.88f), new Vector2(0.95f, 0.97f), Vector2.zero, Vector2.zero);
        Transform playerGrid = CreateScrollGrid("PlayerItems", playerPanel.transform);

        GameObject containerPanel = CreatePanel("ContainerPanel", screenRoot.transform, Panel);
        Anchor(containerPanel.GetComponent<RectTransform>(), new Vector2(0.515f, 0.29f), new Vector2(0.965f, 0.875f), Vector2.zero, Vector2.zero);
        Text containerTitle = CreateText("ContainerTitle", containerPanel.transform, "WORLD CONTAINER", 21, TextAnchor.MiddleLeft, Bronze, FontStyle.Bold);
        Anchor(containerTitle.rectTransform, new Vector2(0.05f, 0.88f), new Vector2(0.95f, 0.97f), Vector2.zero, Vector2.zero);
        Transform containerGrid = CreateScrollGrid("ContainerItems", containerPanel.transform);

        GameObject details = CreatePanel("ItemDetails", screenRoot.transform, new Color(0.05f, 0.045f, 0.038f, 0.99f));
        Anchor(details.GetComponent<RectTransform>(), new Vector2(0.035f, 0.035f), new Vector2(0.965f, 0.255f), Vector2.zero, Vector2.zero);
        Text detailName = CreateText("ItemName", details.transform, "Select an item", 23, TextAnchor.MiddleLeft, Ivory, FontStyle.Bold);
        Anchor(detailName.rectTransform, new Vector2(0.025f, 0.61f), new Vector2(0.36f, 0.93f), Vector2.zero, Vector2.zero);
        Text detailDescription = CreateText(
            "Description",
            details.transform,
            "Choose an item from either side to inspect and transfer it.",
            14,
            TextAnchor.UpperLeft,
            Muted,
            FontStyle.Normal);
        Anchor(detailDescription.rectTransform, new Vector2(0.025f, 0.15f), new Vector2(0.39f, 0.62f), Vector2.zero, Vector2.zero);

        Text detailType = CreateDetailText("Type", details.transform, "TYPE  —", 0.41f, 0.67f);
        Text detailRarity = CreateDetailText("Rarity", details.transform, "RARITY  —    QUALITY  —", 0.41f, 0.51f);
        Text detailQuantity = CreateDetailText("Quantity", details.transform, "QUANTITY  —", 0.41f, 0.35f);
        Text detailDurability = CreateDetailText("Durability", details.transform, "DURABILITY  —", 0.41f, 0.19f);
        Text detailDamage = CreateDetailText("Damage", details.transform, string.Empty, 0.41f, 0.03f);
        Text detailMetadata = CreateDetailText("Metadata", details.transform, string.Empty, 0.65f, 0.03f);

        GameObject actions = CreateUiObject("Actions", details.transform);
        Anchor(actions.GetComponent<RectTransform>(), new Vector2(0.70f, 0.20f), new Vector2(0.98f, 0.89f), Vector2.zero, Vector2.zero);
        GridLayoutGroup actionGrid = actions.AddComponent<GridLayoutGroup>();
        actionGrid.cellSize = new Vector2(132f, 48f);
        actionGrid.spacing = new Vector2(10f, 10f);
        actionGrid.constraint = GridLayoutGroup.Constraint.FixedColumnCount;
        actionGrid.constraintCount = 2;
        actionGrid.childAlignment = TextAnchor.MiddleCenter;
        Button take = CreateButton("Take", actions.transform, "TAKE", 13);
        Button store = CreateButton("Store", actions.transform, "STORE", 13);
        Button takeAll = CreateButton("TakeAll", actions.transform, "TAKE ALL", 13);
        CreateText("Spacer", actions.transform, string.Empty, 1, TextAnchor.MiddleCenter, Color.clear, FontStyle.Normal);

        Text feedback = CreateText("Feedback", details.transform, string.Empty, 13, TextAnchor.MiddleRight, Bronze, FontStyle.Italic);
        Anchor(feedback.rectTransform, new Vector2(0.68f, 0.02f), new Vector2(0.975f, 0.2f), Vector2.zero, Vector2.zero);

        GameObject dragGhostObject = CreatePanel("DragGhost", screenRoot.transform, PanelRaised);
        RectTransform dragGhost = dragGhostObject.GetComponent<RectTransform>();
        SetAnchored(dragGhost, new Vector2(0.5f, 0.5f), new Vector2(0.5f, 0.5f), Vector2.zero, new Vector2(76f, 76f));
        CanvasGroup ghostCanvas = dragGhostObject.AddComponent<CanvasGroup>();
        ghostCanvas.blocksRaycasts = false;
        Text ghostLabel = CreateText("Label", dragGhostObject.transform, "IT", 23, TextAnchor.MiddleCenter, Ivory, FontStyle.Bold);
        Stretch(ghostLabel.rectTransform, 0f, 0f, 0f, 0f);

        SerializedObject serialized = new(screen);
        SetObject(serialized, "screenRoot", screenRoot);
        SetObject(serialized, "containerTitle", containerTitle);
        SetObject(serialized, "playerGrid", playerGrid);
        SetObject(serialized, "containerGrid", containerGrid);
        SetObject(serialized, "itemSlotPrefab", itemSlotPrefab);
        SetInt(serialized, "minimumGridSlots", 20);
        SetObject(serialized, "detailName", detailName);
        SetObject(serialized, "detailDescription", detailDescription);
        SetObject(serialized, "detailType", detailType);
        SetObject(serialized, "detailRarity", detailRarity);
        SetObject(serialized, "detailQuantity", detailQuantity);
        SetObject(serialized, "detailDurability", detailDurability);
        SetObject(serialized, "detailDamage", detailDamage);
        SetObject(serialized, "detailMetadata", detailMetadata);
        SetObject(serialized, "feedbackText", feedback);
        SetObject(serialized, "takeButton", take);
        SetObject(serialized, "storeButton", store);
        SetObject(serialized, "takeAllButton", takeAll);
        SetObject(serialized, "closeButton", close);
        SetObject(serialized, "dragGhost", dragGhost);
        SetObject(serialized, "dragGhostBackground", dragGhostObject.GetComponent<Image>());
        SetObject(serialized, "dragGhostLabel", ghostLabel);
        serialized.ApplyModifiedPropertiesWithoutUndo();
        dragGhostObject.SetActive(false);
        screenRoot.SetActive(false);

        GameObject prefab = SavePrefab(root, ContainerScreenPrefabPath);
        UnityEngine.Object.DestroyImmediate(root);
        return prefab;
    }

    private static GameObject BuildChestPrefab(
        int layer,
        Material wood,
        Material bronze,
        WeaponData sword,
        WeaponData axe)
    {
        GameObject root = new("WorldChest");
        root.layer = layer;
        BoxCollider collider = root.AddComponent<BoxCollider>();
        collider.center = new Vector3(0f, 0.55f, 0f);
        collider.size = new Vector3(1.7f, 1.1f, 1.05f);
        WorldItemContainer container = root.AddComponent<WorldItemContainer>();

        CreatePrimitive("Body", PrimitiveType.Cube, root.transform, new Vector3(0f, 0.46f, 0f), new Vector3(1.65f, 0.88f, 1f), wood, false, layer);
        CreatePrimitive("FrontBand", PrimitiveType.Cube, root.transform, new Vector3(0f, 0.48f, 0.51f), new Vector3(1.72f, 0.16f, 0.06f), bronze, false, layer);
        CreatePrimitive("Lock", PrimitiveType.Cube, root.transform, new Vector3(0f, 0.58f, 0.57f), new Vector3(0.24f, 0.34f, 0.12f), bronze, false, layer);

        GameObject lidPivot = new("LidPivot");
        lidPivot.layer = layer;
        lidPivot.transform.SetParent(root.transform, false);
        lidPivot.transform.localPosition = new Vector3(0f, 0.95f, -0.49f);
        CreatePrimitive("Lid", PrimitiveType.Cube, lidPivot.transform, new Vector3(0f, 0f, 0.49f), new Vector3(1.7f, 0.18f, 1.05f), wood, false, layer);
        CreatePrimitive("LidBand", PrimitiveType.Cube, lidPivot.transform, new Vector3(0f, 0.03f, 0.51f), new Vector3(0.18f, 0.24f, 1.1f), bronze, false, layer);

        Transform point = CreatePoint("InteractionPoint", root.transform, new Vector3(0f, 0.85f, 0.72f), layer);
        SerializedObject serialized = new(container);
        ConfigureInteractable(serialized, "Baú de Viajante", "Um container local de teste.", 20, 3.6f, point);
        SetObject(serialized, "lid", lidPivot.transform);
        SetVector3(serialized, "closedLidEuler", Vector3.zero);
        SetVector3(serialized, "openLidEuler", new Vector3(-72f, 0f, 0f));
        SetFloat(serialized, "lidRotationSpeed", 185f);
        SerializedProperty itemContainer = RequireProperty(serialized, "itemContainer");
        SerializedProperty capacity = itemContainer.FindPropertyRelative("capacity");
        if (capacity != null)
        {
            capacity.intValue = 20;
        }

        SerializedProperty initial = RequireProperty(serialized, "initialContents");
        initial.arraySize = 2;
        ConfigureSeed(initial.GetArrayElementAtIndex(0), sword, 1);
        ConfigureSeed(initial.GetArrayElementAtIndex(1), axe, 1);
        serialized.ApplyModifiedPropertiesWithoutUndo();

        GameObject prefab = SavePrefab(root, ChestPrefabPath);
        UnityEngine.Object.DestroyImmediate(root);
        return prefab;
    }

    private static GameObject BuildDoorPrefab(
        int layer,
        Material wood,
        Material bronze,
        Material stone)
    {
        GameObject root = new("SimpleDoor");
        root.layer = layer;
        SimpleDoorInteractable door = root.AddComponent<SimpleDoorInteractable>();
        CreatePrimitive("LeftFrame", PrimitiveType.Cube, root.transform, new Vector3(-1.05f, 1.2f, 0f), new Vector3(0.22f, 2.4f, 0.35f), stone, true, layer);
        CreatePrimitive("RightFrame", PrimitiveType.Cube, root.transform, new Vector3(1.05f, 1.2f, 0f), new Vector3(0.22f, 2.4f, 0.35f), stone, true, layer);
        CreatePrimitive("TopFrame", PrimitiveType.Cube, root.transform, new Vector3(0f, 2.35f, 0f), new Vector3(2.3f, 0.22f, 0.35f), stone, true, layer);

        GameObject pivot = new("DoorPivot");
        pivot.layer = layer;
        pivot.transform.SetParent(root.transform, false);
        pivot.transform.localPosition = new Vector3(-0.9f, 0f, 0f);
        CreatePrimitive("DoorLeaf", PrimitiveType.Cube, pivot.transform, new Vector3(0.9f, 1.12f, 0f), new Vector3(1.8f, 2.22f, 0.18f), wood, true, layer);
        CreatePrimitive("Handle", PrimitiveType.Sphere, pivot.transform, new Vector3(1.55f, 1.12f, 0.14f), Vector3.one * 0.16f, bronze, false, layer);

        Transform point = CreatePoint("InteractionPoint", root.transform, new Vector3(0f, 1.15f, 0.65f), layer);
        SerializedObject serialized = new(door);
        ConfigureInteractable(serialized, "Porta de Madeira", "Uma porta simples de teste.", 10, 3.7f, point);
        SetObject(serialized, "doorPivot", pivot.transform);
        SetVector3(serialized, "openEulerOffset", new Vector3(0f, 105f, 0f));
        SetFloat(serialized, "animationDuration", 0.55f);
        SetBool(serialized, "startsOpen", false);
        serialized.ApplyModifiedPropertiesWithoutUndo();

        GameObject prefab = SavePrefab(root, DoorPrefabPath);
        UnityEngine.Object.DestroyImmediate(root);
        return prefab;
    }

    private static GameObject BuildNpcPrefab(
        int layer,
        Material cloth,
        Material skin,
        Material bronze)
    {
        GameObject root = new("NpcPlaceholder");
        root.layer = layer;
        CapsuleCollider collider = root.AddComponent<CapsuleCollider>();
        collider.center = new Vector3(0f, 1f, 0f);
        collider.height = 2f;
        collider.radius = 0.43f;
        SimpleNpcInteractable npc = root.AddComponent<SimpleNpcInteractable>();

        CreatePrimitive("Body", PrimitiveType.Capsule, root.transform, new Vector3(0f, 1f, 0f), new Vector3(0.62f, 0.95f, 0.62f), cloth, false, layer);
        CreatePrimitive("Head", PrimitiveType.Sphere, root.transform, new Vector3(0f, 2.03f, 0f), Vector3.one * 0.58f, skin, false, layer);
        CreatePrimitive("Belt", PrimitiveType.Cube, root.transform, new Vector3(0f, 0.92f, 0f), new Vector3(0.68f, 0.12f, 0.44f), bronze, false, layer);
        Transform point = CreatePoint("InteractionPoint", root.transform, new Vector3(0f, 1.45f, 0.35f), layer);

        SerializedObject serialized = new(npc);
        ConfigureInteractable(serialized, "Aldeão", "Um morador preocupado com a floresta.", 15, 3.5f, point);
        SetString(serialized, "speakerName", "Aldeão");
        SetString(serialized, "dialogueLine", "A floresta está perigosa desde que os esqueletos apareceram.");
        serialized.ApplyModifiedPropertiesWithoutUndo();

        GameObject prefab = SavePrefab(root, NpcPrefabPath);
        UnityEngine.Object.DestroyImmediate(root);
        return prefab;
    }

    private static void UpdatePlayerPrefab(int interactionLayer)
    {
        GameObject root = PrefabUtility.LoadPrefabContents(PlayerPrefabPath);
        try
        {
            PlayerInteractor interactor = root.GetComponent<PlayerInteractor>();
            if (interactor == null)
            {
                interactor = root.AddComponent<PlayerInteractor>();
            }

            SerializedObject serialized = new(interactor);
            SetFloat(serialized, "searchRange", 4f);
            SetFloat(serialized, "searchRadius", 1.35f);
            SetFloat(serialized, "maxInteractionAngle", 75f);
            SetFloat(serialized, "originHeight", 1f);
            SetFloat(serialized, "searchInterval", 0.08f);
            RequireProperty(serialized, "interactionMask").intValue = 1 << interactionLayer;
            SetObject(serialized, "meleeAttack", root.GetComponent<BasicMeleeAttack>());
            SetObject(serialized, "movement", root.GetComponent<PlayerCameraRelativeMovement>());
            SetObject(serialized, "cameraTransform", null);
            SetObject(serialized, "promptView", null);
            SetObject(serialized, "containerScreen", null);
            SetObject(serialized, "dialoguePanel", null);
            serialized.ApplyModifiedPropertiesWithoutUndo();
            PrefabUtility.SaveAsPrefabAsset(root, PlayerPrefabPath);
        }
        finally
        {
            PrefabUtility.UnloadPrefabContents(root);
        }
    }

    private static void UpdatePickupPrefab(
        string path,
        int interactionLayer,
        string displayName,
        string description)
    {
        GameObject root = PrefabUtility.LoadPrefabContents(path);
        try
        {
            SetLayerRecursively(root, interactionLayer);
            WeaponPickup pickup = root.GetComponent<WeaponPickup>();
            if (pickup == null)
            {
                throw new InvalidOperationException($"Missing WeaponPickup on {path}.");
            }

            SerializedObject serialized = new(pickup);
            ConfigureInteractable(serialized, displayName, description, 30, 3.5f, root.transform);
            serialized.ApplyModifiedPropertiesWithoutUndo();
            PrefabUtility.SaveAsPrefabAsset(root, path);
        }
        finally
        {
            PrefabUtility.UnloadPrefabContents(root);
        }
    }

    private static void IntegrateScene(
        int interactionLayer,
        GameObject promptPrefab,
        GameObject containerScreenPrefab,
        GameObject dialoguePrefab,
        GameObject chestPrefab,
        GameObject doorPrefab,
        GameObject npcPrefab)
    {
        Scene scene = SceneManager.GetActiveScene();
        if (scene.path != ScenePath)
        {
            if (scene.isDirty)
            {
                throw new InvalidOperationException("Save the current scene before integrating world interactions.");
            }

            scene = EditorSceneManager.OpenScene(ScenePath, OpenSceneMode.Single);
        }

        string[] ownedRoots =
        {
            "InteractionPrompt",
            "ContainerTransferScreen",
            "DialoguePanel",
            "WorldChest",
            "SimpleDoor",
            "NpcPlaceholder"
        };
        foreach (string ownedRoot in ownedRoots)
        {
            GameObject existing = FindSceneRoot(scene, ownedRoot);
            if (existing != null)
            {
                UnityEngine.Object.DestroyImmediate(existing);
            }
        }

        GameObject prompt = InstantiateScenePrefab(promptPrefab, "InteractionPrompt", Vector3.zero, Quaternion.identity);
        GameObject containerScreen = InstantiateScenePrefab(containerScreenPrefab, "ContainerTransferScreen", Vector3.zero, Quaternion.identity);
        GameObject dialogue = InstantiateScenePrefab(dialoguePrefab, "DialoguePanel", Vector3.zero, Quaternion.identity);
        InstantiateScenePrefab(chestPrefab, "WorldChest", new Vector3(-4.4f, 0f, 2.6f), Quaternion.identity);
        InstantiateScenePrefab(doorPrefab, "SimpleDoor", new Vector3(-7f, 0f, 7f), Quaternion.Euler(0f, 90f, 0f));
        Vector3 npcPosition = new(5f, 0f, -3f);
        Vector3 npcForward = Vector3.ProjectOnPlane(-npcPosition, Vector3.up).normalized;
        GameObject npc = InstantiateScenePrefab(
            npcPrefab,
            "NpcPlaceholder",
            npcPosition,
            Quaternion.LookRotation(npcForward, Vector3.up));
        QuestSystemBootstrap.RestoreQuestNpcSceneInstanceIfAvailable(npc);

        GameObject player = FindSceneRoot(scene, "PlayerPlaceholder");
        if (player == null)
        {
            throw new InvalidOperationException("PlayerPlaceholder was not found in CombatSandbox.");
        }

        PlayerInteractor interactor = player.GetComponent<PlayerInteractor>();
        if (interactor == null)
        {
            interactor = player.AddComponent<PlayerInteractor>();
        }

        SerializedObject serialized = new(interactor);
        SetObject(serialized, "cameraTransform", Camera.main != null ? Camera.main.transform : null);
        SetObject(serialized, "meleeAttack", player.GetComponent<BasicMeleeAttack>());
        SetObject(serialized, "movement", player.GetComponent<PlayerCameraRelativeMovement>());
        SetObject(serialized, "promptView", prompt.GetComponent<InteractionPromptView>());
        SetObject(serialized, "containerScreen", containerScreen.GetComponent<ContainerTransferScreen>());
        SetObject(serialized, "dialoguePanel", dialogue.GetComponent<DialoguePanel>());
        RequireProperty(serialized, "interactionMask").intValue = 1 << interactionLayer;
        serialized.ApplyModifiedPropertiesWithoutUndo();

        EventSystem[] eventSystems = UnityEngine.Object.FindObjectsByType<EventSystem>(
            FindObjectsInactive.Include,
            FindObjectsSortMode.None);
        if (eventSystems.Length == 0)
        {
            GameObject eventSystemObject = new("UIEventSystem");
            eventSystemObject.AddComponent<EventSystem>();
            eventSystemObject.AddComponent<InputSystemUIInputModule>();
            SceneManager.MoveGameObjectToScene(eventSystemObject, scene);
        }
        else if (eventSystems.Length > 1)
        {
            throw new InvalidOperationException("CombatSandbox contains more than one EventSystem.");
        }

        EditorSceneManager.MarkSceneDirty(scene);
        EditorSceneManager.SaveScene(scene);
    }

    private static GameObject FindSceneRoot(Scene scene, string name)
    {
        if (!scene.IsValid() || string.IsNullOrWhiteSpace(name))
        {
            return null;
        }

        foreach (GameObject root in scene.GetRootGameObjects())
        {
            if (root.name == name)
            {
                return root;
            }
        }

        return null;
    }

    private static Transform CreateScrollGrid(string name, Transform parent)
    {
        GameObject scrollObject = CreateUiObject(name, parent);
        Anchor(scrollObject.GetComponent<RectTransform>(), new Vector2(0.045f, 0.055f), new Vector2(0.955f, 0.865f), Vector2.zero, Vector2.zero);
        Image background = scrollObject.AddComponent<Image>();
        background.color = new Color(0.025f, 0.023f, 0.021f, 0.78f);
        RectMask2D mask = scrollObject.AddComponent<RectMask2D>();
        ScrollRect scrollRect = scrollObject.AddComponent<ScrollRect>();
        scrollRect.horizontal = false;
        scrollRect.vertical = true;
        scrollRect.movementType = ScrollRect.MovementType.Clamped;
        scrollRect.scrollSensitivity = 30f;
        scrollRect.viewport = scrollObject.GetComponent<RectTransform>();

        GameObject content = CreateUiObject("Grid", scrollObject.transform);
        RectTransform contentRect = content.GetComponent<RectTransform>();
        contentRect.anchorMin = new Vector2(0f, 1f);
        contentRect.anchorMax = new Vector2(1f, 1f);
        contentRect.pivot = new Vector2(0.5f, 1f);
        contentRect.anchoredPosition = new Vector2(0f, -12f);
        contentRect.sizeDelta = new Vector2(-24f, 0f);
        GridLayoutGroup grid = content.AddComponent<GridLayoutGroup>();
        grid.cellSize = new Vector2(75f, 75f);
        grid.spacing = new Vector2(9f, 9f);
        grid.constraint = GridLayoutGroup.Constraint.FixedColumnCount;
        grid.constraintCount = 5;
        grid.childAlignment = TextAnchor.UpperCenter;
        ContentSizeFitter fitter = content.AddComponent<ContentSizeFitter>();
        fitter.verticalFit = ContentSizeFitter.FitMode.PreferredSize;
        scrollRect.content = contentRect;
        return content.transform;
    }

    private static Text CreateDetailText(
        string name,
        Transform parent,
        string value,
        float minX,
        float minY)
    {
        Text text = CreateText(name, parent, value, 13, TextAnchor.MiddleLeft, Muted, FontStyle.Normal);
        Anchor(text.rectTransform, new Vector2(minX, minY), new Vector2(Mathf.Min(0.69f, minX + 0.28f), minY + 0.15f), Vector2.zero, Vector2.zero);
        return text;
    }

    private static GameObject CreateCanvasRoot(string name, int sortingOrder)
    {
        GameObject root = new(name, typeof(RectTransform));
        Canvas canvas = root.AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = sortingOrder;
        CanvasScaler scaler = root.AddComponent<CanvasScaler>();
        scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
        scaler.referenceResolution = new Vector2(1920f, 1080f);
        scaler.matchWidthOrHeight = 0.5f;
        root.AddComponent<GraphicRaycaster>();
        return root;
    }

    private static GameObject CreateUiObject(string name, Transform parent)
    {
        GameObject gameObject = new(name, typeof(RectTransform));
        gameObject.transform.SetParent(parent, false);
        return gameObject;
    }

    private static GameObject CreatePanel(string name, Transform parent, Color color)
    {
        GameObject panel = CreateUiObject(name, parent);
        Image image = panel.AddComponent<Image>();
        image.color = color;
        return panel;
    }

    private static Image CreateImage(string name, Transform parent, Color color)
    {
        GameObject gameObject = CreateUiObject(name, parent);
        Image image = gameObject.AddComponent<Image>();
        image.color = color;
        return image;
    }

    private static Text CreateText(
        string name,
        Transform parent,
        string value,
        int fontSize,
        TextAnchor alignment,
        Color color,
        FontStyle style)
    {
        GameObject gameObject = CreateUiObject(name, parent);
        Text text = gameObject.AddComponent<Text>();
        text.font = Font;
        text.text = value;
        text.fontSize = fontSize;
        text.alignment = alignment;
        text.color = color;
        text.fontStyle = style;
        text.horizontalOverflow = HorizontalWrapMode.Wrap;
        text.verticalOverflow = VerticalWrapMode.Truncate;
        text.raycastTarget = false;
        return text;
    }

    private static Button CreateButton(string name, Transform parent, string label, int fontSize)
    {
        GameObject gameObject = CreateUiObject(name, parent);
        Image image = gameObject.AddComponent<Image>();
        image.color = PanelRaised;
        Outline outline = gameObject.AddComponent<Outline>();
        outline.effectColor = BronzeMuted;
        outline.effectDistance = new Vector2(1f, -1f);
        Button button = gameObject.AddComponent<Button>();
        button.targetGraphic = image;
        ColorBlock colors = button.colors;
        colors.normalColor = Color.white;
        colors.highlightedColor = new Color(1.15f, 1.08f, 0.9f, 1f);
        colors.pressedColor = new Color(0.75f, 0.65f, 0.48f, 1f);
        colors.disabledColor = new Color(0.35f, 0.35f, 0.35f, 0.5f);
        button.colors = colors;
        Text text = CreateText("Label", gameObject.transform, label, fontSize, TextAnchor.MiddleCenter, Ivory, FontStyle.Bold);
        Stretch(text.rectTransform, 4f, 4f, 4f, 4f);
        return button;
    }

    private static GameObject CreatePrimitive(
        string name,
        PrimitiveType type,
        Transform parent,
        Vector3 localPosition,
        Vector3 localScale,
        Material material,
        bool keepCollider,
        int layer)
    {
        GameObject gameObject = GameObject.CreatePrimitive(type);
        gameObject.name = name;
        gameObject.layer = layer;
        gameObject.transform.SetParent(parent, false);
        gameObject.transform.localPosition = localPosition;
        gameObject.transform.localRotation = Quaternion.identity;
        gameObject.transform.localScale = localScale;
        Renderer renderer = gameObject.GetComponent<Renderer>();
        if (renderer != null)
        {
            renderer.sharedMaterial = material;
        }

        if (!keepCollider)
        {
            Collider collider = gameObject.GetComponent<Collider>();
            if (collider != null)
            {
                UnityEngine.Object.DestroyImmediate(collider);
            }
        }

        return gameObject;
    }

    private static Transform CreatePoint(string name, Transform parent, Vector3 localPosition, int layer)
    {
        GameObject point = new(name);
        point.layer = layer;
        point.transform.SetParent(parent, false);
        point.transform.localPosition = localPosition;
        return point.transform;
    }

    private static GameObject InstantiateScenePrefab(
        GameObject prefab,
        string name,
        Vector3 position,
        Quaternion rotation)
    {
        GameObject instance = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
        instance.name = name;
        instance.transform.SetPositionAndRotation(position, rotation);
        return instance;
    }

    private static GameObject SavePrefab(GameObject root, string path)
    {
        GameObject prefab = PrefabUtility.SaveAsPrefabAsset(root, path);
        if (prefab == null)
        {
            throw new InvalidOperationException($"Failed to create prefab at {path}.");
        }

        return prefab;
    }

    private static Material CreateOrUpdateMaterial(
        string path,
        Color color,
        float metallic,
        float smoothness)
    {
        Material material = AssetDatabase.LoadAssetAtPath<Material>(path);
        if (material == null)
        {
            Shader shader = Shader.Find("Universal Render Pipeline/Lit") ?? Shader.Find("Standard");
            material = new Material(shader);
            AssetDatabase.CreateAsset(material, path);
        }

        if (material.HasProperty("_BaseColor"))
        {
            material.SetColor("_BaseColor", color);
        }

        material.color = color;
        if (material.HasProperty("_Metallic"))
        {
            material.SetFloat("_Metallic", metallic);
        }

        if (material.HasProperty("_Smoothness"))
        {
            material.SetFloat("_Smoothness", smoothness);
        }

        EditorUtility.SetDirty(material);
        return material;
    }

    private static void ConfigureInteractable(
        SerializedObject serialized,
        string displayName,
        string description,
        int priority,
        float range,
        Transform point)
    {
        SetString(serialized, "displayName", displayName);
        SetString(serialized, "description", description);
        SetInt(serialized, "priority", priority);
        SetFloat(serialized, "interactionRange", range);
        SetObject(serialized, "interactionPoint", point);
    }

    private static void ConfigureSeed(SerializedProperty seed, ItemDefinition definition, int quantity)
    {
        SerializedProperty definitionProperty = seed.FindPropertyRelative("definition");
        SerializedProperty quantityProperty = seed.FindPropertyRelative("quantity");
        if (definitionProperty == null || quantityProperty == null)
        {
            throw new InvalidOperationException("WorldItemContainer seed serialization changed.");
        }

        definitionProperty.objectReferenceValue = definition;
        quantityProperty.intValue = quantity;
    }

    private static int EnsureInteractionLayer()
    {
        int existing = LayerMask.NameToLayer(InteractionLayerName);
        if (existing >= 0)
        {
            return existing;
        }

        UnityEngine.Object tagManager = AssetDatabase.LoadAllAssetsAtPath("ProjectSettings/TagManager.asset")[0];
        SerializedObject serialized = new(tagManager);
        SerializedProperty layers = serialized.FindProperty("layers");
        SerializedProperty target = layers.GetArrayElementAtIndex(PreferredInteractionLayer);
        if (!string.IsNullOrEmpty(target.stringValue))
        {
            throw new InvalidOperationException(
                $"Layer {PreferredInteractionLayer} is already used by '{target.stringValue}'.");
        }

        target.stringValue = InteractionLayerName;
        serialized.ApplyModifiedPropertiesWithoutUndo();
        AssetDatabase.SaveAssets();
        return PreferredInteractionLayer;
    }

    private static void EnsureFolders()
    {
        EnsureFolder("Assets/_Project/Prefabs/World");
        EnsureFolder(MaterialFolder);
    }

    private static void EnsureFolder(string path)
    {
        if (AssetDatabase.IsValidFolder(path))
        {
            return;
        }

        string parent = path.Substring(0, path.LastIndexOf('/'));
        string name = path.Substring(path.LastIndexOf('/') + 1);
        EnsureFolder(parent);
        AssetDatabase.CreateFolder(parent, name);
    }

    private static void SetLayerRecursively(GameObject root, int layer)
    {
        foreach (Transform child in root.GetComponentsInChildren<Transform>(true))
        {
            child.gameObject.layer = layer;
        }
    }

    private static void Stretch(RectTransform rect, float left, float bottom, float right, float top)
    {
        rect.anchorMin = Vector2.zero;
        rect.anchorMax = Vector2.one;
        rect.pivot = new Vector2(0.5f, 0.5f);
        rect.offsetMin = new Vector2(left, bottom);
        rect.offsetMax = new Vector2(-right, -top);
        rect.localScale = Vector3.one;
    }

    private static void Anchor(
        RectTransform rect,
        Vector2 anchorMin,
        Vector2 anchorMax,
        Vector2 offsetMin,
        Vector2 offsetMax,
        Vector2? pivot = null)
    {
        rect.anchorMin = anchorMin;
        rect.anchorMax = anchorMax;
        rect.pivot = pivot ?? new Vector2(0.5f, 0.5f);
        rect.offsetMin = offsetMin;
        rect.offsetMax = offsetMax;
        rect.localScale = Vector3.one;
    }

    private static void SetAnchored(
        RectTransform rect,
        Vector2 anchorMin,
        Vector2 anchorMax,
        Vector2 anchoredPosition,
        Vector2 sizeDelta)
    {
        rect.anchorMin = anchorMin;
        rect.anchorMax = anchorMax;
        rect.pivot = new Vector2(0.5f, 0.5f);
        rect.anchoredPosition = anchoredPosition;
        rect.sizeDelta = sizeDelta;
        rect.localScale = Vector3.one;
    }

    private static SerializedProperty RequireProperty(SerializedObject serialized, string name)
    {
        SerializedProperty property = serialized.FindProperty(name);
        if (property == null)
        {
            throw new InvalidOperationException(
                $"Serialized field '{name}' was not found on {serialized.targetObject.GetType().Name}.");
        }

        return property;
    }

    private static void SetObject(SerializedObject serialized, string name, UnityEngine.Object value)
    {
        RequireProperty(serialized, name).objectReferenceValue = value;
    }

    private static void SetString(SerializedObject serialized, string name, string value)
    {
        RequireProperty(serialized, name).stringValue = value;
    }

    private static void SetInt(SerializedObject serialized, string name, int value)
    {
        RequireProperty(serialized, name).intValue = value;
    }

    private static void SetFloat(SerializedObject serialized, string name, float value)
    {
        RequireProperty(serialized, name).floatValue = value;
    }

    private static void SetBool(SerializedObject serialized, string name, bool value)
    {
        RequireProperty(serialized, name).boolValue = value;
    }

    private static void SetVector3(SerializedObject serialized, string name, Vector3 value)
    {
        RequireProperty(serialized, name).vector3Value = value;
    }
}
#endif
