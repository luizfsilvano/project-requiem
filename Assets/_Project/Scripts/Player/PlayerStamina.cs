using System;
using UnityEngine;

public sealed class PlayerStamina : MonoBehaviour
{
    [Header("Pool")]
    [SerializeField] private float maxStamina = 100f;
    [SerializeField] private float regenPerSecond = 28f;
    [SerializeField] private float regenDelay = 0.65f;

    [Header("Combat Costs")]
    [SerializeField] private float dodgeCost = 28f;
    [SerializeField] private float unarmedAttackCost = 10f;
    [SerializeField] private float weaponAttackCost = 14f;
    [SerializeField] private float weaponFinisherCost = 24f;

    private float currentStamina;
    private float regenDelayTimer;

    public event Action StaminaChanged;

    public float CurrentStamina => currentStamina;
    public float MaxStamina => maxStamina;
    public float StaminaRatio => maxStamina > 0f ? currentStamina / maxStamina : 0f;
    public float DodgeCost => dodgeCost;

    private void Awake()
    {
        currentStamina = Mathf.Max(0f, maxStamina);
    }

    private void Update()
    {
        if (currentStamina >= maxStamina)
        {
            return;
        }

        if (regenDelayTimer > 0f)
        {
            regenDelayTimer -= Time.deltaTime;
            return;
        }

        currentStamina = Mathf.Min(maxStamina, currentStamina + regenPerSecond * Time.deltaTime);
        StaminaChanged?.Invoke();
    }

    public bool CanSpend(float amount)
    {
        return DevSettings.InfiniteStamina || currentStamina >= Mathf.Max(0f, amount);
    }

    public bool TrySpend(float amount)
    {
        amount = Mathf.Max(0f, amount);
        if (!CanSpend(amount))
        {
            return false;
        }

        if (!DevSettings.InfiniteStamina && amount > 0f)
        {
            currentStamina = Mathf.Max(0f, currentStamina - amount);
            regenDelayTimer = regenDelay;
            StaminaChanged?.Invoke();
        }

        return true;
    }

    public bool TrySpendDodge()
    {
        return TrySpend(dodgeCost);
    }

    public bool CanSpendAttack(int comboStep, bool isWeaponAttack)
    {
        return CanSpend(GetAttackCost(comboStep, isWeaponAttack));
    }

    public bool TrySpendAttack(int comboStep, bool isWeaponAttack)
    {
        return TrySpend(GetAttackCost(comboStep, isWeaponAttack));
    }

    public float GetAttackCost(int comboStep, bool isWeaponAttack)
    {
        if (!isWeaponAttack)
        {
            return unarmedAttackCost;
        }

        return comboStep >= 3 ? weaponFinisherCost : weaponAttackCost;
    }

    public void RestoreFullStamina()
    {
        RestoreStamina(maxStamina);
    }

    public void RestoreStamina(float restoredStamina)
    {
        currentStamina = Mathf.Clamp(restoredStamina, 0f, Mathf.Max(0f, maxStamina));
        regenDelayTimer = currentStamina < maxStamina ? regenDelay : 0f;
        StaminaChanged?.Invoke();
    }
}
