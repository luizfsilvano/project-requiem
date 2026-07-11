using UnityEngine;
using UnityEngine.UI;

public sealed class PlayerStaminaHud : MonoBehaviour
{
    [SerializeField] private PlayerStamina stamina;
    [SerializeField] private PlayerInventory inventory;
    [SerializeField] private Text staminaText;
    [SerializeField] private Text weaponText;
    [SerializeField] private Vector2 anchoredPosition = new(18f, -18f);
    [SerializeField] private Vector2 weaponAnchoredPosition = new(18f, -44f);

    private void Awake()
    {
        if (stamina == null)
        {
            stamina = FindFirstObjectByType<PlayerStamina>();
        }

        if (inventory == null)
        {
            inventory = FindFirstObjectByType<PlayerInventory>();
        }

        if (staminaText == null)
        {
            staminaText = CreateText("StaminaText", anchoredPosition);
        }

        if (weaponText == null)
        {
            weaponText = CreateText("WeaponText", weaponAnchoredPosition);
        }

        Refresh();
    }

    private void OnEnable()
    {
        if (stamina != null)
        {
            stamina.StaminaChanged += Refresh;
        }

        if (inventory != null)
        {
            inventory.InventoryChanged += Refresh;
        }
    }

    private void OnDisable()
    {
        if (stamina != null)
        {
            stamina.StaminaChanged -= Refresh;
        }

        if (inventory != null)
        {
            inventory.InventoryChanged -= Refresh;
        }
    }

    private void Update()
    {
        Refresh();
    }

    private void Refresh()
    {
        if (stamina != null && staminaText != null)
        {
            staminaText.text = $"Stamina: {Mathf.CeilToInt(stamina.CurrentStamina)}/{Mathf.CeilToInt(stamina.MaxStamina)}";
        }

        if (inventory != null && weaponText != null)
        {
            ItemInstance equippedItem = inventory.EquippedItem;
            string durability = equippedItem != null
                ? $"{equippedItem.CurrentDurability:0}/{equippedItem.MaxDurability:0}"
                : "-";
            weaponText.text = $"Active {inventory.ActiveWeaponSlot}: {inventory.EquippedWeaponName} | 1: {inventory.PrimaryWeaponName} | 2: {inventory.SecondaryWeaponName} | Durability: {durability}";
        }
    }

    private Text CreateText(string objectName, Vector2 position)
    {
        Canvas canvas = GetComponentInParent<Canvas>();
        if (canvas == null)
        {
            canvas = gameObject.AddComponent<Canvas>();
            canvas.renderMode = RenderMode.ScreenSpaceOverlay;
            canvas.sortingOrder = 0;
            gameObject.AddComponent<CanvasScaler>();
            gameObject.AddComponent<GraphicRaycaster>();
        }

        GameObject textObject = new(objectName);
        textObject.transform.SetParent(canvas.transform, false);

        Text text = textObject.AddComponent<Text>();
        text.font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        text.fontSize = 18;
        text.alignment = TextAnchor.UpperLeft;
        text.color = new Color(0.86f, 0.95f, 1f, 1f);
        text.raycastTarget = false;

        RectTransform rect = text.GetComponent<RectTransform>();
        rect.anchorMin = new Vector2(0f, 1f);
        rect.anchorMax = new Vector2(0f, 1f);
        rect.pivot = new Vector2(0f, 1f);
        rect.anchoredPosition = position;
        rect.sizeDelta = new Vector2(960f, 32f);

        return text;
    }
}
