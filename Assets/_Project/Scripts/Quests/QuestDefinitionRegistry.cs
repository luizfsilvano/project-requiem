using System;
using System.Collections.Generic;
using UnityEngine;

[CreateAssetMenu(fileName = "QuestDefinitionRegistry", menuName = "Combat Sandbox/Quests/Definition Registry")]
public sealed class QuestDefinitionRegistry : ScriptableObject
{
    [SerializeField] private List<QuestDefinition> definitions = new();

    [NonSerialized] private Dictionary<string, QuestDefinition> definitionsById;
    [NonSerialized] private string cacheError;

    public IReadOnlyList<QuestDefinition> Definitions => definitions;
    public int Count => definitions != null ? definitions.Count : 0;

    public bool TryResolve(string questId, out QuestDefinition definition)
    {
        definition = null;
        return EnsureCache(out _)
            && !string.IsNullOrWhiteSpace(questId)
            && definitionsById.TryGetValue(questId, out definition);
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

        definitionsById = new Dictionary<string, QuestDefinition>(StringComparer.Ordinal);
        if (definitions == null)
        {
            cacheError = "Quest definition registry list is null.";
            error = cacheError;
            return false;
        }

        for (int i = 0; i < definitions.Count; i++)
        {
            QuestDefinition definition = definitions[i];
            if (definition == null)
            {
                cacheError = $"Quest definition registry entry {i} is missing.";
                error = cacheError;
                return false;
            }

            if (!definition.Validate(out string definitionError))
            {
                cacheError = definitionError;
                error = cacheError;
                return false;
            }

            if (!definitionsById.TryAdd(definition.QuestId, definition))
            {
                cacheError = $"Duplicate questId '{definition.QuestId}'.";
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
        definitions ??= new List<QuestDefinition>();
        definitionsById = null;
        cacheError = string.Empty;
    }
}
