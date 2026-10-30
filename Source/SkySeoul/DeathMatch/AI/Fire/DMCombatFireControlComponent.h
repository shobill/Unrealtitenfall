#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeathMatch/Abilities/DMFireAttempt.h"
#include "DeathMatch/AI/Combat/DMFireCycle.h"
#include "GenericTeamAgentInterface.h"
#include "DMCombatFireTypes.h"
#include "DMCombatFireControlComponent.generated.h"

class UDMCombatKnowledgeComponent;
class ADeathMatchRobotAIController;
class ADMWeaponActor;
enum class EDMActionIntent : uint8;
struct FDMShotResult;

/** Muzzle/view-origin range result shared by Fire Control, abilities, and position evaluation. */
struct SKYSEOUL_API FDMFireRangeEvaluation
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	float Distance = 0.f;
	float MaximumRange = 0.f;
	bool bAllowed = false;
};

/** Stable attack-opportunity result. It intentionally excludes fire execution timing and movement policy. */
UENUM(BlueprintType)
enum class EDMClearShotFailure : uint8
{
	None,
	InvalidTarget,
	NotVisible,
	MissingWeapon,
	Range,
	Sight,
	FriendlyFireRisk
};

/** Executes explicit BT fire commands and owns fire safety/cadence state. */
UCLASS(ClassGroup=(DeathMatch), meta=(BlueprintSpawnableComponent))
class SKYSEOUL_API UDMCombatFireControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDMCombatFireControlComponent();

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Fire Control")
	void SetTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Fire Control")
	virtual bool StartFire();

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Fire Control")
	virtual void StopFire();

	uint64 BeginFireRequest();
	void RefreshFireRequest(uint64 RequestId);
	void EndFireRequest(uint64 RequestId);
	void ClearFireRequests();
	void SetActionIntent(EDMActionIntent Intent, TOptional<bool> AllowMovingFireOverride = TOptional<bool>());
	bool HasFireRequests() const { return !FireRequests.IsEmpty(); }
	int32 GetFireRequestCount() const { return FireRequests.Num(); }
	static bool ShouldAttemptFireForRequest(bool bOwnsRequest, bool bFireActive, bool bTimingReady = true)
	{
		return bOwnsRequest && !bFireActive && bTimingReady;
	}
	bool IsMovingFireAllowedByAction() const { return bActionAllowsFireWhileMoving; }
	static bool IntentAllowsFire(EDMActionIntent Intent);
	static bool IntentAllowsMovingFire(EDMActionIntent Intent);
	static bool IsFriendlyFireBlocker(FGenericTeamId ShooterTeam, const AActor* HitActor, const AActor* IntendedTarget);
	/** Shared by live Fire Control and combat-position evaluation. */
	static ECollisionChannel ResolveWeaponTraceChannel(const ADeathMatchRobotAIController* AI);
	/** Resolves the live origin contract used by fire safety: weapon muzzle, then Pawn View. */
	static bool ResolveCurrentFireOrigin(const ADeathMatchRobotAIController* AI, FVector& OutOrigin);
	/** Projects the pawn-relative live fire origin to a candidate without moving the pawn. */
	static FVector ResolveCandidateFireOrigin(const ADeathMatchRobotAIController* AI,
		const FVector& CandidateLocation, float FallbackEyeHeight);
	static FVector ProjectFireOriginOffset(const FVector& PawnLocation, const FVector& CurrentFireOrigin,
		const FVector& CandidateLocation);
	static FVector ResolveIntendedTargetLocation(const AActor* Shooter, AActor* IntendedTarget,
		const FVector& FireOrigin);
	/** Small boundary tolerance only; this is not bonus gameplay range. */
	static constexpr float FireRangeToleranceCm = 2.f;
	static FDMFireRangeEvaluation EvaluateFireRange(const FVector& Start, const FVector& End,
		float MaximumRange);
	/** Resolves the shared aim location, then evaluates muzzle/view-origin distance. */
	static FDMFireRangeEvaluation ResolveFireRange(const AActor* Shooter, AActor* IntendedTarget,
		const FVector& FireOrigin, float MaximumRange);
	static EDMFireAttemptFailure ClassifyLineOfFireHit(FGenericTeamId ShooterTeam,
		const AActor* HitActor, const AActor* IntendedTarget, bool bHadBlockingHit);
	static EDMFireAttemptFailure EvaluateLineOfFire(UWorld* World, const AActor* Shooter,
		FGenericTeamId ShooterTeam, const FVector& Start, AActor* IntendedTarget,
		float MaximumRange, ECollisionChannel TraceChannel);
	/** Pure precedence/mapping helper used by the live query and deterministic policy tests. */
	static EDMClearShotFailure ClassifyClearShotFailure(bool bHasValidContext,
		bool bHasSelectedTarget, bool bSelectedTargetVisible, bool bHasActiveRangeWeapon,
		EDMFireAttemptFailure LineOfFireFailure);
	static const TCHAR* GetClearShotFailureName(EDMClearShotFailure Failure);

	/**
	 * Answers whether the selected, currently visible CombatTarget is attackable from the
	 * current weapon muzzle. Cadence, ability/montage state, ADS, and movement are excluded.
	 */
	UFUNCTION(BlueprintPure, Category="DeathMatch|Fire Control")
	bool HasClearShotToCombatTarget() const { return GetClearShotFailure() == EDMClearShotFailure::None; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Fire Control")
	EDMClearShotFailure GetClearShotFailure() const;
	bool HasClearShotObservation() const { return bHasClearShotObservation; }
	EDMClearShotFailure GetLastClearShotFailure() const { return LastClearShotFailure; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Fire Control")
	bool CanStartFire() const { return GetFireFailure() == EDMFireAttemptFailure::None; }

	EDMFireAttemptFailure GetFireFailure(bool bIgnoreCadence = false, bool bIgnoreMovementPolicy = false) const;
	EDMFireAttemptFailure GetFireSafetyFailure(bool bCheckMovement = false) const;
	/** Authoritative successful-shot cadence. Fire Module time is only a synchronized compatibility cache. */
	float GetNextShotAllowedTime() const { return FireCycle.NextShotTime; }
	/** Independent, short-lived failure retry gate. It never changes successful-shot cadence. */
	float GetRetryAfterTime() const { return RetryAfterTime; }
	float GetTimeUntilFireReady() const;
	bool IsFireCycleReady() const;
	bool IsFireCycleReadyAt(float Now) const;
	static bool IsFireTimingReadyAt(float Now, float NextShotTime, float RetryTime)
	{
		return Now + UE_KINDA_SMALL_NUMBER >= FMath::Max(NextShotTime, RetryTime);
	}
	void RecordFailedFireAttempt(EDMFireAttemptFailure Failure);
	void RecordFailedFireAttemptAt(EDMFireAttemptFailure Failure, float Now);
	void ClearFireRetryDelay();

	UFUNCTION(BlueprintPure, Category="DeathMatch|Fire Control")
	virtual bool IsFireActive() const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Fire Control")
	AActor* GetTarget() const { return FireTarget.Get(); }
	AActor* GetCombatTarget() const { return FireTarget.Get(); }
	bool IsEngaged() const { return FireTarget.IsValid(); }

	bool AllowsFire() const { return CanStartFire(); }
	bool AuthorizeFireCycle(bool bNewActivation);
	void NotifyShotFired();
	void PublishFireCycle();
	void ResetFireControl();
	uint64 EnsureFireAttempt();
	uint64 GetFireAttemptId() const { return FireAttemptId; }
	void ConsumeFireAttempt() { bAttemptConsumed = true; }
	void EvaluatePendingTargetAtCycleBoundary() {}

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category="DeathMatch|Fire Control")
	TObjectPtr<UDMCombatFireModule> FireModule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Fire Control")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Fire Control", meta=(ClampMin="0.1", ClampMax="180"))
	float FacingToleranceDegrees = 12.f;

	FDMFireCycleRuntime FireCycle;

