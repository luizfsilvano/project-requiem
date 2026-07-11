using UnityEngine;
using UnityEngine.EventSystems;

public sealed class CharacterPreviewDragArea : MonoBehaviour, IDragHandler, IScrollHandler
{
    [SerializeField] private CharacterPreviewController previewController;
    [SerializeField] private float rotationSensitivity = 0.35f;
    [SerializeField] private float zoomSensitivity = 0.18f;

    private void Awake()
    {
        if (previewController == null)
        {
            previewController = FindFirstObjectByType<CharacterPreviewController>();
        }
    }

    public void OnDrag(PointerEventData eventData)
    {
        if (eventData.button == PointerEventData.InputButton.Left)
        {
            previewController?.Rotate(-eventData.delta.x * rotationSensitivity);
        }
    }

    public void OnScroll(PointerEventData eventData)
    {
        previewController?.Zoom(-eventData.scrollDelta.y * zoomSensitivity);
    }
}
