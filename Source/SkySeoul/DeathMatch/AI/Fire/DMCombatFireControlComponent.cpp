#include "DMCombatFireControlComponent.h"

#include "DeathMatch/AI/Combat/DMCombatFire_Adaptive.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/AI/Execution/DMActionIntentTypes.h"
#include "DeathMatch/Abilities/DMGameplayAbility_Fire.h"
#include "DeathMatch/Character/DeathMatchRobotCharacter.h"
#include "DeathMatch/Character/DMAimTargetComponent.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "DeathMatch/Weapons/DMWeaponActor.h"
#include "DeathMatch/Weapons/DMAimZoomComponent.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"
#include "PartsCharacter/PartsComponent.h"
#include "PartsCharacter/PartsHitCollision.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"

namespace
{
EDMFireAttemptFailure EvaluateResolvedLineOfFire(UWorld* World, const AActor* Shooter,
	FGenericTeamId ShooterTeam, const FDMFireRangeEvaluation& Range,
	AActor* IntendedTarget, ECollisionChannel TraceChannel)
{
	if (!Range.bAllowed) return EDMFireAttemptFailure::Range;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DMSharedLineOfFire), false, Shooter);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit, Range.Start, Range.End, TraceChannel, Params);
	return UDMCombatFireControlComponent::ClassifyLineOfFireHit(
		ShooterTeam, Hit.GetActor(), IntendedTarget, bHit);
}
}

UDMCombatFireControlComponent::UDMCombatFireControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	FireModule = CreateDefaultSubobject<UDMCombatFire_Adaptive>(TEXT("AdaptiveFirePolicy"));
}

UDMCombatKnowledgeComponent* UDMCombatFireControlComponent::ResolveKnowledge() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UDMCombatKnowledgeComponent>() : nullptr;
}

void UDMCombatFireControlComponent::SetTarget(AActor* NewTarget)
{
	if (FireTarget.Get() != NewTarget && bRetryRequiresConditionChange
		&& (RetryFailure == EDMFireAttemptFailure::InvalidTarget
			|| RetryFailure == EDMFireAttemptFailure::TargetDead
			|| RetryFailure == EDMFireAttemptFailure::TargetChanged))
	{
		ClearFireRetryDelay();
	}
	FireTarget = NewTarget;
}

bool UDMCombatFireControlComponent::StartFire()
{
	// An active Fire Ability already satisfies every outstanding request. Treat it as
	// maintained state instead of asking GAS to activate the same ability again.
	if (IsFireActive())
	{
		return true;
	}
	const EDMFireAttemptFailure Failure = GetFireFailure();
	if (Failure != EDMFireAttemptFailure::None)
	{
		// Cadence is an expected wait state, not a warning or failed attempt.
		if (Failure == EDMFireAttemptFailure::Cadence) return false;
		RecordFailedFireAttempt(Failure);
		LogStartFireFailure(Failure);
		return false;
	}
	LastLoggedStartFailure = EDMFireAttemptFailure::None;
	const uint64 AttemptId = EnsureFireAttempt();
	UE_LOG(LogDM, Log, TEXT("FireCycle Ready attempt=%llu cycle=%d owner=%s next=%.3f retry=%.3f"),
		AttemptId, FireCycle.CycleId, *GetNameSafe(GetOwner()), FireCycle.NextShotTime, RetryAfterTime);
	ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(GetOwner());
	ADeathMatchRobotCharacter* Robot = AI ? Cast<ADeathMatchRobotCharacter>(AI->GetPawn()) : nullptr;
	const bool bActivated = Robot && Robot->TryActivateDeathMatchFire();
	if (!bActivated)
	{
		RecordFailedFireAttempt(EDMFireAttemptFailure::Commit);
		UE_LOG(LogDM, Log, TEXT("ShotExecuted attempt=%llu cycle=%d result=false reason=AbilityRejected"),
			AttemptId, FireCycle.CycleId);
	}
	return bActivated;
}

