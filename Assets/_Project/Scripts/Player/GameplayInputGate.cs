using UnityEngine;

public static class GameplayInputGate
{
    private static int suppressThroughFrame = -1;

    public static bool InventoryOpen { get; private set; }
    public static bool IsBlocked => DevSettings.ConsoleOpen
        || InventoryOpen
        || Time.frameCount <= suppressThroughFrame;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    private static void ResetBeforeSceneLoad()
    {
        ResetInventoryState();
    }

    public static bool TrySetInventoryOpen(bool isOpen)
    {
        if (isOpen && DevSettings.ConsoleOpen)
        {
            return false;
        }

        InventoryOpen = isOpen;
        return true;
    }

    public static void SuppressForFrames(int frameCount = 1)
    {
        suppressThroughFrame = Mathf.Max(suppressThroughFrame, Time.frameCount + Mathf.Max(0, frameCount));
    }

    public static void ResetInventoryState()
    {
        InventoryOpen = false;
        suppressThroughFrame = -1;
    }
}
