using System.Collections;
using UnityEngine;

public sealed class PlayerHealth : MonoBehaviour
{
    [SerializeField] private int maxHealth = 100;
    [SerializeField] private Renderer bodyRenderer;
    [SerializeField] private Color hitColor = new(1f, 0.25f, 0.22f);
    [SerializeField] private Color deathColor = new(0.18f, 0.18f, 0.18f);
    [SerializeField] private float invulnerableAfterHit = 0.3f;
    [SerializeField] private float deathResetDelay = 0.75f;

    private int currentHealth;
    private float invulnerableTimer;
    private bool isDead;
    private Color defaultColor;
    private Coroutine flashRoutine;
    private Coroutine deathRoutine;

    public int CurrentHealth => currentHealth;
    public int MaxHealth => maxHealth;
    public float HealthRatio => maxHealth > 0 ? currentHealth / (float)maxHealth : 0f;

    private void Awake()
    {
        currentHealth = maxHealth;

        if (bodyRenderer == null)
        {
            bodyRenderer = GetComponentInChildren<Renderer>();
        }

        if (bodyRenderer != null)
        {
            defaultColor = bodyRenderer.material.color;
        }
    }

    private void Update()
    {
        if (invulnerableTimer > 0f)
        {
            invulnerableTimer -= Time.deltaTime;
        }
    }

    public void TakeHit(CombatHit hit)
    {
        if (isDead || invulnerableTimer > 0f)
        {
            return;
        }

        if (DevSettings.PlayerInvincible)
        {
            Debug.Log($"Player ignored {hit.Damage} damage because dev invincibility is enabled.");
            return;
        }

        currentHealth = Mathf.Max(0, currentHealth - hit.Damage);
        invulnerableTimer = invulnerableAfterHit;
        Debug.Log($"Player took {hit.Damage} damage. HP: {currentHealth}/{maxHealth}.");
        CombatFeedbackAudio.PlayPlayerHurt(transform.position);

        if (flashRoutine != null)
        {
            StopCoroutine(flashRoutine);
        }

        flashRoutine = StartCoroutine(FlashHit(currentHealth == 0));

        if (currentHealth == 0 && deathRoutine == null)
        {
            deathRoutine = StartCoroutine(ResetAfterDeath());
        }
    }

    public void RestoreFullHealth()
    {
        RestoreHealth(maxHealth);
    }

    public void RestoreHealth(int restoredHealth)
    {
        if (flashRoutine != null)
        {
            StopCoroutine(flashRoutine);
            flashRoutine = null;
        }

        if (deathRoutine != null)
        {
            StopCoroutine(deathRoutine);
            deathRoutine = null;
        }

        currentHealth = Mathf.Clamp(restoredHealth, 1, Mathf.Max(1, maxHealth));
        invulnerableTimer = 0f;
        isDead = false;

        if (bodyRenderer != null)
        {
            bodyRenderer.material.color = defaultColor;
        }
    }

    private IEnumerator FlashHit(bool died)
    {
        if (bodyRenderer == null)
        {
            yield break;
        }

        bodyRenderer.material.color = died ? deathColor : hitColor;
        yield return new WaitForSecondsRealtime(died ? 0.18f : 0.08f);

        if (!died)
        {
            bodyRenderer.material.color = defaultColor;
        }

        flashRoutine = null;
    }

    private IEnumerator ResetAfterDeath()
    {
        isDead = true;
        yield return new WaitForSecondsRealtime(deathResetDelay);
        RestoreFullHealth();
        deathRoutine = null;
        Debug.Log("Player health reset after death.");
    }
}
