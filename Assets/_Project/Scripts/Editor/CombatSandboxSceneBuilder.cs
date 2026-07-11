using System.IO;
using UnityEditor;
using UnityEditor.Animations;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

public static class CombatSandboxSceneBuilder
{
    private const string ProjectRoot = "Assets/_Project";
    private const string MaterialsPath = ProjectRoot + "/Materials";
    private const string AnimationsPath = ProjectRoot + "/Animations";
    private const string ScriptsPath = ProjectRoot + "/Scripts";
    private const string UiScriptsPath = ScriptsPath + "/UI";
    private const string DataPath = ProjectRoot + "/Data";
    private const string WeaponsDataPath = DataPath + "/Weapons";
    private const string PrefabsPath = ProjectRoot + "/Prefabs";
    private const string ItemPrefabsPath = PrefabsPath + "/Items";
    private const string ScenesPath = ProjectRoot + "/Scenes";
    private const string SettingsPath = ProjectRoot + "/Settings";
    private const string ScenePath = ScenesPath + "/CombatSandbox.unity";
    private const string BuilderVersion = "2026-07-08-ual-animation-v1";
    private const string BuilderVersionPath = SettingsPath + "/CombatSandboxBuilderVersion.txt";
    private const string PlayerModelPath = ProjectRoot + "/Art/Mixamo/ActionAdventurePack/Y Bot.fbx";
    private const string PlayerAnimatorPath = AnimationsPath + "/PlayerThirdPerson.controller";
    private const string Ual1AnimationPath = ProjectRoot + "/Art/AnimationLibraries/UAL1_Standard.fbx";
    private const string Ual2AnimationPath = ProjectRoot + "/Art/AnimationLibraries/UAL2_Standard.fbx";
    private const string BronzeSwordModelPath = ProjectRoot + "/Art/Props/FantasyPropsMegaKit/Sword_Bronze.fbx";

    [InitializeOnLoadMethod]
    private static void BuildOnceWhenMissing()
    {
        EditorApplication.delayCall += () =>
        {
            if (!File.Exists(ScenePath) || !File.Exists(BuilderVersionPath) || File.ReadAllText(BuilderVersionPath) != BuilderVersion)
            {
                BuildCombatSandbox();
            }
        };
    }

    [MenuItem("Tools/Combat Sandbox/Rebuild CombatSandbox Scene")]
    public static void BuildCombatSandbox()
    {
        EnsureFolder(ProjectRoot);
        EnsureFolder(AnimationsPath);
        EnsureFolder(MaterialsPath);
        EnsureFolder(ScriptsPath);
        EnsureFolder(UiScriptsPath);
        EnsureFolder(DataPath);
        EnsureFolder(WeaponsDataPath);
        EnsureFolder(PrefabsPath);
        EnsureFolder(ItemPrefabsPath);
        EnsureFolder(ScenesPath);
        EnsureFolder(SettingsPath);

        Material groundMaterial = GetOrCreateMaterial("Ground_Mat", new Color(0.34f, 0.45f, 0.36f));
        Material playerMaterial = GetOrCreateMaterial("PlayerPlaceholder_Mat", new Color(0.22f, 0.5f, 0.95f));
        Material dummyMaterial = GetOrCreateMaterial("TrainingDummy_Mat", new Color(0.9f, 0.6f, 0.25f));
        Material hitboxMaterial = GetOrCreateTransparentMaterial("HitboxPreview_Mat", new Color(1f, 0.15f, 0.08f, 0.28f));
        Material bronzeSwordMaterial = GetOrCreateMaterial("BronzeSword_Mat", new Color(0.72f, 0.48f, 0.25f));

        GameObject playerPrefab = CreatePlayerPrefab(playerMaterial, hitboxMaterial);
        GameObject dummyPrefab = CreateTrainingDummyPrefab(dummyMaterial);
        GameObject cameraRigPrefab = CreateCameraRigPrefab();
        GameObject bronzeSwordPickupPrefab = CreateBronzeSwordPickupPrefab(bronzeSwordMaterial);

        Scene scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);

        CreateGround(groundMaterial);
        CreateLighting();

        GameObject player = (GameObject)PrefabUtility.InstantiatePrefab(playerPrefab);
        player.name = "PlayerPlaceholder";
        player.transform.position = Vector3.zero;