void UDMCombatFireControlComponent::LogStartFireFailure(EDMFireAttemptFailure Failure)
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Failure == LastLoggedStartFailure && Now < NextStartFailureLogTime)
	{
		return;
	}
	LastLoggedStartFailure = Failure;
	NextStartFailureLogTime = Now + 1.0;

	const ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(GetOwner());
	const APawn* Pawn = AI ? AI->GetPawn() : nullptr;
	const UDMCombatKnowledgeComponent* Knowledge = ResolveKnowledge();
	const UDMAimZoomComponent* AimZoom = UDMAimZoomComponent::FindOnActor(Pawn);
	UE_LOG(LogDM, Warning,
		TEXT("DM Fire Blocked owner=%s reason=%s requests=%d actionFire=%d movingFire=%d aiming=%d speed=%.1f target=%s visible=%d weapon=%s"),
		*GetNameSafe(GetOwner()), DMFireAttempt::Name(Failure), FireRequests.Num(),
		bActionAllowsFire, bActionAllowsFireWhileMoving, AimZoom ? AimZoom->IsAiming() : 1,
		Pawn ? Pawn->GetVelocity().Size2D() : 0.0,
		*GetNameSafe(FireTarget.Get()), Knowledge && Knowledge->IsCombatTargetVisible(),
		*GetNameSafe(AI ? AI->GetActiveRangeWeapon() : nullptr));
}

uint64 UDMCombatFireControlComponent::BeginFireRequest()
{
	const uint64 RequestId = NextFireRequestId++;
	FireRequests.Add(RequestId);
	UE_LOG(LogDM, Log, TEXT("DM Fire Request owner=%s event=Begin id=%llu count=%d target=%s"),
		*GetNameSafe(GetOwner()), RequestId, FireRequests.Num(), *GetNameSafe(FireTarget.Get()));
	RefreshFireRequest(RequestId);
	return RequestId;
}

void UDMCombatFireControlComponent::RefreshFireRequest(uint64 RequestId)
{
	const bool bOwnsRequest = RequestId != 0 && FireRequests.Contains(RequestId);
	if (!bOwnsRequest)
	{
		return;
	}
	if (const UDMCombatKnowledgeComponent* Knowledge = ResolveKnowledge())
	{
		SetTarget(Knowledge->GetCombatTarget());
	}
	RefreshConditionBasedRetry();
	if (ShouldAttemptFireForRequest(bOwnsRequest, IsFireActive(), IsFireCycleReady()))
	{
		StartFire();
	}
}

void UDMCombatFireControlComponent::EndFireRequest(uint64 RequestId)
{
	const bool bRemoved = RequestId != 0 && FireRequests.Remove(RequestId) > 0;
	if (bRemoved)
	{
		UE_LOG(LogDM, Log, TEXT("DM Fire Request owner=%s event=End id=%llu count=%d stop=%d"),
			*GetNameSafe(GetOwner()), RequestId, FireRequests.Num(), FireRequests.IsEmpty());
	}
	if (bRemoved && FireRequests.IsEmpty())
	{
		StopFire();
	}
}

void UDMCombatFireControlComponent::ClearFireRequests()
{
	FireRequests.Reset();
	StopFire();
}

bool UDMCombatFireControlComponent::IntentAllowsFire(EDMActionIntent Intent)
{
	return Intent == EDMActionIntent::Attack || Intent == EDMActionIntent::AttackMove
		|| Intent == EDMActionIntent::CombatCover || Intent == EDMActionIntent::Retreat;
}

bool UDMCombatFireControlComponent::IntentAllowsMovingFire(EDMActionIntent Intent)
{
	return Intent == EDMActionIntent::Attack || Intent == EDMActionIntent::AttackMove
		|| Intent == EDMActionIntent::CombatCover;
}

