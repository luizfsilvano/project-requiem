using UnityEngine;
using UnityEngine.UI;

public enum InventoryFilterType
{
    All,
    Weapons,
    Armor,
    Consumables,
    Materials,
    Tools,
    Other
}

[RequireComponent(typeof(Button))]
public sealed class InventoryFilterTabView : MonoBehaviour
{
    [SerializeField] private InventoryFilterType filterType;
    [SerializeField] private Image background;
    [SerializeField] private Text label;

    private CharacterInventoryScreen screen;
    private Button button;

    public InventoryFilterType FilterType => filterType;

    private void Awake()
    {
        button = GetComponent<Button>();
        screen = GetComponentInParent<CharacterInventoryScreen>(true);
        button.onClick.AddListener(HandleClicked);
    }

    private void OnDestroy()
    {
        if (button != null)
        {
            button.onClick.RemoveListener(HandleClicked);
        }
    }

    public void SetSelected(bool selected)
    {
        if (background != null)
        {
            background.color = selected
                ? new Color(0.32f, 0.22f, 0.11f, 1f)
                : new Color(0.075f, 0.07f, 0.065f, 0.98f);
        }

        if (label != null)
        {
            label.color = selected
                ? new Color(0.98f, 0.84f, 0.52f, 1f)
                : new Color(0.72f, 0.7f, 0.64f, 1f);
        }
    }

    private void HandleClicked()
    {
        screen?.SetFilter(filterType);
    }
}
