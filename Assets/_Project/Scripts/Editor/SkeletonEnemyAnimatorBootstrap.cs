#if UNITY_EDITOR
using UnityEditor;
using UnityEditor.Animations;
using UnityEngine;

public static class SkeletonEnemyAnimatorBootstrap
{
    private const string ControllerPath = "Assets/_Project/Animations/SkeletonEnemy.controller";
    private const string IdleClipPath = "Assets/SazenGames/Skeleton/Art/Animations/Skeleton_idle.fbx";
    private const string WalkClipPath = "Assets/SazenGames/Skeleton/Art/Animations/Skeleton_walk_forward.fbx";
    private const string AttackClipPath = "Assets/SazenGames/Skeleton/Art/Animations/Skeleton_slash01.fbx";
    private const string StaggerClipPath = "Assets/SazenGames/Skeleton/Art/Animations/Skeleton_take_damage.fbx";
    private const string DeathClipPath = "Assets/SazenGames/Skeleton/Art/Animations/Skeleton_death.fbx";

    [InitializeOnLoadMethod]
    private static void EnsureControllerExists()
    {
        EditorApplication.delayCall += RebuildIfNeeded;
    }

    [MenuItem("Tools/Combat Sandbox/Rebuild Skeleton Enemy Animator")]
    public static void RebuildController()
    {
        Rebuild(force: true);
    }

    private static void RebuildIfNeeded()
    {
        Rebuild(force: false);
    }

    private static void Rebuild(bool force)
    {
        AnimatorController existingController = AssetDatabase.LoadAssetAtPath<AnimatorController>(ControllerPath);
        if (!force && existingController != null && HasExpectedStates(existingController))
        {
            return;
        }

        if (existingController != null)
        {
            AssetDatabase.DeleteAsset(ControllerPath);
        }

        AnimatorController controller = AnimatorController.CreateAnimatorControllerAtPath(ControllerPath);
        if (controller.layers.Length == 0)
        {
            controller.AddLayer("Base Layer");
        }

        AnimatorStateMachine stateMachine = controller.layers[0].stateMachine;

        AddState(stateMachine, "Idle", IdleClipPath, new Vector3(220f, 120f, 0f));
        AddState(stateMachine, "Walk", WalkClipPath, new Vector3(220f, 200f, 0f));
        AddState(stateMachine, "Attack", AttackClipPath, new Vector3(500f, 120f, 0f));
        AddState(stateMachine, "Stagger", StaggerClipPath, new Vector3(500f, 200f, 0f));
        AddState(stateMachine, "Death", DeathClipPath, new Vector3(780f, 160f, 0f));

        AssetDatabase.SaveAssets();
        AssetDatabase.Refresh();
    }

    private static bool HasExpectedStates(AnimatorController controller)
    {
        if (controller.layers.Length == 0)
        {
            return false;
        }

        AnimatorStateMachine stateMachine = controller.layers[0].stateMachine;
        return HasState(stateMachine, "Idle")
            && HasState(stateMachine, "Walk")
            && HasState(stateMachine, "Attack")
            && HasState(stateMachine, "Stagger")
            && HasState(stateMachine, "Death");
    }

    private static bool HasState(AnimatorStateMachine stateMachine, string stateName)
    {
        foreach (ChildAnimatorState childState in stateMachine.states)
        {
            if (childState.state != null && childState.state.name == stateName)
            {
                return true;
            }
        }

        return false;
    }

    private static void AddState(AnimatorStateMachine stateMachine, string stateName, string clipPath, Vector3 position)
    {
        AnimationClip clip = LoadClip(clipPath);
        AnimatorState state = stateMachine.AddState(stateName, position);
        state.motion = clip;
        state.writeDefaultValues = true;

        if (stateName == "Idle")
        {
            stateMachine.defaultState = state;
        }
    }

    private static AnimationClip LoadClip(string clipPath)
    {
        Object[] assets = AssetDatabase.LoadAllAssetsAtPath(clipPath);
        foreach (Object asset in assets)
        {
            if (asset is AnimationClip clip && !clip.name.StartsWith("__preview__", System.StringComparison.Ordinal))
            {
                return clip;
            }
        }

        Debug.LogWarning($"Could not find animation clip at {clipPath}.");
        return null;
    }
}
#endif