        GameObject dummy = (GameObject)PrefabUtility.InstantiatePrefab(dummyPrefab);
        dummy.name = "TrainingDummy";
        dummy.transform.position = new Vector3(0f, 1f, 4f);

        if (bronzeSwordPickupPrefab != null)
        {
            GameObject swordPickup = (GameObject)PrefabUtility.InstantiatePrefab(bronzeSwordPickupPrefab);
            swordPickup.name = "BronzeSwordPickup";
            swordPickup.transform.position = new Vector3(-2f, 0.02f, 2f);
        }

        GameObject cameraRig = (GameObject)PrefabUtility.InstantiatePrefab(cameraRigPrefab);
        cameraRig.name = "MainCameraRig";
        Camera mainCamera = cameraRig.GetComponentInChildren<Camera>();
        ThirdPersonCameraController follow = cameraRig.GetComponent<ThirdPersonCameraController>();
        follow.Target = player.transform;

        PlayerCameraRelativeMovement movement = player.GetComponent<PlayerCameraRelativeMovement>();
        movement.CameraTransform = mainCamera.transform;
        CreateCombatHud(player.GetComponent<PlayerStamina>());

        EditorSceneManager.MarkSceneDirty(scene);
        EditorSceneManager.SaveScene(scene, ScenePath);
        File.WriteAllText(BuilderVersionPath, BuilderVersion);
        AssetDatabase.ImportAsset(BuilderVersionPath);
        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();