void UDMCombatFireControlComponent::SetActionIntent(EDMActionIntent Intent, TOptional<bool> AllowMovingFireOverride)
{
	const bool bPreviousAllowsFire = bActionAllowsFire;
	const bool bPreviousAllowsMovingFire = bActionAllowsFireWhileMoving;
	bActionAllowsFire = IntentAllowsFire(Intent);
	bActionAllowsFireWhileMoving = AllowMovingFireOverride.IsSet()
		? AllowMovingFireOverride.GetValue() : IntentAllowsMovingFire(Intent);
	if (bPreviousAllowsFire != bActionAllowsFire || bPreviousAllowsMovingFire != bActionAllowsFireWhileMoving)
	{
		UE_LOG(LogDM, Log, TEXT("DM Fire Intent owner=%s intent=%d actionFire=%d movingFire=%d requests=%d"),
			*GetNameSafe(GetOwner()), static_cast<int32>(Intent), bActionAllowsFire,
			bActionAllowsFireWhileMoving, FireRequests.Num());
	}
	if (!bActionAllowsFire)
	{
		StopFire();
	}
}

void UDMCombatFireControlComponent::StopFire()
{
	const ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(GetOwner());
	UAbilitySystemComponent* ASC = AI && AI->GetPawn()
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AI->GetPawn()) : nullptr;
	if (!ASC)
	{
		return;
	}
	TArray<FGameplayAbilitySpecHandle> Handles;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.Ability && Spec.Ability->IsA<UDMGameplayAbility_Fire>())
		{
			Handles.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle Handle : Handles)
	{
		ASC->CancelAbilityHandle(Handle);
	}
}

bool UDMCombatFireControlComponent::IsFireActive() const
{
	const ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(GetOwner());
	const UAbilitySystemComponent* ASC = AI && AI->GetPawn()
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AI->GetPawn()) : nullptr;
	if (!ASC)
	{
		return false;
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Spec.Ability && Spec.Ability->IsA<UDMGameplayAbility_Fire>())
		{
			return true;
		}
	}
	return false;
}

EDMFireAttemptFailure UDMCombatFireControlComponent::GetFireSafetyFailure(bool bCheckMovement) const
{
	const ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(GetOwner());
	const APawn* Pawn = AI ? AI->GetPawn() : nullptr;
	AActor* Target = FireTarget.Get();
	if (!AI || !Pawn) return EDMFireAttemptFailure::Context;
	if (!bEnabled || !bActionAllowsFire) return EDMFireAttemptFailure::CannotShoot;
	if (!IsValid(Target)) return EDMFireAttemptFailure::InvalidTarget;
	if (!AI->CanShoot()) return EDMFireAttemptFailure::CannotShoot;
	const UDMCombatKnowledgeComponent* Knowledge = ResolveKnowledge();
	if (!Knowledge || Knowledge->GetCombatTarget() != Target || !Knowledge->IsCombatTargetVisible())
		return EDMFireAttemptFailure::NotSensed;
	const ADMWeaponActor* Weapon = AI->GetActiveRangeWeapon();
	if (!Weapon) return EDMFireAttemptFailure::MissingWeapon;
	FVector FireOrigin;
	if (!ResolveCurrentFireOrigin(AI, FireOrigin)) return EDMFireAttemptFailure::Context;
	const FDMFireRangeEvaluation Range = ResolveFireRange(
		Pawn, Target, FireOrigin, Weapon->GetMaxFireRange());
	if (!Range.bAllowed)
	{
		LogRangeRejection(Range, Weapon, Target);
		return EDMFireAttemptFailure::Range;
	}
	const EDMFireAttemptFailure LineFailure = EvaluateResolvedLineOfFire(
		AI->GetWorld(), Pawn, AI->GetGenericTeamId(), Range, Target, ResolveWeaponTraceChannel(AI));
	if (LineFailure != EDMFireAttemptFailure::None) return LineFailure;
	if (bCheckMovement)
	{
		const double Speed = Pawn->GetVelocity().Size2D();
		if ((!bActionAllowsFireWhileMoving && Speed > 1.0 + UE_KINDA_SMALL_NUMBER)
			|| (FireModule && !FireModule->AllowsMoveSpeed(Speed)))
		{
			return EDMFireAttemptFailure::MoveSpeed;
		}
	}
	if (const UDMAimZoomComponent* AimZoom = UDMAimZoomComponent::FindOnActor(Pawn);
		AimZoom && !AimZoom->IsAiming())
	{
		return EDMFireAttemptFailure::AimSettle;
	}
	return EDMFireAttemptFailure::None;
}

