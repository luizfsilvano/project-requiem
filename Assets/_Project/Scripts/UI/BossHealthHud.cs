using UnityEngine;
using UnityEngine.UI;

public sealed class BossHealthHud : MonoBehaviour
{
    [SerializeField] private TrainingDummyHealth health;
    [SerializeField] private string bossName = "STONE GOLEM";
    [SerializeField] private Vector2 barSize = new(620f, 26f);
    [SerializeField] private float bottomOffset = 52f;
    [SerializeField] private Color fillColor = new(0.62f, 0.09f, 0.06f, 1f);
    [SerializeField] private Color backgroundColor = new(0.035f, 0.035f, 0.035f, 0.94f);
    [SerializeField] private Color labelColor = new(0.92f, 0.9f, 0.82f, 1f);

    private GameObject canvasObject;
    private RectTransform hudRoot;
    private Image healthFill;
    private RectTransform healthFillRect;
    private Text healthLabel;
    private int displayedHealth = int.MinValue;

    private void Awake()
    {
        if (health == null)
        {
            health = GetComponent<TrainingDummyHealth>();
        }

        CreateHud();
        Refresh();
    }

    private void Update()
    {
        Refresh();
    }

    public void Configure(TrainingDummyHealth bossHealth, string displayName)
    {
        health = bossHealth;
        bossName = displayName;
        displayedHealth = int.MinValue;
    }

    private void CreateHud()
    {
        if (canvasObject != null)
        {
            return;
        }

        canvasObject = new GameObject($"{bossName}_BossHud");
        canvasObject.transform.SetParent(transform, false);

        Canvas canvas = canvasObject.AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 20;

        CanvasScaler scaler = canvasObject.AddComponent<CanvasScaler>();
        scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
        scaler.referenceResolution = new Vector2(1920f, 1080f);
        scaler.matchWidthOrHeight = 1f;

        GameObject rootObject = new("BossBarRoot");
        rootObject.transform.SetParent(canvasObject.transform, false);
        hudRoot = rootObject.AddComponent<RectTransform>();
        hudRoot.anchorMin = new Vector2(0.5f, 0f);
        hudRoot.anchorMax = new Vector2(0.5f, 0f);
        hudRoot.pivot = new Vector2(0.5f, 0f);
        hudRoot.anchoredPosition = new Vector2(0f, bottomOffset);
        hudRoot.sizeDelta = new Vector2(barSize.x, barSize.y + 30f);

        healthLabel = CreateLabel(hudRoot);
        CreateBar(hudRoot);
    }

    private Text CreateLabel(RectTransform parent)
    {
        GameObject labelObject = new("BossName");
        labelObject.transform.SetParent(parent, false);

        Text label = labelObject.AddComponent<Text>();
        label.font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        label.fontSize = 20;
        label.fontStyle = FontStyle.Bold;
        label.alignment = TextAnchor.MiddleCenter;
        label.color = labelColor;
        label.raycastTarget = false;

        RectTransform rect = label.rectTransform;
        rect.anchorMin = new Vector2(0f, 1f);
        rect.anchorMax = new Vector2(1f, 1f);
        rect.pivot = new Vector2(0.5f, 1f);
        rect.anchoredPosition = Vector2.zero;
        rect.sizeDelta = new Vector2(0f, 28f);
        return label;
    }

    private void CreateBar(RectTransform parent)
    {
        GameObject backgroundObject = new("HealthBackground");
        backgroundObject.transform.SetParent(parent, false);
        Image background = backgroundObject.AddComponent<Image>();
        background.color = backgroundColor;
        background.raycastTarget = false;

        RectTransform backgroundRect = background.rectTransform;
        backgroundRect.anchorMin = new Vector2(0.5f, 0f);
        backgroundRect.anchorMax = new Vector2(0.5f, 0f);
        backgroundRect.pivot = new Vector2(0.5f, 0f);
        backgroundRect.anchoredPosition = Vector2.zero;
        backgroundRect.sizeDelta = barSize;

        GameObject fillObject = new("HealthFill");
        fillObject.transform.SetParent(backgroundRect, false);
        healthFill = fillObject.AddComponent<Image>();
        healthFill.color = fillColor;
        healthFill.type = Image.Type.Simple;
        healthFill.raycastTarget = false;

        healthFillRect = healthFill.rectTransform;
        healthFillRect.anchorMin = new Vector2(0f, 0.5f);
        healthFillRect.anchorMax = new Vector2(0f, 0.5f);
        healthFillRect.pivot = new Vector2(0f, 0.5f);
        healthFillRect.anchoredPosition = new Vector2(3f, 0f);
        healthFillRect.sizeDelta = new Vector2(barSize.x - 6f, barSize.y - 6f);
    }

    private void Refresh()
    {
        if (hudRoot == null)
        {
            return;
        }

        bool visible = health != null && !health.IsDead;
        if (hudRoot.gameObject.activeSelf != visible)
        {
            hudRoot.gameObject.SetActive(visible);
        }

        if (!visible)
        {
            return;
        }

        if (healthFill != null && healthFillRect != null)
        {
            float availableWidth = Mathf.Max(0f, barSize.x - 6f);
            healthFillRect.sizeDelta = new Vector2(availableWidth * health.HealthRatio, barSize.y - 6f);
        }

        if (healthLabel != null && displayedHealth != health.CurrentHealth)
        {
            healthLabel.text = $"{bossName}   {health.CurrentHealth}/{health.MaxHealth}";
            displayedHealth = health.CurrentHealth;
        }
    }
}