        Debug.Log("CombatSandbox scene built at " + ScenePath);
    }

    private static void EnsureFolder(string folderPath)
    {
        if (AssetDatabase.IsValidFolder(folderPath))
        {
            return;
        }

        string parent = Path.GetDirectoryName(folderPath).Replace("\\", "/");
        string folderName = Path.GetFileName(folderPath);
        EnsureFolder(parent);
        AssetDatabase.CreateFolder(parent, folderName);
    }

    private static Material GetOrCreateMaterial(string materialName, Color color)
    {
        string assetPath = MaterialsPath + "/" + materialName + ".mat";
        Material material = AssetDatabase.LoadAssetAtPath<Material>(assetPath);

        if (material == null)
        {
            material = new Material(Shader.Find("Universal Render Pipeline/Lit"));
            AssetDatabase.CreateAsset(material, assetPath);
        }

        material.color = color;
        EditorUtility.SetDirty(material);
        return material;
    }

    private static Material GetOrCreateTransparentMaterial(string materialName, Color color)
    {
        Material material = GetOrCreateMaterial(materialName, color);
        material.SetFloat("_Surface", 1f);
        material.SetFloat("_Blend", 0f);
        material.SetFloat("_AlphaClip", 0f);
        material.SetOverrideTag("RenderType", "Transparent");
        material.EnableKeyword("_SURFACE_TYPE_TRANSPARENT");
        material.renderQueue = (int)UnityEngine.Rendering.RenderQueue.Transparent;
        material.color = color;
        EditorUtility.SetDirty(material);
        return material;
    }

    private static GameObject CreatePlayerPrefab(Material playerMaterial, Material hitboxMaterial)
    {
        string prefabPath = PrefabsPath + "/PlayerPlaceholder.prefab";
        GameObject root = new GameObject("PlayerPlaceholder");
        root.name = "PlayerPlaceholder";
        root.transform.position = Vector3.zero;

        GameObject body = CreatePlayerVisual(playerMaterial);
        body.transform.SetParent(root.transform);
        body.transform.localPosition = new Vector3(0f, -0.08f, 0f);
        body.transform.localRotation = Quaternion.identity;

        CharacterController controller = root.AddComponent<CharacterController>();
        controller.height = 2.15f;
        controller.radius = 0.5f;
        controller.center = new Vector3(0f, 1.075f, 0f);
        controller.stepOffset = 0.35f;

        PlayerLockOnController lockOn = root.AddComponent<PlayerLockOnController>();
        PlayerStamina stamina = root.AddComponent<PlayerStamina>();
        PlayerCameraRelativeMovement movement = root.AddComponent<PlayerCameraRelativeMovement>();
        PlayerEquipment equipment = root.AddComponent<PlayerEquipment>();
        PlayerInventory inventory = root.AddComponent<PlayerInventory>();
        PlayerAnimationDriver animationDriver = root.AddComponent<PlayerAnimationDriver>();

        GameObject hitboxObject = GameObject.CreatePrimitive(PrimitiveType.Cube);
        hitboxObject.name = "MeleeHitbox";
        hitboxObject.transform.SetParent(root.transform);
        hitboxObject.transform.localPosition = new Vector3(0f, 1f, 1.2f);
        hitboxObject.transform.localRotation = Quaternion.identity;
        hitboxObject.transform.localScale = new Vector3(1.45f, 1.15f, 1.35f);

        Renderer hitboxRenderer = hitboxObject.GetComponent<Renderer>();
        hitboxRenderer.sharedMaterial = hitboxMaterial;

        BoxCollider hitboxCollider = hitboxObject.GetComponent<BoxCollider>();
        hitboxCollider.isTrigger = true;
        hitboxCollider.enabled = false;

        Rigidbody hitboxBody = hitboxObject.AddComponent<Rigidbody>();
        hitboxBody.isKinematic = true;
        hitboxBody.useGravity = false;

        MeleeHitbox hitbox = hitboxObject.AddComponent<MeleeHitbox>();
        BasicMeleeAttack attack = root.AddComponent<BasicMeleeAttack>();
        SerializedObject attackObject = new SerializedObject(attack);
        attackObject.FindProperty("hitbox").objectReferenceValue = hitbox;
        attackObject.FindProperty("movement").objectReferenceValue = movement;
        attackObject.FindProperty("lockOnController").objectReferenceValue = lockOn;
        attackObject.FindProperty("stamina").objectReferenceValue = stamina;
        attackObject.FindProperty("equipment").objectReferenceValue = equipment;
        attackObject.FindProperty("characterController").objectReferenceValue = controller;
        attackObject.FindProperty("weaponFirstAttackDuration").floatValue = 0.38f;
        attackObject.FindProperty("weaponSecondAttackDuration").floatValue = 0.5f;
        attackObject.FindProperty("weaponThirdAttackDuration").floatValue = 1.25f;
        attackObject.FindProperty("weaponComboRecoveryTime").floatValue = 0.16f;
        attackObject.FindProperty("weaponFinalRecoveryTime").floatValue = 0.85f;
        attackObject.FindProperty("weaponFirstAttackLungeDistance").floatValue = 0.28f;
        attackObject.FindProperty("weaponSecondAttackLungeDistance").floatValue = 0f;
        attackObject.FindProperty("weaponThirdAttackLungeDistance").floatValue = 0f;
        attackObject.FindProperty("weaponLungeDuration").floatValue = 0.12f;
        attackObject.ApplyModifiedPropertiesWithoutUndo();

        Animator animator = body.GetComponent<Animator>();
        SerializedObject equipmentObject = new SerializedObject(equipment);
        equipmentObject.FindProperty("animator").objectReferenceValue = animator;
        equipmentObject.FindProperty("weaponLocalEuler").vector3Value = new Vector3(-80f, 180f, 0f);
        equipmentObject.FindProperty("weaponLocalScale").vector3Value = Vector3.one * 100f;
        equipmentObject.ApplyModifiedPropertiesWithoutUndo();

        SerializedObject inventoryObject = new SerializedObject(inventory);
        inventoryObject.FindProperty("equipment").objectReferenceValue = equipment;
        inventoryObject.ApplyModifiedPropertiesWithoutUndo();

        SerializedObject movementObject = new SerializedObject(movement);
        movementObject.FindProperty("meleeAttack").objectReferenceValue = attack;
        movementObject.FindProperty("lockOnController").objectReferenceValue = lockOn;
        movementObject.FindProperty("stamina").objectReferenceValue = stamina;
        movementObject.ApplyModifiedPropertiesWithoutUndo();

        SerializedObject animationDriverObject = new SerializedObject(animationDriver);
        animationDriverObject.FindProperty("animator").objectReferenceValue = animator;
        animationDriverObject.FindProperty("characterController").objectReferenceValue = controller;
        animationDriverObject.FindProperty("movement").objectReferenceValue = movement;
        animationDriverObject.FindProperty("meleeAttack").objectReferenceValue = attack;
        animationDriverObject.FindProperty("equipment").objectReferenceValue = equipment;
        animationDriverObject.FindProperty("attackRecoveryLockTime").floatValue = 0.22f;
        animationDriverObject.FindProperty("weaponFinisherLockTime").floatValue = 1.05f;
        animationDriverObject.ApplyModifiedPropertiesWithoutUndo();

        GameObject prefab = PrefabUtility.SaveAsPrefabAsset(root, prefabPath);
        Object.DestroyImmediate(root);
        return prefab;
    }

    private static void CreateCombatHud(PlayerStamina stamina)
    {
        GameObject hud = new("CombatHUD");
        PlayerStaminaHud staminaHud = hud.AddComponent<PlayerStaminaHud>();
        SerializedObject hudObject = new SerializedObject(staminaHud);
        hudObject.FindProperty("stamina").objectReferenceValue = stamina;
        hudObject.FindProperty("inventory").objectReferenceValue = stamina != null ? stamina.GetComponent<PlayerInventory>() : null;
        hudObject.ApplyModifiedPropertiesWithoutUndo();
    }

    private static GameObject CreatePlayerVisual(Material fallbackMaterial)
    {
        GameObject modelAsset = AssetDatabase.LoadAssetAtPath<GameObject>(PlayerModelPath);
        if (modelAsset != null)
        {
            GameObject model = (GameObject)PrefabUtility.InstantiatePrefab(modelAsset);
            model.name = "Body";
            model.transform.localPosition = new Vector3(0f, -0.08f, 0f);
            model.transform.localScale = Vector3.one * 1.12f;

            foreach (Collider collider in model.GetComponentsInChildren<Collider>())
            {
                Object.DestroyImmediate(collider);
            }

            Animator animator = model.GetComponent<Animator>();
            if (animator != null)
            {
                animator.applyRootMotion = false;
                animator.runtimeAnimatorController = CreatePlayerAnimatorController();
            }

            return model;
        }

        GameObject fallback = GameObject.CreatePrimitive(PrimitiveType.Capsule);
        fallback.name = "Body";
        fallback.transform.localPosition = new Vector3(0f, 1f, 0f);
        fallback.transform.localScale = Vector3.one * 1.12f;
        Object.DestroyImmediate(fallback.GetComponent<CapsuleCollider>());
        fallback.GetComponent<Renderer>().sharedMaterial = fallbackMaterial;
        return fallback;
    }

    private static RuntimeAnimatorController CreatePlayerAnimatorController()
    {
        AnimationClip idleClip = LoadAnimationClip(Ual1AnimationPath, "Armature|Idle_Loop");
        AnimationClip walkClip = LoadAnimationClip(Ual1AnimationPath, "Armature|Walk_Loop");
        AnimationClip runClip = LoadAnimationClip(Ual1AnimationPath, "Armature|Sprint_Loop");
        AnimationClip dodgeClip = LoadAnimationClip(Ual1AnimationPath, "Armature|Roll");
        AnimationClip attackJabClip = LoadAnimationClip(Ual1AnimationPath, "Armature|Punch_Jab");
        AnimationClip attackCrossClip = LoadAnimationClip(Ual1AnimationPath, "Armature|Punch_Cross");
        AnimationClip swordAttackAClip = LoadAnimationClip(Ual2AnimationPath, "Armature|Sword_Regular_A");
        AnimationClip swordAttackARecoveryClip = LoadAnimationClip(Ual2AnimationPath, "Armature|Sword_Regular_A_Rec");
        AnimationClip swordAttackBClip = LoadAnimationClip(Ual2AnimationPath, "Armature|Sword_Regular_B");
        AnimationClip swordAttackBRecoveryClip = LoadAnimationClip(Ual2AnimationPath, "Armature|Sword_Regular_B_Rec");
        AnimationClip swordAttackCClip = LoadAnimationClip(Ual2AnimationPath, "Armature|Sword_Regular_C");

        if (File.Exists(PlayerAnimatorPath))
        {
            AssetDatabase.DeleteAsset(PlayerAnimatorPath);
        }

        AnimatorController controller = AnimatorController.CreateAnimatorControllerAtPath(PlayerAnimatorPath);
        controller.AddParameter("Speed", AnimatorControllerParameterType.Float);
        controller.AddParameter("Attack", AnimatorControllerParameterType.Trigger);
        controller.AddParameter("Dodge", AnimatorControllerParameterType.Trigger);

        AnimatorStateMachine stateMachine = controller.layers[0].stateMachine;
        AnimatorState idle = stateMachine.AddState("Idle");
        AnimatorState walk = stateMachine.AddState("Walk");
        AnimatorState run = stateMachine.AddState("Run");
        AnimatorState dodge = stateMachine.AddState("Dodge");
        AnimatorState attackJab = stateMachine.AddState("AttackJab");
        AnimatorState attackCross = stateMachine.AddState("AttackCross");
        AnimatorState swordAttackA = stateMachine.AddState("SwordAttackA");
        AnimatorState swordAttackARecovery = stateMachine.AddState("SwordAttackA_REC");
        AnimatorState swordAttackB = stateMachine.AddState("SwordAttackB");
        AnimatorState swordAttackBRecovery = stateMachine.AddState("SwordAttackB_REC");
        AnimatorState swordAttackC = stateMachine.AddState("SwordAttackC");

        idle.motion = idleClip;
        walk.motion = walkClip;
        run.motion = runClip;
        dodge.motion = dodgeClip;
        attackJab.motion = attackJabClip;
        attackCross.motion = attackCrossClip;
        swordAttackA.motion = swordAttackAClip;
        swordAttackARecovery.motion = swordAttackARecoveryClip;
        swordAttackB.motion = swordAttackBClip;
        swordAttackBRecovery.motion = swordAttackBRecoveryClip;
        swordAttackC.motion = swordAttackCClip;
        stateMachine.defaultState = idle;

        AnimatorStateTransition idleToWalk = idle.AddTransition(walk);
        idleToWalk.hasExitTime = false;
        idleToWalk.duration = 0.1f;
        idleToWalk.AddCondition(AnimatorConditionMode.Greater, 0.1f, "Speed");

        AnimatorStateTransition walkToIdle = walk.AddTransition(idle);
        walkToIdle.hasExitTime = false;
        walkToIdle.duration = 0.1f;
        walkToIdle.AddCondition(AnimatorConditionMode.Less, 0.1f, "Speed");

        AnimatorStateTransition anyToDodge = stateMachine.AddAnyStateTransition(dodge);
        anyToDodge.hasExitTime = false;
        anyToDodge.duration = 0.05f;
        anyToDodge.AddCondition(AnimatorConditionMode.If, 0f, "Dodge");

        AnimatorStateTransition dodgeToIdle = dodge.AddTransition(idle);
        dodgeToIdle.hasExitTime = true;
        dodgeToIdle.exitTime = 0.85f;
        dodgeToIdle.duration = 0.1f;

        AnimatorStateTransition anyToAttack = stateMachine.AddAnyStateTransition(attackJab);
        anyToAttack.hasExitTime = false;
        anyToAttack.duration = 0.05f;
        anyToAttack.AddCondition(AnimatorConditionMode.If, 0f, "Attack");

        AnimatorStateTransition attackJabToIdle = attackJab.AddTransition(idle);
        attackJabToIdle.hasExitTime = true;
        attackJabToIdle.exitTime = 0.85f;
        attackJabToIdle.duration = 0.1f;

        AnimatorStateTransition attackCrossToIdle = attackCross.AddTransition(idle);
        attackCrossToIdle.hasExitTime = true;
        attackCrossToIdle.exitTime = 0.85f;
        attackCrossToIdle.duration = 0.1f;

        EditorUtility.SetDirty(controller);
        AssetDatabase.SaveAssets();
        return controller;
    }

    private static AnimationClip LoadAnimationClip(string path, string clipName)
    {
        foreach (Object asset in AssetDatabase.LoadAllAssetsAtPath(path))
        {
            if (asset is AnimationClip clip && clip.name == clipName)
            {
                return clip;
            }
        }

        return AssetDatabase.LoadAssetAtPath<AnimationClip>(path);
    }

    private static GameObject CreateBronzeSwordPickupPrefab(Material bronzeSwordMaterial)
    {
        GameObject swordModel = AssetDatabase.LoadAssetAtPath<GameObject>(BronzeSwordModelPath);
        if (swordModel == null)
        {
            return null;
        }

        GameObject equippedPrefab = CreateBronzeSwordEquippedPrefab(swordModel, bronzeSwordMaterial);
        WeaponData weaponData = CreateBronzeSwordData(equippedPrefab);
        string prefabPath = ItemPrefabsPath + "/BronzeSwordPickup.prefab";

        GameObject root = new GameObject("BronzeSwordPickup");
        SphereCollider trigger = root.AddComponent<SphereCollider>();
        trigger.isTrigger = true;
        trigger.radius = 1.2f;

        Rigidbody body = root.AddComponent<Rigidbody>();
        body.isKinematic = true;
        body.useGravity = false;

        GameObject visual = (GameObject)PrefabUtility.InstantiatePrefab(equippedPrefab);
        visual.name = "Visual";
        visual.transform.SetParent(root.transform);
        visual.transform.localPosition = new Vector3(0f, 0.04f, 0f);
        visual.transform.localRotation = Quaternion.Euler(0f, 35f, 0f);
        visual.transform.localScale = Vector3.one * 100f;

        WeaponPickup pickup = root.AddComponent<WeaponPickup>();
        SerializedObject pickupObject = new SerializedObject(pickup);
        pickupObject.FindProperty("weaponData").objectReferenceValue = weaponData;
        pickupObject.FindProperty("equippedWeaponPrefab").objectReferenceValue = equippedPrefab;
        pickupObject.FindProperty("weaponName").stringValue = "Bronze Sword";
        pickupObject.FindProperty("visualRoot").objectReferenceValue = visual.transform;
        pickupObject.ApplyModifiedPropertiesWithoutUndo();

        GameObject prefab = PrefabUtility.SaveAsPrefabAsset(root, prefabPath);
        Object.DestroyImmediate(root);
        return prefab;
    }

    private static WeaponData CreateBronzeSwordData(GameObject equippedPrefab)
    {
        string assetPath = WeaponsDataPath + "/BronzeSword.asset";
        WeaponData weaponData = AssetDatabase.LoadAssetAtPath<WeaponData>(assetPath);
        if (weaponData == null)
        {
            weaponData = ScriptableObject.CreateInstance<WeaponData>();
            AssetDatabase.CreateAsset(weaponData, assetPath);
        }

        weaponData.displayName = "Bronze Sword";
        weaponData.equippedPrefab = equippedPrefab;
        weaponData.maxComboSteps = 3;
        weaponData.comboRecoveryTime = 0.16f;
        weaponData.finalRecoveryTime = 0.85f;
        weaponData.lungeDuration = 0.12f;
        weaponData.attacks = new[]
        {
            new WeaponAttackData
            {
                duration = 0.38f,
                hitMoment = 0.74f,
                staminaCost = 14f,
                damage = 28,
                poiseDamage = 60f,
                knockbackDistance = 0.24f,
                impactWeight = CombatImpactWeight.Medium,
                lungeDistance = 0.28f,
                hitboxScaleMultiplier = Vector3.one,
                hitboxLocalOffset = Vector3.zero
            },
            new WeaponAttackData
            {
                duration = 0.5f,
                hitMoment = 0.76f,
                staminaCost = 14f,
                damage = 38,
                poiseDamage = 85f,
                knockbackDistance = 0.42f,
                impactWeight = CombatImpactWeight.Medium,
                lungeDistance = 0.55f,
                hitboxScaleMultiplier = Vector3.one,
                hitboxLocalOffset = Vector3.zero
            },
            new WeaponAttackData
            {
                duration = 0.6f,
                hitMoment = 0.95f,
                staminaCost = 24f,
                damage = 46,
                poiseDamage = 105f,
                knockbackDistance = 0.52f,
                impactWeight = CombatImpactWeight.Heavy,
                lungeDistance = 2f,
                hitboxScaleMultiplier = new Vector3(1.85f, 1f, 1.18f),
                hitboxLocalOffset = new Vector3(0f, 0f, 0.18f)
            }
        };

        EditorUtility.SetDirty(weaponData);
        return weaponData;
    }

    private static GameObject CreateBronzeSwordEquippedPrefab(GameObject swordModel, Material bronzeSwordMaterial)
    {
        string prefabPath = ItemPrefabsPath + "/BronzeSword_Equipped.prefab";
        GameObject sword = (GameObject)PrefabUtility.InstantiatePrefab(swordModel);
        sword.name = "BronzeSword_Equipped";
        AssignMaterial(sword, bronzeSwordMaterial);
        RemovePhysics(sword);

        GameObject prefab = PrefabUtility.SaveAsPrefabAsset(sword, prefabPath);
        Object.DestroyImmediate(sword);
        return prefab;
    }

    private static void AssignMaterial(GameObject root, Material material)
    {
        foreach (Renderer renderer in root.GetComponentsInChildren<Renderer>())
        {
            renderer.sharedMaterial = material;
        }
    }

    private static void RemovePhysics(GameObject root)
    {
        foreach (Collider collider in root.GetComponentsInChildren<Collider>())
        {
            Object.DestroyImmediate(collider);
        }

        foreach (Rigidbody body in root.GetComponentsInChildren<Rigidbody>())
        {
            Object.DestroyImmediate(body);
        }
    }

    private static GameObject CreateTrainingDummyPrefab(Material dummyMaterial)
    {
        string prefabPath = PrefabsPath + "/TrainingDummy.prefab";
        GameObject root = GameObject.CreatePrimitive(PrimitiveType.Capsule);
        root.name = "TrainingDummy";
        root.transform.position = Vector3.zero;

        Renderer renderer = root.GetComponent<Renderer>();
        renderer.sharedMaterial = dummyMaterial;

        Rigidbody body = root.AddComponent<Rigidbody>();
        body.isKinematic = true;
        body.useGravity = false;

        TrainingDummyHealth health = root.AddComponent<TrainingDummyHealth>();
        SerializedObject healthObject = new SerializedObject(health);
        healthObject.FindProperty("bodyRenderer").objectReferenceValue = renderer;
        healthObject.ApplyModifiedPropertiesWithoutUndo();

        GameObject prefab = PrefabUtility.SaveAsPrefabAsset(root, prefabPath);
        Object.DestroyImmediate(root);
        return prefab;
    }

    private static GameObject CreateCameraRigPrefab()
    {
        string prefabPath = PrefabsPath + "/MainCameraRig.prefab";
        GameObject root = new GameObject("MainCameraRig");
        GameObject cameraObject = new GameObject("Main Camera");
        cameraObject.transform.SetParent(root.transform);
        cameraObject.transform.position = new Vector3(0f, 3.1f, -5.5f);
        cameraObject.transform.rotation = Quaternion.Euler(18f, 0f, 0f);

        Camera camera = cameraObject.AddComponent<Camera>();
        camera.orthographic = false;
        camera.fieldOfView = 60f;
        camera.nearClipPlane = 0.1f;
        camera.farClipPlane = 200f;
        cameraObject.tag = "MainCamera";
        ThirdPersonCameraController controller = root.AddComponent<ThirdPersonCameraController>();
        SerializedObject controllerObject = new SerializedObject(controller);
        controllerObject.FindProperty("controlledCamera").objectReferenceValue = camera;
        controllerObject.FindProperty("rightOffset").floatValue = 0.65f;
        controllerObject.ApplyModifiedPropertiesWithoutUndo();

        GameObject prefab = PrefabUtility.SaveAsPrefabAsset(root, prefabPath);
        Object.DestroyImmediate(root);
        return prefab;
    }

    private static void CreateGround(Material groundMaterial)
    {
        GameObject ground = GameObject.CreatePrimitive(PrimitiveType.Cube);
        ground.name = "Ground";
        ground.transform.position = new Vector3(0f, -0.05f, 0f);
        ground.transform.localScale = new Vector3(48f, 0.1f, 48f);
        ground.GetComponent<Renderer>().sharedMaterial = groundMaterial;
    }

    private static void CreateLighting()
    {
        RenderSettings.ambientMode = UnityEngine.Rendering.AmbientMode.Flat;
        RenderSettings.ambientLight = new Color(0.42f, 0.45f, 0.48f);

        GameObject lightObject = new GameObject("Directional Light");
        Light light = lightObject.AddComponent<Light>();
        light.type = LightType.Directional;
        light.intensity = 1.4f;
        lightObject.transform.rotation = Quaternion.Euler(50f, -35f, 0f);
    }
}