bool UDMCombatFireControlComponent::IsFriendlyFireBlocker(
	FGenericTeamId ShooterTeam, const AActor* HitActor, const AActor* IntendedTarget)
{
	return IsValid(HitActor) && HitActor != IntendedTarget
		&& DeathMatchTeamUtils::GetAttitudeTowards(ShooterTeam, *HitActor) == ETeamAttitude::Friendly;
}

ECollisionChannel UDMCombatFireControlComponent::ResolveWeaponTraceChannel(const ADeathMatchRobotAIController* AI)
{
	const UPartsComponent* Parts = AI ? AI->GetPartsComponent() : nullptr;
	return Parts ? Parts->WeaponTraceChannel.GetValue() : PartsHitCollision::DefaultWeaponTraceChannel;
}

bool UDMCombatFireControlComponent::ResolveCurrentFireOrigin(
	const ADeathMatchRobotAIController* AI, FVector& OutOrigin)
{
	if (!AI || !AI->GetPawn()) return false;
	if (const ADMWeaponActor* Weapon = AI->GetActiveRangeWeapon(); Weapon && Weapon->GetWorld())
	{
		OutOrigin = Weapon->GetMuzzleLocation();
		if (!OutOrigin.ContainsNaN()) return true;
	}
	OutOrigin = AI->GetPawn()->GetPawnViewLocation();
	return !OutOrigin.ContainsNaN();
}

FVector UDMCombatFireControlComponent::ProjectFireOriginOffset(const FVector& PawnLocation,
	const FVector& CurrentFireOrigin, const FVector& CandidateLocation)
{
	return CandidateLocation + (CurrentFireOrigin - PawnLocation);
}

FVector UDMCombatFireControlComponent::ResolveCandidateFireOrigin(
	const ADeathMatchRobotAIController* AI, const FVector& CandidateLocation, float FallbackEyeHeight)
{
	FVector CurrentOrigin;
	if (ResolveCurrentFireOrigin(AI, CurrentOrigin) && AI && AI->GetPawn())
	{
		return ProjectFireOriginOffset(AI->GetPawn()->GetActorLocation(), CurrentOrigin, CandidateLocation);
	}
	return CandidateLocation + FVector::UpVector * FMath::Max(0.f, FallbackEyeHeight);
}

FVector UDMCombatFireControlComponent::ResolveIntendedTargetLocation(const AActor* Shooter,
	AActor* IntendedTarget, const FVector& FireOrigin)
{
	if (!IsValid(IntendedTarget)) return FVector::ZeroVector;
	if (const UDMAimTargetComponent* Aim = UDMAimTargetComponent::FindOnActor(IntendedTarget))
	{
		return Aim->GetCombatAimLocationFrom(FireOrigin, Shooter);
	}
	return IntendedTarget->GetActorLocation() + FVector::UpVector * 60.f;
}

FDMFireRangeEvaluation UDMCombatFireControlComponent::EvaluateFireRange(
	const FVector& Start, const FVector& End, float MaximumRange)
{
	FDMFireRangeEvaluation Result;
	Result.Start = Start;
	Result.End = End;
	Result.Distance = FVector::Distance(Start, End);
	Result.MaximumRange = FMath::Max(0.f, MaximumRange);
	Result.bAllowed = FMath::IsFinite(Result.Distance)
		&& Result.Distance <= Result.MaximumRange + FireRangeToleranceCm;
	return Result;
}

FDMFireRangeEvaluation UDMCombatFireControlComponent::ResolveFireRange(
	const AActor* Shooter, AActor* IntendedTarget, const FVector& FireOrigin, float MaximumRange)
{
	return EvaluateFireRange(FireOrigin,
		ResolveIntendedTargetLocation(Shooter, IntendedTarget, FireOrigin), MaximumRange);
}