#if !UE_BUILD_SHIPPING
	void RecordDebugShot(const FDMShotResult&, float, const FVector* = nullptr, const FVector* = nullptr) {}
#endif

private:
	UDMCombatKnowledgeComponent* ResolveKnowledge() const;
	void LogStartFireFailure(EDMFireAttemptFailure Failure);
	void RefreshConditionBasedRetry();
	void LogRangeRejection(const FDMFireRangeEvaluation& Evaluation,
		const ADMWeaponActor* Weapon, AActor* Target) const;
	static float RetryDelayForFailure(EDMFireAttemptFailure Failure);
	UPROPERTY(Transient) TWeakObjectPtr<AActor> FireTarget;
	uint64 FireAttemptId = 0;
	bool bAttemptConsumed = false;
	TSet<uint64> FireRequests;
	uint64 NextFireRequestId = 1;
	bool bActionAllowsFire = false;
	bool bActionAllowsFireWhileMoving = false;
	EDMFireAttemptFailure LastLoggedStartFailure = EDMFireAttemptFailure::None;
	double NextStartFailureLogTime = 0.0;
	float RetryAfterTime = 0.f;
	EDMFireAttemptFailure RetryFailure = EDMFireAttemptFailure::None;
	bool bRetryRequiresConditionChange = false;
	mutable bool bHasClearShotObservation = false;
	mutable EDMClearShotFailure LastClearShotFailure = EDMClearShotFailure::InvalidTarget;
	mutable TWeakObjectPtr<AActor> LastRangeLogTarget;
	mutable double NextRangeLogTime = 0.0;
};
