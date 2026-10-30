#pragma once

#include "CoreMinimal.h"
#include "DeathMatch/Abilities/DMGameplayAbilityBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimTypes.h"
#include "DMFireAttempt.h"
#include "PartsCharacter/PartsHitCollision.h"
#include "DMGameplayAbility_Fire.generated.h"

class UAnimMontage;
class UParticleSystem;
class UWorld;
class ADMWeaponActor;
class UDMCombatFireModule;
struct FDMFireCycleRuntime;
class UDMCombatFireControlComponent;
class UAbilityTask_PlayMontageAndWait;

USTRUCT(BlueprintType)
struct FDMShotResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") bool bShotExecuted = false;
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") bool bHitAnything = false;
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") bool bHitHostile = false;
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") bool bHitFriendly = false;
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") bool bBlockedByWorld = false;
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") TObjectPtr<AActor> HitActor = nullptr;
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") FGameplayTag HitPart;
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") float BaseDamage = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") float DistanceDamageMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") float RequestedDamage = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") float DamageApplied = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result") bool bKilledTarget = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDMShotResultEvent, const FDMShotResult&, Result);

/**
 * DeathMatch fire: play FireMontage, ExecuteFire on AnimNotify_PlayMontageNotify
 * whose Notify Name matches FireNotifyName (set the same string in BP + montage).
 * Fire Control shares one Fire/FireCycle runner for Single, Burst and Automatic.
 */
UCLASS()
class SKYSEOUL_API UDMGameplayAbility_Fire : public UDMGameplayAbilityBase
{
	GENERATED_BODY()

public:
	UDMGameplayAbility_Fire();

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Called from PlayMontageNotify listener or UAnimNotify_DMNamed. */
	void HandleNamedAnimNotify(FName IncomingNotifyName, int32 MontageInstanceId = INDEX_NONE);

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Fire|Result")
	const FDMShotResult& GetLastShotResult() const { return LastShotResult; }

	/** True when AI Fire Control owns range/sight safety for this shot path. */
	static bool UsesAuthoritativeFireControl(
		const UDMCombatFireControlComponent* FireControl, bool bHasActiveCycle);

