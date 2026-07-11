using UnityEngine;

public abstract class ItemDefinition : ScriptableObject
{
    [Header("Item Definition")]
    [SerializeField] private string definitionId;
    [SerializeField, Min(1)] private int maxStackSize = 1;
    [SerializeField, Min(0f)] private float defaultMaxDurability = 100f;

    public string DefinitionId => string.IsNullOrWhiteSpace(definitionId) ? name : definitionId;
    public abstract string DisplayName { get; }
    public int MaxStackSize => Mathf.Max(1, maxStackSize);
    public float DefaultMaxDurability => Mathf.Max(0f, defaultMaxDurability);

    protected virtual void OnValidate()
    {
        maxStackSize = Mathf.Max(1, maxStackSize);
        defaultMaxDurability = Mathf.Max(0f, defaultMaxDurability);
    }
}
