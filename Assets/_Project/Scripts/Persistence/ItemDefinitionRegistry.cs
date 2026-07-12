using System;
using System.Collections.Generic;
using UnityEngine;

[CreateAssetMenu(fileName = "ItemDefinitionRegistry", menuName = "Combat Sandbox/Items/Definition Registry")]
public sealed class ItemDefinitionRegistry : ScriptableObject
{
    [SerializeField] private List<ItemDefinition> definitions = new();

    [NonSerialized] private Dictionary<string, ItemDefinition> definitionsById;
    [NonSerialized] private string cacheError;

    public IReadOnlyList<ItemDefinition> Definitions => definitions;
    public int Count => definitions != null ? definitions.Count : 0;

    public bool TryResolve(string definitionId, out ItemDefinition definition)
    {
        definition = null;
        return EnsureCache(out _)
            && !string.IsNullOrWhiteSpace(definitionId)
            && definitionsById.TryGetValue(definitionId, out definition);
    }

    public bool Validate(out string error)
    {
        definitionsById = null;
        cacheError = string.Empty;
        return EnsureCache(out error);
    }

    private bool EnsureCache(out string error)
    {
        if (definitionsById != null)
        {
            error = cacheError ?? string.Empty;
            return string.IsNullOrEmpty(error);
        }

        definitionsById = new Dictionary<string, ItemDefinition>(StringComparer.Ordinal);
        if (definitions == null)
        {
            cacheError = "Item definition registry list is null.";
            error = cacheError;
            return false;
        }

        for (int i = 0; i < definitions.Count; i++)
        {
            ItemDefinition definition = definitions[i];
            if (definition == null)
            {
                cacheError = $"Item definition registry entry {i} is missing.";
                error = cacheError;
                return false;
            }

            string definitionId = definition.DefinitionId;
            if (string.IsNullOrWhiteSpace(definitionId))
            {
                cacheError = $"Item definition '{definition.name}' has an empty definitionId.";
                error = cacheError;
                return false;
            }

            if (!definitionsById.TryAdd(definitionId, definition))
            {
                cacheError = $"Duplicate item definitionId '{definitionId}'.";
                error = cacheError;
                return false;
            }
        }

        cacheError = string.Empty;
        error = string.Empty;
        return true;
    }

    private void OnEnable()
    {
        definitionsById = null;
        cacheError = string.Empty;
    }

    private void OnValidate()
    {
        definitions ??= new List<ItemDefinition>();
        definitionsById = null;
        cacheError = string.Empty;
    }
}
