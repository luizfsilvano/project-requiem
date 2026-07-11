using System;
using UnityEngine;

public enum GameplayModalMode
{
    None,
    Inventory,
    Container,
    Dialogue,
    DevConsole
}

public static class GameplayInputGate
{
    private static readonly object LegacyInventoryOwner = new();

    private static object activeModalOwner;
    private static int suppressThroughFrame = -1;
    private static CursorLockMode previousCursorLockMode;
    private static bool previousCursorVisible;
    private static bool ownsCursorOverride;

    public static event Action<GameplayModalMode, GameplayModalMode> ModalChanged;

    public static GameplayModalMode ActiveMode { get; private set; }
    public static bool IsModalOpen => ActiveMode != GameplayModalMode.None;
    public static bool InventoryOpen => ActiveMode == GameplayModalMode.Inventory;
    public static bool IsBlocked => IsModalOpen || Time.frameCount <= suppressThroughFrame;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    private static void ResetBeforeSceneLoad()
    {
        ActiveMode = GameplayModalMode.None;
        activeModalOwner = null;
        suppressThroughFrame = -1;
        ownsCursorOverride = false;
        ModalChanged = null;
        DevSettings.ConsoleOpen = false;
    }

    public static bool TryOpenModal(GameplayModalMode mode, object owner)
    {
        if (mode == GameplayModalMode.None || owner == null)
        {
            return false;
        }

        if (ActiveMode == mode && ReferenceEquals(activeModalOwner, owner))
        {
            return true;
        }

        if (IsModalOpen || Time.frameCount <= suppressThroughFrame)
        {
            return false;
        }

        GameplayModalMode previousMode = ActiveMode;
        previousCursorLockMode = Cursor.lockState;
        previousCursorVisible = Cursor.visible;
        ownsCursorOverride = true;
        activeModalOwner = owner;
        ActiveMode = mode;
        EnsureModalCursor();
        ModalChanged?.Invoke(previousMode, ActiveMode);
        return true;
    }

    public static bool TryCloseModal(
        GameplayModalMode mode,
        object owner,
        int suppressFrames = 1)
    {
        if (mode == GameplayModalMode.None
            || ActiveMode != mode
            || owner == null
            || !ReferenceEquals(activeModalOwner, owner))
        {
            return false;
        }

        CloseActiveModal(suppressFrames);
        return true;
    }

    public static bool IsOpen(GameplayModalMode mode)
    {
        return mode != GameplayModalMode.None && ActiveMode == mode;
    }

    public static bool IsOwnedBy(GameplayModalMode mode, object owner)
    {
        return owner != null && ActiveMode == mode && ReferenceEquals(activeModalOwner, owner);
    }

    public static void EnsureModalCursor()
    {
        if (!IsModalOpen)
        {
            return;
        }

        Cursor.lockState = CursorLockMode.None;
        Cursor.visible = true;
    }

    public static bool TrySetInventoryOpen(bool isOpen)
    {
        if (isOpen)
        {
            return TryOpenModal(GameplayModalMode.Inventory, LegacyInventoryOwner);
        }

        if (!InventoryOpen)
        {
            return true;
        }

        return TryCloseModal(GameplayModalMode.Inventory, LegacyInventoryOwner);
    }

    public static void SuppressForFrames(int frameCount = 1)
    {
        suppressThroughFrame = Mathf.Max(
            suppressThroughFrame,
            Time.frameCount + Mathf.Max(0, frameCount));
    }

    public static void ResetInventoryState()
    {
        if (InventoryOpen)
        {
            CloseActiveModal(suppressFrames: -1);
        }

        suppressThroughFrame = -1;
    }

    private static void CloseActiveModal(int suppressFrames)
    {
        GameplayModalMode previousMode = ActiveMode;
        ActiveMode = GameplayModalMode.None;
        activeModalOwner = null;

        if (ownsCursorOverride)
        {
            Cursor.lockState = previousCursorLockMode;
            Cursor.visible = previousCursorVisible;
            ownsCursorOverride = false;
        }

        if (suppressFrames >= 0)
        {
            SuppressForFrames(suppressFrames);
        }

        ModalChanged?.Invoke(previousMode, GameplayModalMode.None);
    }
}