EDMFireAttemptFailure UDMCombatFireControlComponent::ClassifyLineOfFireHit(
	FGenericTeamId ShooterTeam, const AActor* HitActor, const AActor* IntendedTarget, bool bHadBlockingHit)
{
	if (!bHadBlockingHit || !IsValid(HitActor)) return EDMFireAttemptFailure::Sight;
	if (HitActor == IntendedTarget) return EDMFireAttemptFailure::None;
	return IsFriendlyFireBlocker(ShooterTeam, HitActor, IntendedTarget)
		? EDMFireAttemptFailure::FriendlyFireRisk : EDMFireAttemptFailure::Sight;
}

EDMFireAttemptFailure UDMCombatFireControlComponent::EvaluateLineOfFire(UWorld* World,
	const AActor* Shooter, FGenericTeamId ShooterTeam, const FVector& Start,
	AActor* IntendedTarget, float MaximumRange, ECollisionChannel TraceChannel)
{
	if (!World || !IsValid(IntendedTarget)) return EDMFireAttemptFailure::InvalidTarget;
	const FDMFireRangeEvaluation Range = ResolveFireRange(Shooter, IntendedTarget, Start, MaximumRange);
	return EvaluateResolvedLineOfFire(
		World, Shooter, ShooterTeam, Range, IntendedTarget, TraceChannel);
}

void UDMCombatFireControlComponent::LogRangeRejection(
	const FDMFireRangeEvaluation& Evaluation, const ADMWeaponActor* Weapon, AActor* Target) const
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (LastRangeLogTarget.Get() == Target && Now < NextRangeLogTime)
	{
		return;
	}
	LastRangeLogTarget = Target;
	NextRangeLogTime = Now + 1.0;
	UE_LOG(LogDM, Log,
		TEXT("ShotRangeRejected attempt=%llu start=%s end=%s distance=%.2f max=%.2f weapon=%s fireControl=1"),
		FireAttemptId, *Evaluation.Start.ToCompactString(), *Evaluation.End.ToCompactString(),
		Evaluation.Distance, Evaluation.MaximumRange, *GetNameSafe(Weapon));
}

EDMClearShotFailure UDMCombatFireControlComponent::ClassifyClearShotFailure(
	bool bHasValidContext, bool bHasSelectedTarget, bool bSelectedTargetVisible,
	bool bHasActiveRangeWeapon, EDMFireAttemptFailure LineOfFireFailure)
{
	if (!bHasValidContext || !bHasSelectedTarget) return EDMClearShotFailure::InvalidTarget;
	if (!bSelectedTargetVisible) return EDMClearShotFailure::NotVisible;
	if (!bHasActiveRangeWeapon) return EDMClearShotFailure::MissingWeapon;
	switch (LineOfFireFailure)
	{
	case EDMFireAttemptFailure::None: return EDMClearShotFailure::None;
	case EDMFireAttemptFailure::Range: return EDMClearShotFailure::Range;
	case EDMFireAttemptFailure::FriendlyFireRisk: return EDMClearShotFailure::FriendlyFireRisk;
	default: return EDMClearShotFailure::Sight;
	}
}

const TCHAR* UDMCombatFireControlComponent::GetClearShotFailureName(EDMClearShotFailure Failure)
{
	switch (Failure)
	{
	case EDMClearShotFailure::None: return TEXT("None");
	case EDMClearShotFailure::InvalidTarget: return TEXT("InvalidTarget");
	case EDMClearShotFailure::NotVisible: return TEXT("NotVisible");
	case EDMClearShotFailure::MissingWeapon: return TEXT("MissingWeapon");
	case EDMClearShotFailure::Range: return TEXT("Range");
	case EDMClearShotFailure::Sight: return TEXT("Sight");
	case EDMClearShotFailure::FriendlyFireRisk: return TEXT("FriendlyFireRisk");
	}
	return TEXT("Sight");
}

