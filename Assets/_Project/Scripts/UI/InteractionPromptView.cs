using UnityEngine;
using UnityEngine.UI;

public sealed class InteractionPromptView : MonoBehaviour
{
    [SerializeField] private GameObject promptRoot;
    [SerializeField] private Text promptText;

    private InteractionPromptData currentPrompt;
    private bool hasPrompt;

    public bool IsVisible => promptRoot != null && promptRoot.activeSelf;
    public bool HasPrompt => hasPrompt;
    public InteractionPromptData CurrentPrompt => currentPrompt;

    private void Awake()
    {
        if (promptRoot == null)
        {
            promptRoot = gameObject;
        }

        Hide();
    }

    public void Show(InteractionPromptData prompt)
    {
        string text = string.IsNullOrWhiteSpace(prompt.Text)
            ? BuildFallbackText(prompt)
            : prompt.Text;
        if (string.IsNullOrWhiteSpace(text))
        {
            Hide();
            return;
        }

        currentPrompt = prompt;
        hasPrompt = true;
        if (promptText != null && !string.Equals(promptText.text, text, System.StringComparison.Ordinal))
        {
            promptText.text = text;
        }

        if (promptRoot != null && !promptRoot.activeSelf)
        {
            promptRoot.SetActive(true);
        }
    }

    public void Hide()
    {
        hasPrompt = false;
        currentPrompt = default;
        if (promptRoot != null && promptRoot.activeSelf)
        {
            promptRoot.SetActive(false);
        }
    }

    private static string BuildFallbackText(InteractionPromptData prompt)
    {
        if (string.IsNullOrWhiteSpace(prompt.ActionName))
        {
            return string.Empty;
        }

        return $"E — {prompt.ActionName}";
    }
}
