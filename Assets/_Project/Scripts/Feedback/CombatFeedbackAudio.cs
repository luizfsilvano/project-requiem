using UnityEngine;

public sealed class CombatFeedbackAudio : MonoBehaviour
{
    [SerializeField] private AudioClip swordSwingClip;
    [SerializeField] private AudioClip heavySwingClip;
    [SerializeField] private AudioClip hitClip;
    [SerializeField] private AudioClip heavyHitClip;
    [SerializeField] private AudioClip equipClip;
    [SerializeField] private AudioClip pickupClip;
    [SerializeField] private AudioClip enemyDeathClip;
    [SerializeField] private AudioClip playerHurtClip;
    [SerializeField] private AudioClip enemyAttackClip;
    [SerializeField, Range(0f, 1f)] private float volume = 1f;
    [SerializeField] private float enemyHitSoundCooldown = 0.08f;

    private static CombatFeedbackAudio instance;
    private float lastEnemyHitSoundTime = -999f;

    private void Awake()
    {
        instance = this;
        EnsureAudioListener();
    }

    private void OnDestroy()
    {
        if (instance == this)
        {
            instance = null;
        }
    }

    public static void PlayPlayerAttack(bool heavy, Vector3 position)
    {
        instance?.PlayAt(heavy ? instance.heavySwingClip : instance.swordSwingClip, position, heavy ? 0.78f : 0.62f, heavy ? 0.92f : 1.05f);
    }

    public static void PlayEnemyAttack(Vector3 position)
    {
        instance?.PlayAt(instance.enemyAttackClip, position, 0.55f, 1f);
    }

    public static void PlayEnemyHit(CombatImpactReaction reaction, CombatImpactWeight impactWeight, Vector3 position)
    {
        if (instance == null)
        {
            return;
        }

        if (reaction == CombatImpactReaction.Death)
        {
            instance.PlayAt(instance.enemyDeathClip, position, 0.8f, 0.92f);
            return;
        }

        if (Time.unscaledTime - instance.lastEnemyHitSoundTime < instance.enemyHitSoundCooldown)
        {
            return;
        }

        instance.lastEnemyHitSoundTime = Time.unscaledTime;
        bool heavy = reaction == CombatImpactReaction.Stagger || impactWeight == CombatImpactWeight.Heavy;
        AudioClip clip = instance.hitClip != null ? instance.hitClip : instance.heavyHitClip;
        instance.PlayAt(clip, position, heavy ? 0.58f : 0.52f, heavy ? 0.9f : 1.03f);
    }

    public static void PlayPlayerHurt(Vector3 position)
    {
        instance?.PlayAt(instance.playerHurtClip, position, 0.65f, 1f);
    }

    public static void PlayEquip(Vector3 position)
    {
        instance?.PlayAt(instance.equipClip, position, 0.55f, 1.05f);
    }

    public static void PlayPickup(Vector3 position)
    {
        instance?.PlayAt(instance.pickupClip, position, 0.55f, 1.08f);
    }

    private void PlayAt(AudioClip clip, Vector3 position, float clipVolume, float pitch)
    {
        if (clip == null)
        {
            return;
        }

        EnsureAudioListener();
        GameObject audioObject = new("CombatAudioOneShot");
        audioObject.transform.position = position;
        AudioSource source = audioObject.AddComponent<AudioSource>();
        source.clip = clip;
        source.volume = volume * clipVolume;
        source.pitch = pitch;
        source.spatialBlend = 0f;
        source.rolloffMode = AudioRolloffMode.Linear;
        source.maxDistance = 18f;
        source.Play();
        Destroy(audioObject, Mathf.Max(0.1f, clip.length / Mathf.Max(0.1f, pitch)) + 0.1f);
    }

    private static void EnsureAudioListener()
    {
        if (FindAnyObjectByType<AudioListener>() != null)
        {
            return;
        }

        Camera mainCamera = Camera.main;
        if (mainCamera != null)
        {
            mainCamera.gameObject.AddComponent<AudioListener>();
        }
    }
}