EDMClearShotFailure UDMCombatFireControlComponent::GetClearShotFailure() const
{
	const ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(GetOwner());
	const APawn* Pawn = AI ? AI->GetPawn() : nullptr;
	const UDMCombatKnowledgeComponent* Knowledge = ResolveKnowledge();
	AActor* Target = Knowledge ? Knowledge->GetCombatTarget() : nullptr;
	const bool bHasContext = AI && Pawn;
	const bool bHasTarget = IsValid(Target);
	const bool bTargetVisible = bHasTarget && Knowledge && Knowledge->IsCombatTargetVisible();
	const ADMWeaponActor* Weapon = AI ? AI->GetActiveRangeWeapon() : nullptr;
	const bool bHasWeapon = IsValid(Weapon);
	EDMFireAttemptFailure LineFailure = EDMFireAttemptFailure::None;
	if (bHasContext && bHasTarget && bTargetVisible && bHasWeapon)
	{
		FVector FireOrigin;
		LineFailure = ResolveCurrentFireOrigin(AI, FireOrigin)
			? EvaluateLineOfFire(AI->GetWorld(), Pawn, AI->GetGenericTeamId(), FireOrigin,
				Target, Weapon->GetMaxFireRange(), ResolveWeaponTraceChannel(AI))
			: EDMFireAttemptFailure::Context;
	}
	LastClearShotFailure = ClassifyClearShotFailure(
		bHasContext, bHasTarget, bTargetVisible, bHasWeapon, LineFailure);
	bHasClearShotObservation = true;
	return LastClearShotFailure;
}

float UDMCombatFireControlComponent::GetTimeUntilFireReady() const
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	return FMath::Max(0.f, FMath::Max(FireCycle.NextShotTime, RetryAfterTime) - Now);
}

bool UDMCombatFireControlComponent::IsFireCycleReady() const
{
	const UWorld* World = GetWorld();
	return IsFireCycleReadyAt(World ? World->GetTimeSeconds() : 0.f);
}

bool UDMCombatFireControlComponent::IsFireCycleReadyAt(float Now) const
{
	return IsFireTimingReadyAt(Now, FireCycle.NextShotTime, RetryAfterTime);
}

float UDMCombatFireControlComponent::RetryDelayForFailure(EDMFireAttemptFailure Failure)
{
	switch (Failure)
	{
	case EDMFireAttemptFailure::Movement:
	case EDMFireAttemptFailure::MoveSpeed:
	case EDMFireAttemptFailure::AimSettle:
		return 0.f;
	case EDMFireAttemptFailure::Sight:
	case EDMFireAttemptFailure::FriendlyFireRisk:
		return .1f;
	default:
		return .075f;
	}
}

void UDMCombatFireControlComponent::RecordFailedFireAttempt(EDMFireAttemptFailure Failure)
{
	if (const UWorld* World = GetWorld())
	{
		RecordFailedFireAttemptAt(Failure, World->GetTimeSeconds());
	}
}

void UDMCombatFireControlComponent::RecordFailedFireAttemptAt(
	EDMFireAttemptFailure Failure, float Now)
{
	if (Failure == EDMFireAttemptFailure::None || Failure == EDMFireAttemptFailure::Cadence)
	{
		return;
	}
	RetryFailure = Failure;
	bRetryRequiresConditionChange = Failure == EDMFireAttemptFailure::MissingWeapon
		|| Failure == EDMFireAttemptFailure::InvalidTarget
		|| Failure == EDMFireAttemptFailure::TargetDead
		|| Failure == EDMFireAttemptFailure::AvatarDead
		|| Failure == EDMFireAttemptFailure::Montage
		|| Failure == EDMFireAttemptFailure::NoNotifyReceived
		|| Failure == EDMFireAttemptFailure::MissingFireCycleNotify;
	RetryAfterTime = bRetryRequiresConditionChange
		? TNumericLimits<float>::Max()
		: FMath::Max(RetryAfterTime, Now + RetryDelayForFailure(Failure));
}

void UDMCombatFireControlComponent::ClearFireRetryDelay()
{
	RetryAfterTime = 0.f;
	RetryFailure = EDMFireAttemptFailure::None;
	bRetryRequiresConditionChange = false;
}

