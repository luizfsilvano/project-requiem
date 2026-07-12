using UnityEngine;

[DisallowMultipleComponent]
public sealed class PersistentWorldId : MonoBehaviour
{
    [SerializeField] private string worldId;

    public string WorldId => worldId != null ? worldId.Trim() : string.Empty;
    public bool HasValidId => !string.IsNullOrWhiteSpace(WorldId);

    private void OnValidate()
    {
        worldId = worldId != null ? worldId.Trim() : string.Empty;
    }
}
