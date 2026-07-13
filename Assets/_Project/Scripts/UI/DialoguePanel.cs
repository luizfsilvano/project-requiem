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
    [SerializeField] private Text feedbackText;
    [SerializeField] private Button primaryActionButton;
    [SerializeField] private Text primaryActionText;
    [SerializeField] private Button secondaryActionButton;
    [SerializeField] private Text secondaryActionText;
    [SerializeField] private Button closeButton;

    [Header("Lifetime")]
    [SerializeField, Min(0.5f)] private float maxDialogueDistance = 5f;

    private SimpleNpcInteractable currentNpc;
    private PlayerInteractor currentInteractor;
    private PlayerHealth playerHealth;
    private PlayerQuestLog questLog;
    private QuestGiver questGiver;
    private QuestDialogueViewModel dialogueViewModel;
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
        questLog = interactor.GetComponent<PlayerQuestLog>();
        questGiver = npc.GetComponent<QuestGiver>();
        if (questLog != null)
        {
            questLog.QuestChanged -= HandleQuestChanged;
            questLog.QuestChanged += HandleQuestChanged;
        }
        openedFrame = Time.frameCount;
        IsOpen = true;

        SetText(speakerText, npc.SpeakerName);
        SetText(feedbackText, string.Empty);
        RefreshDialogue();
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
        if (questLog != null)
        {
            questLog.QuestChanged -= HandleQuestChanged;
        }

        currentNpc = null;
        currentInteractor = null;
        playerHealth = null;
        questLog = null;
        questGiver = null;
        dialogueViewModel = default;
        openedFrame = -1;
    }

    private void HandleQuestChanged(QuestRuntimeState quest)
    {
        if (IsOpen
            && quest != null
            && questGiver?.QuestDefinition != null
            && string.Equals(
                quest.QuestId,
                questGiver.QuestDefinition.QuestId,
                System.StringComparison.Ordinal))
        {
            RefreshDialogue();
        }
    }

    public void ExecutePrimaryAction()
    {
        ExecuteDialogueAction(dialogueViewModel.PrimaryAction);
    }

    public void ExecuteSecondaryAction()
    {
        ExecuteDialogueAction(dialogueViewModel.SecondaryAction);
    }

    private void ExecuteDialogueAction(QuestDialogueAction action)
    {
        if (!IsOpen || questGiver == null || questLog == null)
        {
            return;
        }

        switch (action)
        {
            case QuestDialogueAction.Accept:
                bool accepted = questGiver.TryAccept(questLog, out string acceptFeedback);
                SetText(feedbackText, acceptFeedback);
                if (accepted)
                {
                    RefreshDialogue(keepFeedback: true);
                }
                break;
            case QuestDialogueAction.Decline:
                SetText(dialogueText, questGiver.GetDeclinedLine());
                SetText(feedbackText, "Missao recusada. Voce pode conversar novamente mais tarde.");
                dialogueViewModel = default;
                ConfigureActionButton(primaryActionButton, primaryActionText, string.Empty, false);
                ConfigureActionButton(secondaryActionButton, secondaryActionText, string.Empty, false);
                break;
            case QuestDialogueAction.TurnIn:
                bool completed = questGiver.TryTurnIn(questLog, out string turnInFeedback);
                SetText(feedbackText, turnInFeedback);
                if (completed)
                {
                    RefreshDialogue(keepFeedback: true);
                }
                break;
        }
    }

    private void RefreshDialogue(bool keepFeedback = false)
    {
        if (!keepFeedback)
        {
            SetText(feedbackText, string.Empty);
        }

        string fallbackLine = currentNpc != null ? currentNpc.DialogueLine : string.Empty;
        dialogueViewModel = questGiver != null
            ? questGiver.BuildDialogue(questLog, fallbackLine)
            : new QuestDialogueViewModel(
                fallbackLine,
                QuestDialogueAction.None,
                string.Empty,
                QuestDialogueAction.None,
                string.Empty);
        SetText(dialogueText, dialogueViewModel.Line);
        ConfigureActionButton(
            primaryActionButton,
            primaryActionText,
            dialogueViewModel.PrimaryLabel,
            dialogueViewModel.PrimaryAction != QuestDialogueAction.None);
        ConfigureActionButton(
            secondaryActionButton,
            secondaryActionText,
            dialogueViewModel.SecondaryLabel,
            dialogueViewModel.SecondaryAction != QuestDialogueAction.None);
    }

    private void BindListener()
    {
        if (listenerBound || closeButton == null)
        {
            return;
        }

        closeButton.onClick.AddListener(Close);
        primaryActionButton?.onClick.AddListener(ExecutePrimaryAction);
        secondaryActionButton?.onClick.AddListener(ExecuteSecondaryAction);
        listenerBound = true;
    }

    private void UnbindListener()
    {
        if (!listenerBound || closeButton == null)
        {
            return;
        }

        closeButton.onClick.RemoveListener(Close);
        primaryActionButton?.onClick.RemoveListener(ExecutePrimaryAction);
        secondaryActionButton?.onClick.RemoveListener(ExecuteSecondaryAction);
        listenerBound = false;
    }

    private static void ConfigureActionButton(
        Button button,
        Text label,
        string value,
        bool visible)
    {
        if (button != null)
        {
            button.gameObject.SetActive(visible);
            button.interactable = visible;
        }

        SetText(label, visible ? value : string.Empty);
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