void UDMCombatFireControlComponent::RefreshConditionBasedRetry()
{
	if (!bRetryRequiresConditionChange)
	{
		return;
	}
	const ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(GetOwner());
	bool bConditionChanged = false;
	switch (RetryFailure)
	{
	case EDMFireAttemptFailure::MissingWeapon:
		bConditionChanged = AI && IsValid(AI->GetActiveRangeWeapon());
		break;
	case EDMFireAttemptFailure::InvalidTarget:
		bConditionChanged = IsValid(FireTarget.Get());
		break;
	case EDMFireAttemptFailure::AvatarDead:
		bConditionChanged = AI && AI->CanShoot();
		break;
	default:
		// Montage/notify configuration failures stay suspended until Fire Control resets.
		break;
	}
	if (bConditionChanged)
	{
		ClearFireRetryDelay();
	}
}

EDMFireAttemptFailure UDMCombatFireControlComponent::GetFireFailure(bool bIgnoreCadence, bool bIgnoreMovementPolicy) const
{
	const EDMFireAttemptFailure Safety = GetFireSafetyFailure(!bIgnoreMovementPolicy);
	if (Safety != EDMFireAttemptFailure::None) return Safety;
	if (!bIgnoreCadence && !IsFireCycleReady())
		return EDMFireAttemptFailure::Cadence;
	return EDMFireAttemptFailure::None;
}

bool UDMCombatFireControlComponent::AuthorizeFireCycle(bool bNewActivation)
{
	// Inner Burst/Automatic cycles pre-roll their montage so Fire Notify lands on
	// NextShotTime. Only a new Ability activation consumes the shared timing gate.
	const EDMFireAttemptFailure Failure = GetFireFailure(!bNewActivation);
	FireCycle.LastFailureReason = Failure;
	if (Failure != EDMFireAttemptFailure::None) return false;
	if (bNewActivation && FireModule)
	{
		FireCycle.Select(FireModule->GetCycleSettings(), true);
		FireModule->SetNextShotAllowedTime(FireCycle.NextShotTime);
	}
	return true;
}

void UDMCombatFireControlComponent::NotifyShotFired()
{
	if (!GetWorld() || !FireModule) return;
	const float Now = GetWorld()->GetTimeSeconds();
	ClearFireRetryDelay();
	FireCycle.RecordSuccessfulShot(Now);
	FireModule->SetNextShotAllowedTime(FireCycle.NextShotTime);
	UE_LOG(LogDM, Log, TEXT("FireCooldown Committed attempt=%llu cycle=%d next=%.3f"),
		FireAttemptId, FireCycle.CycleId, FireCycle.NextShotTime);
	PublishFireCycle();
}

void UDMCombatFireControlComponent::PublishFireCycle()
{
	if (UDMCombatFire_Adaptive* Adaptive = Cast<UDMCombatFire_Adaptive>(FireModule))
	{
		Adaptive->ObserveCycle(FireCycle);
	}
}

void UDMCombatFireControlComponent::ResetFireControl()
{
	ClearFireRequests();
	FireTarget.Reset();
	FireCycle = FDMFireCycleRuntime();
	if (FireModule)
	{
		FireModule->Reset();
		FireModule->SetNextShotAllowedTime(FireCycle.NextShotTime);
	}
	FireAttemptId = 0;
	bAttemptConsumed = false;
	bActionAllowsFire = false;
	bActionAllowsFireWhileMoving = false;
	LastLoggedStartFailure = EDMFireAttemptFailure::None;
	NextStartFailureLogTime = 0.0;
	ClearFireRetryDelay();
	bHasClearShotObservation = false;
	LastClearShotFailure = EDMClearShotFailure::InvalidTarget;
}

uint64 UDMCombatFireControlComponent::EnsureFireAttempt()
{
	if (!FireAttemptId || (bAttemptConsumed && !IsFireActive()))
	{
		FireAttemptId = DMFireAttempt::NextId();
		bAttemptConsumed = false;
	}
	return FireAttemptId;
}
