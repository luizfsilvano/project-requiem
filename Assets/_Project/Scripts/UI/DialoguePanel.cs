using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

[DefaultExecutionOrder(30)]
public sealed class DialoguePanel : MonoBehaviour
{
    [Header("Panel")]
    [SerializeField] private GameObject panelRoot;
    [SerializeField] private Text speakerText;
    [SerializeField] private Text dialogueText;
    [SerializeField] private Text closeHintText;
    [SerializeField] private Button closeButton;

    [Header("Lifetime")]
    [SerializeField, Min(0.5f)] private float maxDialogueDistance = 5f;

    private SimpleNpcInteractable currentNpc;
    private PlayerInteractor currentInteractor;
    private PlayerHealth playerHealth;
    private int openedFrame = -1;
    private bool listenerBound;

    public bool IsOpen { get; private set; }
    public SimpleNpcInteractable CurrentNpc => currentNpc;

    private void Awake()
    {
        BindListener();
        SetPanelVisible(false);
    }

    private void OnEnable()
    {
        BindListener();
    }

    private void OnDisable()
    {
        UnbindListener();
        if (Application.isPlaying && IsOpen)
        {
            Close();
        }
    }

    private void OnDestroy()
    {
        UnbindListener();
        if (IsOpen && GameplayInputGate.IsOwnedBy(GameplayModalMode.Dialogue, this))
        {
            GameplayInputGate.TryCloseModal(GameplayModalMode.Dialogue, this, 1);
        }
    }

    private void Update()
    {
        if (!IsOpen)
        {
            return;
        }

        if (!GameplayInputGate.IsOwnedBy(GameplayModalMode.Dialogue, this))
        {
            HideWithoutClosingModal();
            return;
        }

        GameplayInputGate.EnsureModalCursor();
        if (!CanRemainOpen())
        {
            Close();
            return;
        }

        if (Time.frameCount <= openedFrame || Keyboard.current == null)
        {
            return;
        }

        if (Keyboard.current.eKey.wasPressedThisFrame || Keyboard.current.escapeKey.wasPressedThisFrame)
        {
            Close();
        }
    }

    public bool Open(SimpleNpcInteractable npc, PlayerInteractor interactor)
    {
        if (npc == null || !npc.isActiveAndEnabled || interactor == null || panelRoot == null)
        {
            return false;
        }

        if (!IsOpen && !GameplayInputGate.TryOpenModal(GameplayModalMode.Dialogue, this))
        {
            return false;
        }

        if (IsOpen && !GameplayInputGate.IsOwnedBy(GameplayModalMode.Dialogue, this))
        {
            return false;
        }

        currentNpc = npc;
        currentInteractor = interactor;
        playerHealth = interactor.GetComponent<PlayerHealth>();
        openedFrame = Time.frameCount;
        IsOpen = true;

        SetText(speakerText, npc.SpeakerName);
        SetText(dialogueText, npc.DialogueLine);
        SetText(closeHintText, "E / Esc — Fechar");
        SetPanelVisible(true);
        GameplayInputGate.EnsureModalCursor();
        return true;
    }

    public void Close()
    {
        if (!IsOpen)
        {
            return;
        }

        bool ownsModal = GameplayInputGate.IsOwnedBy(GameplayModalMode.Dialogue, this);
        IsOpen = false;
        SetPanelVisible(false);
        ClearSession();
        if (ownsModal)
        {
            GameplayInputGate.TryCloseModal(GameplayModalMode.Dialogue, this, 1);
        }
    }

    private bool CanRemainOpen()
    {
        if (currentNpc == null
            || !currentNpc.isActiveAndEnabled
            || currentInteractor == null
            || !currentInteractor.isActiveAndEnabled
            || playerHealth != null && playerHealth.CurrentHealth <= 0)
        {
            return false;
        }

        Transform interactionPoint = currentNpc.InteractionPoint;
        if (interactionPoint == null || !interactionPoint.gameObject.activeInHierarchy)
        {
            return false;
        }

        float allowedDistance = Mathf.Max(maxDialogueDistance, currentNpc.InteractionRange);
        return (interactionPoint.position - currentInteractor.transform.position).sqrMagnitude
            <= allowedDistance * allowedDistance;
    }

    private void HideWithoutClosingModal()
    {
        IsOpen = false;
        SetPanelVisible(false);
        ClearSession();
    }

    private void ClearSession()
    {
        currentNpc = null;
        currentInteractor = null;
        playerHealth = null;
        openedFrame = -1;
    }

    private void BindListener()
    {
        if (listenerBound || closeButton == null)
        {
            return;
        }

        closeButton.onClick.AddListener(Close);
        listenerBound = true;
    }

    private void UnbindListener()
    {
        if (!listenerBound || closeButton == null)
        {
            return;
        }

        closeButton.onClick.RemoveListener(Close);
        listenerBound = false;
    }

    private void SetPanelVisible(bool visible)
    {
        if (panelRoot != null && panelRoot.activeSelf != visible)
        {
            panelRoot.SetActive(visible);
        }
    }

    private static void SetText(Text target, string value)
    {
        if (target != null)
        {
            target.text = value ?? string.Empty;
        }
    }

    private void OnValidate()
    {
        maxDialogueDistance = Mathf.Max(0.5f, maxDialogueDistance);
    }
}
