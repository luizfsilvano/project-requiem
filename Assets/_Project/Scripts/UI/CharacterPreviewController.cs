using UnityEngine;

public sealed class CharacterPreviewController : MonoBehaviour
{
    [SerializeField] private Camera previewCamera;
    [SerializeField] private Transform characterPivot;
    [SerializeField] private Animator previewAnimator;
    [SerializeField] private PlayerEquipment playerEquipment;
    [SerializeField] private RenderTexture renderTexture;
    [SerializeField] private int previewLayer = 30;
    [SerializeField] private float cameraHeight = 1.12f;
    [SerializeField] private float minCameraDistance = 2.9f;
    [SerializeField] private float maxCameraDistance = 5.2f;
    [SerializeField] private float cameraDistance = 4.05f;

    private Transform weaponSocket;
    private GameObject previewWeapon;
    private bool subscribed;

    public RenderTexture RenderTexture => renderTexture;

    private void Awake()
    {
        if (playerEquipment == null)
        {
            playerEquipment = FindFirstObjectByType<PlayerEquipment>();
        }

        if (previewCamera != null)
        {
            previewCamera.targetTexture = renderTexture;
            previewCamera.enabled = false;
        }

        EnsureWeaponSocket();
        ApplyCameraPose();
        SetLayerRecursively(gameObject, previewLayer);
    }

    private void OnEnable()
    {
        Subscribe();
        RefreshWeapon();
    }

    private void OnDisable()
    {
        Unsubscribe();
    }

    private void OnDestroy()
    {
        Unsubscribe();
    }

    public void SetVisible(bool visible)
    {
        if (previewCamera != null)
        {
            previewCamera.enabled = visible;
        }

        if (characterPivot != null)
        {
            characterPivot.gameObject.SetActive(visible);
        }

        if (visible)
        {
            if (playerEquipment == null)
            {
                playerEquipment = FindFirstObjectByType<PlayerEquipment>();
                Subscribe();
            }

            RefreshWeapon();
        }
    }

    public void Rotate(float yawDelta)
    {
        if (characterPivot != null)
        {
            characterPivot.Rotate(Vector3.up, yawDelta, Space.Self);
        }
    }

    public void Zoom(float distanceDelta)
    {
        cameraDistance = Mathf.Clamp(cameraDistance + distanceDelta, minCameraDistance, maxCameraDistance);
        ApplyCameraPose();
    }

    public void RefreshWeapon()
    {
        if (previewWeapon != null)
        {
            Destroy(previewWeapon);
            previewWeapon = null;
        }

        EnsureWeaponSocket();
        WeaponData weaponData = playerEquipment != null ? playerEquipment.EquippedWeapon : null;
        if (weaponData == null || weaponData.equippedPrefab == null || weaponSocket == null)
        {
            return;
        }

        previewWeapon = Instantiate(weaponData.equippedPrefab, weaponSocket);
        previewWeapon.name = $"{weaponData.DisplayName}_Preview";
        if (playerEquipment.TryGetEquippedWeaponPose(
                out Vector3 localPosition,
                out Vector3 localEuler,
                out Vector3 localScale))
        {
            previewWeapon.transform.localPosition = localPosition;
            previewWeapon.transform.localRotation = Quaternion.Euler(localEuler);
            previewWeapon.transform.localScale = localScale;
        }

        DisablePhysics(previewWeapon);
        SetLayerRecursively(previewWeapon, previewLayer);
    }

    private void Subscribe()
    {
        if (!subscribed && playerEquipment != null)
        {
            playerEquipment.ActiveWeaponChanged += HandleActiveWeaponChanged;
            subscribed = true;
        }
    }

    private void Unsubscribe()
    {
        if (subscribed && playerEquipment != null)
        {
            playerEquipment.ActiveWeaponChanged -= HandleActiveWeaponChanged;
        }

        subscribed = false;
    }

    private void HandleActiveWeaponChanged(ItemInstance item)
    {
        RefreshWeapon();
    }

    private void EnsureWeaponSocket()
    {
        if (weaponSocket != null)
        {
            return;
        }

        Transform rightHand = previewAnimator != null
            ? previewAnimator.GetBoneTransform(HumanBodyBones.RightHand)
            : null;
        if (rightHand == null)
        {
            return;
        }

        GameObject socketObject = new("PreviewWeaponSocket");
        socketObject.transform.SetParent(rightHand, false);
        weaponSocket = socketObject.transform;
        SetLayerRecursively(socketObject, previewLayer);
    }

    private void ApplyCameraPose()
    {
        if (previewCamera == null)
        {
            return;
        }

        previewCamera.transform.localPosition = new Vector3(0f, cameraHeight, cameraDistance);
        previewCamera.transform.localRotation = Quaternion.Euler(0f, 180f, 0f);
    }

    private static void DisablePhysics(GameObject root)
    {
        foreach (Collider collider in root.GetComponentsInChildren<Collider>(true))
        {
            collider.enabled = false;
        }

        foreach (Rigidbody body in root.GetComponentsInChildren<Rigidbody>(true))
        {
            body.isKinematic = true;
            body.useGravity = false;
            body.detectCollisions = false;
        }
    }

    private static void SetLayerRecursively(GameObject root, int layer)
    {
        if (root == null || layer < 0 || layer > 31)
        {
            return;
        }

        foreach (Transform child in root.GetComponentsInChildren<Transform>(true))
        {
            child.gameObject.layer = layer;
        }
    }
}
