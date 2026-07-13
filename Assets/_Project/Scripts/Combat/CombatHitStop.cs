using System.Collections;
using UnityEngine;

public sealed class CombatHitStop : MonoBehaviour
{
    private const float HitStopTimeScale = 0.05f;

    private static CombatHitStop instance;
    private static Coroutine activeRoutine;
    private static bool changedTimeScale;
    private static float previousTimeScale;
    private static float previousFixedDeltaTime;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
    private static void ResetStaticState()
    {
        RestoreTimeScale();
        instance = null;
        activeRoutine = null;
    }

    public static bool TryPlay(float duration)
    {
        if (duration <= 0f || activeRoutine != null || Time.timeScale <= Mathf.Epsilon)
        {
            return false;
        }

        EnsureInstance();
        activeRoutine = instance.StartCoroutine(instance.Play(duration));
        return true;
    }

    private static void EnsureInstance()
    {
        if (instance != null)
        {
            return;
        }

        GameObject host = new GameObject("[Combat Hit Stop]");
        DontDestroyOnLoad(host);
        instance = host.AddComponent<CombatHitStop>();
    }

    private IEnumerator Play(float duration)
    {
        previousTimeScale = Time.timeScale;
        previousFixedDeltaTime = Time.fixedDeltaTime;
        changedTimeScale = true;

        float fixedDeltaRatio = HitStopTimeScale / previousTimeScale;
        Time.timeScale = HitStopTimeScale;
        Time.fixedDeltaTime = previousFixedDeltaTime * fixedDeltaRatio;

        yield return new WaitForSecondsRealtime(duration);

        RestoreTimeScale();
        activeRoutine = null;
    }

    private static void RestoreTimeScale()
    {
        if (!changedTimeScale)
        {
            return;
        }

        Time.timeScale = previousTimeScale;
        Time.fixedDeltaTime = previousFixedDeltaTime;
        changedTimeScale = false;
    }

    private void OnDestroy()
    {
        if (instance != this)
        {
            return;
        }

        RestoreTimeScale();
        activeRoutine = null;
        instance = null;
    }
}
