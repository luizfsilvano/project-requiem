#if UNITY_EDITOR
using UnityEditor;
using UnityEngine;

public static class EnemyCombatTuningAssetBootstrap
{
    private const string AssetPath = "Assets/_Project/Settings/EnemyCombatTuning.asset";

    [InitializeOnLoadMethod]
    private static void EnsureAssetExists()
    {
        if (AssetDatabase.LoadAssetAtPath<EnemyCombatTuning>(AssetPath) != null)
        {
            return;
        }

        EnemyCombatTuning tuning = ScriptableObject.CreateInstance<EnemyCombatTuning>();
        AssetDatabase.CreateAsset(tuning, AssetPath);
        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();
    }
}
#endif
