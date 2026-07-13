using UnityEngine;

[CreateAssetMenu(fileName = "GenericItem", menuName = "Combat Sandbox/Items/Generic Item")]
public sealed class GenericItemDefinition : ItemDefinition
{
    [SerializeField] private string displayName = "Item";

    public override string DisplayName => string.IsNullOrWhiteSpace(displayName)
        ? "Item"
        : displayName.Trim();

    protected override void OnValidate()
    {
        base.OnValidate();
        displayName = displayName != null ? displayName.Trim() : string.Empty;
    }
}