	UPROPERTY(BlueprintAssignable, Category = "DeathMatch|Fire|Result")
	FDMShotResultEvent OnShotResolved;

protected:
    friend class FDMAdaptiveAbilityTest;
    friend class FDMAdaptiveSafetyTest;
    UDMCombatFireControlComponent* GetCombatBrain() const;
    void StartFireCycleMontage();
    void ClearCycleMontageTask();
    void HandleFireCycle();
    void QueueFireCycle(bool bEnd);
    void ResumeFireCycle(int32 ExpectedCycle, bool bEnd);
    void CheckCycleSafety();
    EDMFireAttemptFailure CycleSafetyFailure(bool bCheckMovement = false) const;
    UPROPERTY(Transient) TObjectPtr<UAbilityTask_PlayMontageAndWait> CycleMontageTask;
    // Strong Fire Control reference keeps its native runtime state alive through callbacks.
    UPROPERTY(Transient) TObjectPtr<UDMCombatFireControlComponent> ActiveCombatBrain;
    UPROPERTY(Transient) TObjectPtr<UDMCombatFireModule> ActiveFireAction;
    FDMFireCycleRuntime* ActiveCycle = nullptr;
    bool bLegacySingleWithoutCycle = false;
    bool bEndingFireAbility = false;
    bool bExecutingFire = false;
    float CycleFireNotifyOffsetSeconds = 0.f;
    TWeakObjectPtr<AController> CycleController;
    TWeakObjectPtr<AActor> CycleTarget;
    TWeakObjectPtr<ADMWeaponActor> CycleWeapon;
    FTimerHandle CycleTimer, CycleNotifyTimer, SafetyTimer, CycleWatchdogTimer;
    bool bCycleQueued = false;
    int32 CycleMontageInstanceId = INDEX_NONE;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="DeathMatch|Fire|Animation")
    FName FireCycleNotifyName = TEXT("FireCycle");
    friend class FDMWeaponFireIntegrationTest;
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) const override;
	/** Avatar must be ADS (AimZoom IsAiming). Player: RMB. AI: Attack/Hold. */
	bool IsPlayerAimingAllowed(const FGameplayAbilityActorInfo* ActorInfo) const;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** Default: line trace + damage. BP can override for custom FX after notify. */
	UFUNCTION(BlueprintNativeEvent, Category = "DeathMatch|Fire")
	bool ExecuteFire(AActor* Target);
	virtual bool ExecuteFire_Implementation(AActor* Target);

	bool ExecutePatternFire(AActor* Target);
	bool IsCombatPatternAllowed(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool PerformFireTrace(AActor* Avatar, AActor* Target, FHitResult& OutHit, FVector& OutStart, FVector& OutEnd) const;
	float ApplyFireDamage(const FHitResult& Hit, AActor* Avatar, float RequestedDamage, FGameplayTag& OutHitPart) const;
	void DrawFireTrail(UWorld* World, const FVector& Start, const FVector& End, bool bHit, const FVector& ImpactPoint) const;
	FVector GetFireStartLocation(AActor* Avatar) const;
	FVector GetFireAimLocation(AActor* Avatar, AActor* Target) const;
	FVector GetCameraAimPoint(AActor* Avatar) const;
	ADMWeaponActor* ResolveActiveWeapon(AActor* Avatar) const;
	float ResolveMaxFireRange(AActor* Avatar) const;
	EDMFireAttemptFailure EvaluateLegacyRangeFailure(AActor* Avatar, AActor* Target) const;
	FDMShotResult BuildShotResult(AActor* Avatar, const FHitResult& Hit, bool bHit) const;
	bool IsActorDead(const AActor* Actor) const;
	bool IsShooterAds(AActor* Avatar) const;
	float ResolveShotSpreadDegrees(AActor* Avatar) const;
	FVector ApplySpreadToDirection(const FVector& BaseDir, float SpreadDegrees) const;
	void NotifyWeaponShotFired(AActor* Avatar) const;
	/** Rotate pawn/controller toward aim before muzzle sample + trace (AI combat). */
	void FaceTowardFireAim(AActor* Avatar, const FVector& AimPoint) const;

	void BindMontageNotify(UAnimInstance* AnimInstance);
	void UnbindMontageNotify();
	void HandleFireFromNotify(int32 NotifyIndex = INDEX_NONE);
	int32 ResolveNotifyIndexFromCurrentMontagePosition() const;
	void FinishFromMontage(bool bWasCancelled);
	void SetLegacyNextFireAllowedAfter(float ExtraDelaySeconds);
	void CollectFireNotifyTriggerTimes(TArray<float>& OutMontageTimes) const;
	void ScheduleFireNotifyTimers();
	void ClearFireNotifyTimers();
	const UDMGameplayAbility_Fire* ResolveInstanceForCanActivate(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsFireMontagePlaying(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool DoesNotifyNameMatch(FName IncomingNotifyName) const;
	bool DoesNotifyEventMatch(const FAnimNotifyEvent& NotifyEvent) const;

	UFUNCTION()
	void OnScheduledFireNotify(int32 NotifyIndex);

	UFUNCTION()
	void OnFireMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	UFUNCTION()
	void OnFireMontageCompleted();

	UFUNCTION()
	void OnFireMontageBlendOut();

	UFUNCTION()
	void OnFireMontageInterrupted();

	UFUNCTION()
	void OnFireMontageCancelled();

	// --- Animation (notify-driven fire) ---

	/** Fire pose montage. Assign on BP; leave empty to fire immediately (no anim). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeathMatch|Fire|Animation")
	TObjectPtr<UAnimMontage> FireMontage;

	/**
	 * Must match the notify on FireMontage:
	 * - AnimNotify_PlayMontageNotify → Notify Name, or
	 * - AnimNotify DM Named Notify → NotifyName
	 * (plain empty AnimNotify with display name only will NOT fire).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeathMatch|Fire|Animation")
	FName FireNotifyName = TEXT("Fire");

	/** Default true: wait for FireNotifyName, then ExecuteFire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeathMatch|Fire|Animation")
	bool bFireOnAnimNotify = true;

	/** Player/non-Fire-Control path only. Fire Control executes at most one shot per cycle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeathMatch|Fire|Animation")
	bool bAllowMultipleFireNotifies = true;

	/** If montage ends without a callback or timer arrival, attempt ExecuteFire once. Off by default so missing notifies are obvious. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeathMatch|Fire|Animation")
	bool bFallbackFireIfNotifyMissed = false;

	/**
	 * Also schedule ExecuteFire from montage asset notify times (authoritative).
	 * Runtime AnimNotify / PlayMontageNotify still work; timers avoid missed server notifies.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeathMatch|Fire|Animation")
	bool bScheduleFireFromMontageNotifyTimes = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeathMatch|Fire|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "DeathMatch|Fire|Animation")
	FName MontageStartSection = NAME_None;

	// --- Fire / trace ---

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire")
	bool bUseWeaponMuzzle = true;

	/** Require AimZoom IsAiming before fire (player RMB / AI Attack-Hold). */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire")
	bool bRequirePlayerAiming = true;

	/** Player/legacy path delay. AI Fire Control uses FireCycle settings instead. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire", meta = (ClampMin = "0.0"))
	float FireCooldown = 0.2f;

	/** Additional AI cone half-angle at 600 cm/s; scales with horizontal speed. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire", meta = (ClampMin = "0.0"))
	float MovingAISpreadDegrees = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire")
	bool bAllowFriendlyFire = false;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire", meta = (ClampMin = "0.0"))
	float FireDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire")
	bool bRequireTarget = false;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire")
	TEnumAsByte<ECollisionChannel> TraceChannel = PartsHitCollision::DefaultWeaponTraceChannel;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire")
	bool bTraceComplex = false;

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "DeathMatch|Fire", meta = (DeprecatedProperty, DeprecationMessage = "Ignored: PartsComponent owns PhAT-only collision; attached visuals are NoCollision.", ToolTip = "Deprecated. Stored value has no effect. Fire traces owner PhAT without per-shot attached-part ignore lists."))
	bool bIgnoreTargetPartMeshes = true;

	/**
	 * Before shot: rotate pawn/controller toward aim point.
	 * Needed for AI (OrientToMovement) so they don't fire while facing walk direction.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire|Aim")
	bool bFaceAimDirectionOnFire = true;

	/** If true, only AI controllers face aim (player keeps camera look). */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire|Aim", meta = (EditCondition = "bFaceAimDirectionOnFire"))
	bool bFaceAimDirectionOnlyForAI = true;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire|Accuracy", meta = (ClampMin = "0.0"))
	float FallbackSpreadDegrees = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire|Accuracy")
	bool bApplyWeaponSpread = true;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire|Trail")
	bool bShowFireTrail = true;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire|Trail", meta = (EditCondition = "bShowFireTrail", ClampMin = "0.01"))
	float FireTrailDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire|Trail", meta = (EditCondition = "bShowFireTrail", ClampMin = "0.1"))
	float FireTrailThickness = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire|Trail", meta = (EditCondition = "bShowFireTrail"))
	FColor FireTrailHitColor = FColor(255, 220, 80);

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire|Trail", meta = (EditCondition = "bShowFireTrail"))
	FColor FireTrailMissColor = FColor(180, 180, 180);

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire|Trail")
	TObjectPtr<UParticleSystem> FireTrailImpactEffect;

	/** Fallback only for pawns without an active DM weapon. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Fire", meta = (ClampMin = "1.0"))
	float FallbackFireRange = 3000.0f;

	/** Player/legacy-only activation gate. Never read or written by the AI Fire Control path. */
	float LegacyNextFireAllowedTimeSeconds = -1000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Fire|Result")
	FDMShotResult LastShotResult;

	uint64 FireAttemptId = 0;
    bool bFireNotifyDiscovered = false;
    bool bFireNotifyReceived = false;
    bool bFireTimerReceived = false;
    bool bShotAttempted = false;
    bool bShotCooldownCommitted = false;
	bool bShotOutcomeLogged = false;
    EDMFireAttemptFailure LastFireAttemptFailure = EDMFireAttemptFailure::None;
    bool BlockShot(EDMFireAttemptFailure Failure);
    bool bFiredThisActivation = false;
	bool bMontagePathActive = false;
	bool bFinishFromMontageHandled = false;

	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
	TArray<FTimerHandle> FireNotifyTimerHandles;
	/** Canonical notify slots for this activation; timer and runtime notify share these indices. */
	TArray<float> FireNotifyTriggerTimes;
	TSet<int32> ProcessedFireNotifyIndices;
};


