using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(PlayerEquipment))]
public sealed class PlayerEquipmentEditor : Editor
{
    public override void OnInspectorGUI()
    {
        DrawDefaultInspector();

        EditorGUILayout.Space();
        using (new EditorGUI.DisabledScope(!Application.isPlaying))
        {
            if (GUILayout.Button("Save Current Equipped Weapon Pose"))
            {
                SaveCurrentEquippedWeaponPose((PlayerEquipment)target);
            }
        }

        if (!Application.isPlaying)
        {
            EditorGUILayout.HelpBox("Enter Play Mode, equip the weapon, adjust it under the hand, then use this button before exiting Play Mode.", MessageType.Info);
        }
    }

    private static void SaveCurrentEquippedWeaponPose(PlayerEquipment equipment)
    {
        if (!equipment.TryGetEquippedWeaponPose(out Vector3 position, out Vector3 euler, out Vector3 scale))
        {
            Debug.LogWarning("No equipped weapon found to copy pose from.");
            return;
        }

        ApplyPoseToEquipment(equipment, position, euler, scale);

        PlayerEquipment prefabEquipment = PrefabUtility.GetCorrespondingObjectFromSource(equipment);
        if (prefabEquipment != null)
        {
            ApplyPoseToEquipment(prefabEquipment, position, euler, scale);
        }

        AssetDatabase.SaveAssets();
        Debug.Log($"Saved equipped weapon pose: position {position}, euler {euler}, scale {scale}");
    }

    private static void ApplyPoseToEquipment(PlayerEquipment equipment, Vector3 position, Vector3 euler, Vector3 scale)
    {
        Undo.RecordObject(equipment, "Save Equipped Weapon Pose");

        SerializedObject serializedEquipment = new SerializedObject(equipment);
        serializedEquipment.FindProperty("weaponLocalPosition").vector3Value = position;
        serializedEquipment.FindProperty("weaponLocalEuler").vector3Value = euler;
        serializedEquipment.FindProperty("weaponLocalScale").vector3Value = scale;
        serializedEquipment.ApplyModifiedProperties();

        EditorUtility.SetDirty(equipment);
    }
}
