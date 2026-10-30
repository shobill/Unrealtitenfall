#include "DeathMatchRobotAIController.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/Movement/DMMovementSpeedPolicyComponent.h"
#include "DeathMatch/AI/Execution/DMActionIntentComponent.h"
#include "DeathMatch/AI/Execution/DMCombatFocusComponent.h"
#include "DeathMatch/AI/State/DMCombatStatusComponent.h"
#include "DeathMatch/AI/Targeting/DMCombatTargetSelectionPolicy.h"
#include "DeathMatch/Core/DeathMatchLog.h"

#include "DeathMatch/Character/DeathMatchRobotCharacter.h"
#include "DeathMatch/PlayerState/DeathMatchPlayerState.h"
#include "DeathMatch/Weapons/DMWeaponActor.h"
#include "DeathMatch/Weapons/DMWeaponLoadoutComponent.h"
#include "PartsCharacter/PartsComponent.h"
#include "DeathMatch/AIModules/AIModule_Sense.h"
#include "DeathMatch/Navigation/DMMovePoint.h"
#include "DeathMatch/Navigation/DMTacticalAreaSubsystem.h"
#include "DeathMatch/AI/Move/DMCombatMoveDirector.h"
#include "DeathMatch/AI/Combat/DMCombatPositionRegistry.h"
#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/Navigation/DMNavigationSafety.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"

ADeathMatchRobotAIController::ADeathMatchRobotAIController()
{
	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
	SetPerceptionComponent(*AIPerceptionComponent);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SenseModule = CreateDefaultSubobject<UAIModule_Sense>(TEXT("SenseModule"));
	CombatMoveDirector = CreateDefaultSubobject<UDMCombatMoveDirector>(TEXT("CombatMoveDirector"));
	CombatKnowledge = CreateDefaultSubobject<UDMCombatKnowledgeComponent>(TEXT("CombatKnowledge"));
	CombatFireControl = CreateDefaultSubobject<UDMCombatFireControlComponent>(TEXT("CombatFireControl"));
	MovementSpeedPolicy = CreateDefaultSubobject<UDMMovementSpeedPolicyComponent>(TEXT("MovementSpeedPolicy"));
	ActionIntentComponent = CreateDefaultSubobject<UDMActionIntentComponent>(TEXT("ActionIntent"));
	CombatFocusComponent = CreateDefaultSubobject<UDMCombatFocusComponent>(TEXT("CombatFocus"));
	CombatStatus = CreateDefaultSubobject<UDMCombatStatusComponent>(TEXT("CombatStatus"));
	TargetSelectionPolicy = CreateDefaultSubobject<UDMCombatTargetSelectionPolicy>(TEXT("TargetSelectionPolicy"));

	SetupPerception();
}

void ADeathMatchRobotAIController::SetupPerception()
{
	if (!SightConfig || !AIPerceptionComponent)
	{
		return;
	}

	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = SightRadius + LoseSightRadiusOffset;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
	SightConfig->SetMaxAge(SightMaxAge);
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;

	AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	AIPerceptionComponent->ConfigureSense(*SightConfig);
}

TSubclassOf<APawn> ADeathMatchRobotAIController::GetAIPawnClass() const
{
	return AICharacterClass;
}

void ADeathMatchRobotAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	RefreshRobotCache(InPawn);
	if (CachedRobot.IsValid())
	{
		CachedRobot->SynchronizeNavAgentWithCapsule();
		CachedRobot->SetDeathNavigationState(false);
		CachedRobot->RecheckNavigationCompatibility(true);
	}
	SetNavigationMovementMode(false);

	if (ADeathMatchPlayerState* DMPS = GetPlayerState<ADeathMatchPlayerState>())
	{
		CachedTeamId = DMPS->GetGenericTeamId();
	}
	if (UDMTacticalAreaSubsystem* Areas = GetWorld() ? GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr)
	{
		Areas->ReportCharacterSpawn(InPawn, GetGenericTeamId().GetId());
	}

	if (CombatKnowledge) CombatKnowledge->ResetKnowledge();
	if (CombatFireControl) CombatFireControl->ResetFireControl();
	if (ActionIntentComponent) ActionIntentComponent->ResetIntentRequests();
	if (CombatFocusComponent) CombatFocusComponent->ResetFocusRequests();
	if (CombatStatus) CombatStatus->RefreshStatus();
	if (MovementSpeedPolicy) MovementSpeedPolicy->ClearAllSpeedMultipliers();
	RunDeathMatchBehaviorTree();
}

void ADeathMatchRobotAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	const FVector CompletedGoal = CurrentMoveDestination;
	const bool bHadMoveOwner = !CurrentMoveOwner.IsNone();
	Super::OnMoveCompleted(RequestID, Result);

	// Ignore abort — happens when we issue a new MoveTo while replacing the goal.
	if (Result.Code == EPathFollowingResult::Aborted)
	{
		CurrentMoveOwner = NAME_None;
		return;
	}
	if (!Result.IsSuccess() && bHadMoveOwner)
	{
		RecordMoveFailure(CompletedGoal, TEXT("PathFollowingFailed"));
	}
	CurrentMoveOwner = NAME_None;

	if (UDMCombatMoveDirector* Director = GetCombatMoveDirector())
		Director->NotifyPathFollowingCompleted(Result.IsSuccess());


	if (Result.IsSuccess())
	{
		UE_LOG(LogDM, Verbose, TEXT("DM Roam: %s MoveFinished"), *GetName());
	}
	else
	{
		UE_LOG(LogDM, Warning, TEXT("DM Roam: %s MoveFailed code=%d"), *GetName(), static_cast<int32>(Result.Code));
	}

}

void ADeathMatchRobotAIController::RunDeathMatchBehaviorTree()
{
	if (!RootBehaviorTree)
	{
		UE_LOG(LogDM, Warning,
			TEXT("DM AI: %s has no RootBehaviorTree; controller remains safely idle."),
			*GetName());
		return;
	}
	RunBehaviorTree(RootBehaviorTree);
}

void ADeathMatchRobotAIController::OnUnPossess()
{
	APawn* PreviousPawn = GetPawn();
    if (SenseModule) SenseModule->ResetAwareness();
	if (CombatKnowledge) CombatKnowledge->ResetKnowledge();
	if (CombatFireControl) CombatFireControl->ResetFireControl();
	if (ActionIntentComponent) ActionIntentComponent->ResetIntentRequests();
	if (CombatFocusComponent) CombatFocusComponent->ResetFocusRequests();
	if (CombatStatus) CombatStatus->ResetStatus();
	if (MovementSpeedPolicy) MovementSpeedPolicy->ClearAllSpeedMultipliers();
	if (CombatMoveDirector) CombatMoveDirector->CancelAllMoveRequests(true);
	if (UDMTacticalAreaSubsystem* Areas = GetWorld() ? GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr)
	{
		Areas->RemoveCharacter(PreviousPawn);
	}
	ClearRobotCache();
	Super::OnUnPossess();
}

void ADeathMatchRobotAIController::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	Super::SetGenericTeamId(NewTeamID);
	CachedTeamId = NewTeamID;
	if (ADeathMatchPlayerState* DMPS = GetPlayerState<ADeathMatchPlayerState>())
	{
		DMPS->SetGenericTeamId(NewTeamID);
	}
}

FGenericTeamId ADeathMatchRobotAIController::GetGenericTeamId() const
{
	if (const ADeathMatchPlayerState* DMPS = GetPlayerState<ADeathMatchPlayerState>())
	{
		return DMPS->GetGenericTeamId();
	}
	return CachedTeamId;
}

ETeamAttitude::Type ADeathMatchRobotAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// Perception Sight affiliation path — not a TeamId equality check on the pawn.
	const ETeamAttitude::Type Attitude = DeathMatchTeamUtils::GetAttitudeTowards(GetGenericTeamId(), Other);

	UE_LOG(LogDM, Verbose,
		TEXT("DM Team Attitude: %s -> %s = %s (self=%u other=%u)"),
		*GetName(),
		*GetNameSafe(&Other),
		DeathMatchTeamUtils::AttitudeToString(Attitude),
		GetGenericTeamId().GetId(),
		DeathMatchTeamUtils::GetTeamIdFromActor(&Other).GetId());

	return Attitude;
}

void ADeathMatchRobotAIController::RefreshRobotCache(APawn* InPawn)
{
	CachedRobot = Cast<ADeathMatchRobotCharacter>(InPawn);
	CachedParts = CachedRobot.IsValid() ? CachedRobot->GetPartsComponent() : nullptr;
}

void ADeathMatchRobotAIController::ClearRobotCache()
{
	CachedRobot.Reset();
	CachedParts.Reset();
	CachedRangeWeapon.Reset();
}

ADMWeaponActor* ADeathMatchRobotAIController::GetActiveRangeWeapon() const
{
	const APawn* ControlledPawn = GetPawn();
	const UDMWeaponLoadoutComponent* Loadout = ControlledPawn ? ControlledPawn->FindComponentByClass<UDMWeaponLoadoutComponent>() : nullptr;
	ADMWeaponActor* ActiveWeapon = Loadout ? Loadout->GetActiveWeaponActor() : nullptr;
	if (ActiveWeapon != CachedRangeWeapon.Get())
	{
		CachedRangeWeapon = ActiveWeapon;
		if (ActiveWeapon && SightConfig && SightConfig->SightRadius < ActiveWeapon->GetMaxFireRange())
		{
			UE_LOG(LogDM, Warning, TEXT("DM Weapon: %s SightRadius %.0f is below %s MaxFireRange %.0f; consider increasing sight radius."),
				*GetName(), SightConfig->SightRadius, *ActiveWeapon->GetName(), ActiveWeapon->GetMaxFireRange());
		}
	}
	return ActiveWeapon;
}

float ADeathMatchRobotAIController::GetActiveWeaponEffectiveRange() const
{
	const ADMWeaponActor* Weapon = GetActiveRangeWeapon();
	return Weapon ? Weapon->GetEffectiveRange() : 3000.0f;
}

float ADeathMatchRobotAIController::GetActiveWeaponMaxFireRange() const
{
	const ADMWeaponActor* Weapon = GetActiveRangeWeapon();
	return Weapon ? Weapon->GetMaxFireRange() : 3000.0f;
}

bool ADeathMatchRobotAIController::CanShoot() const
{
	if (CachedRobot.IsValid())
	{
		return CachedRobot->CanShoot();
	}
	return true;
}

EPartsArmState ADeathMatchRobotAIController::GetLeftArmState() const
{
	if (CachedRobot.IsValid())
	{
		return CachedRobot->GetLeftArmState();
	}
	return EPartsArmState::Intact;
}

EPartsArmState ADeathMatchRobotAIController::GetRightArmState() const
{
	if (CachedRobot.IsValid())
	{
		return CachedRobot->GetRightArmState();
	}
	return EPartsArmState::Intact;
}

bool ADeathMatchRobotAIController::IsHeadIntact() const
{
	if (CachedRobot.IsValid())
	{
		return CachedRobot->IsHeadIntact();
	}
	return true;
}

void ADeathMatchRobotAIController::NotifyPossessedRobotDied()
{
	if (UDMTacticalAreaSubsystem* Areas = GetWorld() ? GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr)
	{
		Areas->ReportCharacterDeath(GetPawn(), GetGenericTeamId().GetId());
		Areas->ReleaseIncomingAI(GetPawn());
	}
	if (UDMCombatPositionRegistry* Registry = GetWorld() ? GetWorld()->GetSubsystem<UDMCombatPositionRegistry>() : nullptr)
	{
		Registry->Release(this);
	}
    if (SenseModule) SenseModule->ResetAwareness();
	if (CombatKnowledge) CombatKnowledge->ResetKnowledge();
	if (CombatFireControl) CombatFireControl->ResetFireControl();
	if (ActionIntentComponent) ActionIntentComponent->ResetIntentRequests();
	if (CombatFocusComponent) CombatFocusComponent->ResetFocusRequests();
	if (CombatStatus) CombatStatus->RefreshStatus();
	if (MovementSpeedPolicy) MovementSpeedPolicy->ClearAllSpeedMultipliers();
	if (CombatMoveDirector) CombatMoveDirector->CancelAllMoveRequests(true);
	if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent())
	{
		BlackboardComponent->ClearValue(DMBlackboardKeys::CombatTarget);
		BlackboardComponent->ClearValue(DMBlackboardKeys::TargetActor);
		BlackboardComponent->ClearValue(DMBlackboardKeys::SelectedTacticalArea);
		BlackboardComponent->ClearValue(DMBlackboardKeys::MoveGoal);
	}
	if (CachedRobot.IsValid())
	{
		CachedRobot->SetDeathNavigationState(true);
		CachedRobot->HandleOwnerDeathPartsExplosion();
	}
}

void ADeathMatchRobotAIController::NotifyPossessedRobotRespawnReset()
{
    if (SenseModule) SenseModule->ResetAwareness();
	if (CombatKnowledge) CombatKnowledge->ResetKnowledge();
	if (CombatFireControl) CombatFireControl->ResetFireControl();
	if (ActionIntentComponent) ActionIntentComponent->ResetIntentRequests();
	if (CombatFocusComponent) CombatFocusComponent->ResetFocusRequests();
	if (CombatStatus) CombatStatus->RefreshStatus();
	if (MovementSpeedPolicy) MovementSpeedPolicy->ClearAllSpeedMultipliers();
	if (CombatMoveDirector) CombatMoveDirector->CancelAllMoveRequests(true);
	if (UDMCombatPositionRegistry* Registry = GetWorld() ? GetWorld()->GetSubsystem<UDMCombatPositionRegistry>() : nullptr)
	{
		Registry->Release(this);
	}
	if (CachedRobot.IsValid())
	{
		CachedRobot->SetDeathNavigationState(false);
		CachedRobot->SynchronizeNavAgentWithCapsule();
		CachedRobot->RecheckNavigationCompatibility(true);
		CachedRobot->ResetAllParts();
	}
	if (UDMTacticalAreaSubsystem* Areas = GetWorld() ? GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr)
	{
		Areas->ReportCharacterSpawn(GetPawn(), GetGenericTeamId().GetId());
	}
}

AActor* ADeathMatchRobotAIController::GetFocusTarget() const
{
	return SenseModule ? SenseModule->GetFocusTarget() : nullptr;
}

FVector ADeathMatchRobotAIController::GetFocusLocation() const
{
	return SenseModule ? SenseModule->GetFocusLocation() : FVector::ZeroVector;
}

bool ADeathMatchRobotAIController::HasHostile() const
{
	return SenseModule && SenseModule->HasHostile();
}

AActor* ADeathMatchRobotAIController::FindBestHostile(bool bPreferSeenOnly) const
{
	return SenseModule ? SenseModule->FindBestHostile(bPreferSeenOnly) : nullptr;
}

void ADeathMatchRobotAIController::GetSeenHostiles(TArray<AActor*>& OutHostiles) const
{
	if (SenseModule)
	{
		SenseModule->GetSeenHostiles(OutHostiles);
	}
	else
	{
		OutHostiles.Reset();
	}
}

void ADeathMatchRobotAIController::GetKnownHostiles(TArray<AActor*>& OutHostiles) const
{
	if (SenseModule)
	{
		SenseModule->GetKnownHostiles(OutHostiles);
	}
	else
	{
		OutHostiles.Reset();
	}
}

bool ADeathMatchRobotAIController::GetAwareTarget(AActor* Target, FDMAwareTarget& OutData) const
{
	return SenseModule && SenseModule->GetAwareTarget(Target, OutData);
}

void ADeathMatchRobotAIController::SetLastMovePoint(ADMMovePoint* Point)
{
	LastMovePoint = Point;
}

void ADeathMatchRobotAIController::SetNavigationMovementMode(bool bCombat)
{
	ADeathMatchRobotCharacter* Robot = CachedRobot.Get();
	UCharacterMovementComponent* Movement = Robot ? Robot->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}
	// This controller deliberately uses RVO only. Do not combine it with Crowd Following.
	Movement->SetAvoidanceEnabled(!Robot->IsDead);
	Movement->AvoidanceWeight = FMath::Clamp(bCombat ? CombatAvoidanceWeight : RoamAvoidanceWeight, 0.0f, 1.0f);
	Movement->AvoidanceConsiderationRadius = FMath::Max(0.0f, AvoidanceConsiderationRadius);
}

bool ADeathMatchRobotAIController::ValidateMoveGoal(const FVector& Goal, bool bCombat, FVector& OutProjectedGoal, FName& OutFailureReason)
{
	OutProjectedGoal = Goal;
	OutFailureReason = NAME_None;
	LastNavigationState = TEXT("Pending");
	LastClearanceFailure = NAME_None;
	LastPathQuality = NAME_None;
	bLastDynamicObstacleDetected = false;
	bLastRaisedStepProbeUsed = false;
	APawn* ControlledPawn = GetPawn();
	const ACharacter* ControlledCharacter = Cast<ACharacter>(ControlledPawn);
	const UCharacterMovementComponent* Movement = ControlledCharacter ? ControlledCharacter->GetCharacterMovement() : nullptr;
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	auto RejectHardFailure = [&](FName Reason, const FDMClearanceResult* Clearance = nullptr)
	{
		OutFailureReason = Reason;
		if (Reason == TEXT("NavigationUnavailable") || Reason == TEXT("NavProjectionFailed")
			|| Reason == TEXT("InvalidOrPartialPath"))
		{
			LastNavigationState = Reason;
		}
		LastClearanceFailure = Reason;
		if (DMNavigationSafety::IsHardPathFailureReason(Reason))
		{
			RecordMoveFailure(Goal, Reason, false);
		}
		UE_LOG(LogDM, Warning,
			TEXT("DM Nav Blocked: AI=%s Goal=%s Reason=%s HitActor=%s HitComponent=%s ProbeRadius=%.1f ProbeHalfHeight=%.1f RaisedHeight=%.1f PathSegment=%d"),
			*GetName(), *Goal.ToCompactString(), *Reason.ToString(),
			*GetNameSafe(Clearance ? Clearance->HitActor.Get() : nullptr),
			*GetNameSafe(Clearance ? Clearance->HitComponent.Get() : nullptr),
			Clearance ? Clearance->TestedRadius : 0.0f,
			Clearance ? Clearance->TestedHalfHeight : 0.0f,
			Clearance ? Clearance->RaisedHeight : 0.0f,
			Clearance ? Clearance->FailedSegment : INDEX_NONE);
		return false;
	};
	if (!ControlledPawn || !Movement || !NavSystem)
	{
		return RejectHardFailure(TEXT("NavigationUnavailable"));
	}
	if (CachedRobot.IsValid()) CachedRobot->RecheckNavigationCompatibility(false);
	if (IsMoveGoalTemporarilyBlocked(Goal))
	{
		OutFailureReason = TEXT("BlockedPathCooldown");
		return false;
	}
	FNavLocation Projected;
	if (!NavSystem->ProjectPointToNavigation(Goal, Projected, FVector(100.0f, 100.0f, 250.0f), &Movement->GetNavAgentPropertiesRef()))
	{
		return RejectHardFailure(TEXT("NavProjectionFailed"));
	}
	UNavigationPath* Path = NavSystem->FindPathToLocationSynchronously(this, ControlledPawn->GetActorLocation(), Projected.Location, ControlledPawn);
	if (!Path || !Path->IsValid() || Path->IsPartial())
	{
		return RejectHardFailure(TEXT("InvalidOrPartialPath"));
	}
	LastNavigationState = TEXT("Full");

	FDMPathProbeSettings PassabilitySettings;
	PassabilitySettings.RadiusScale = FMath::Clamp(PathPassabilityRadiusScale, 0.5f, 1.0f);
	PassabilitySettings.HalfHeightScale = FMath::Clamp(PathPassabilityHalfHeightScale, 0.5f, 1.0f);
	PassabilitySettings.SampleSpacing = FMath::Max(1.0f, PathClearanceSampleSpacing);
	PassabilitySettings.bRaiseOnVerticalSegments = bRaisePassabilityProbeOnVerticalSegments;
	PassabilitySettings.VerticalSegmentThreshold = FMath::Max(0.0f, VerticalSegmentThreshold);
	PassabilitySettings.StepHeightProbeMultiplier = FMath::Max(0.0f, StepHeightProbeMultiplier);
	PassabilitySettings.MaxStepHeight = Movement->MaxStepHeight;
	PassabilitySettings.bPassabilityProbe = true;
	const FDMClearanceResult Passability = DMNavigationSafety::ValidateCapsulePath(
		GetWorld(), ControlledPawn, Path, PassabilitySettings);
	bLastDynamicObstacleDetected = Passability.bDynamicObstacleDetected;
	bLastRaisedStepProbeUsed = Passability.bRaisedStepProbeUsed;
	if (DMNavigationSafety::ShouldRejectMoveForClearance(Passability.bClear, true))
	{
		return RejectHardFailure(Passability.FailureReason, &Passability);
	}

	const FDMClearanceResult Destination = DMNavigationSafety::ValidateDestinationCapsule(
		GetWorld(), ControlledPawn, Projected.Location, DestinationCapsuleScale);
	if (!Destination.bClear)
	{
		return RejectHardFailure(Destination.FailureReason, &Destination);
	}

	if (!Passability.QualityReason.IsNone())
	{
		LastPathQuality = Passability.QualityReason;
	}
	const float QualityScale = FMath::Max(1.0f, bCombat ? CombatWallClearanceScale : RoamWallClearanceScale);
	if (QualityScale > 1.0f + KINDA_SMALL_NUMBER)
	{
		FDMPathProbeSettings QualitySettings = PassabilitySettings;
		QualitySettings.RadiusScale = QualityScale;
		QualitySettings.HalfHeightScale = 1.0f;
		QualitySettings.bPassabilityProbe = false;
		const FDMClearanceResult Quality = DMNavigationSafety::ValidateCapsulePath(
			GetWorld(), ControlledPawn, Path, QualitySettings);
		bLastDynamicObstacleDetected |= Quality.bDynamicObstacleDetected;
		bLastRaisedStepProbeUsed |= Quality.bRaisedStepProbeUsed;
		if (!Quality.bClear)
		{
			LastPathQuality = Passability.QualityReason.IsNone() ? FName(TEXT("NarrowPath")) : FName(TEXT("VeryNarrowPath"));
			UE_LOG(LogDM, Verbose, TEXT("DM Nav Quality: %s quality=%s (non-blocking) actor=%s component=%s location=%s radius=%.1f halfHeight=%.1f scale=%.2f segment=%d"),
				*GetName(), *LastPathQuality.ToString(), *GetNameSafe(Quality.HitActor.Get()), *GetNameSafe(Quality.HitComponent.Get()),
				*Quality.FailureLocation.ToCompactString(), Quality.TestedRadius, Quality.TestedHalfHeight,
				Quality.ClearanceScale, Quality.FailedSegment);
		}
	}
	if (bLastDynamicObstacleDetected)
	{
		UE_LOG(LogDM, Verbose, TEXT("DM Nav Quality: %s DynamicObstacleDetected (non-blocking)"), *GetName());
	}
	OutProjectedGoal = Projected.Location;
	return true;
}

bool ADeathMatchRobotAIController::IsMoveGoalTemporarilyBlocked(const FVector& Goal) const
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	for (const FBlockedMoveGoal& Entry : BlockedMoveGoals)
	{
		if (Entry.ExpiresAt > Now && FVector::DistSquared2D(Entry.Goal, Goal) <= FMath::Square(100.0f))
		{
			return true;
		}
	}
	return false;
}

void ADeathMatchRobotAIController::RecordMoveFailure(const FVector& Goal, FName Reason, bool bLogWarning)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	BlockedMoveGoals.RemoveAll([Now](const FBlockedMoveGoal& Entry) { return Entry.ExpiresAt <= Now; });
	BlockedMoveGoals.Add({Goal, Now + FMath::Max(0.1f, BlockedPathCooldown), Reason});
	if (BlockedMoveGoals.Num() > 8)
	{
		BlockedMoveGoals.RemoveAt(0, BlockedMoveGoals.Num() - 8);
	}
	LastClearanceFailure = Reason;
	if (bLogWarning)
	{
		UE_LOG(LogDM, Warning, TEXT("DM Nav Blocked: %s goal=%s reason=%s cooldown=%.2fs"),
			*GetName(), *Goal.ToCompactString(), *Reason.ToString(), BlockedPathCooldown);
	}
}

void ADeathMatchRobotAIController::SetMoveDebugOwner(FName OwnerName, const FVector& Goal)
{
	CurrentMoveOwner = OwnerName;
	CurrentMoveDestination = Goal;
}

void ADeathMatchRobotAIController::SetStuckDebug(FName Reason, int32 RepathCount)
{
	LastStuckReason = Reason;
	DebugRepathCount = RepathCount;
}

FString ADeathMatchRobotAIController::GetNavigationDebugSummary() const
{
#if !UE_BUILD_SHIPPING
	const ADeathMatchRobotCharacter* Robot = CachedRobot.Get();
	const UCapsuleComponent* Capsule = Robot ? Robot->GetCapsuleComponent() : nullptr;
	const UCharacterMovementComponent* Movement = Robot ? Robot->GetCharacterMovement() : nullptr;
	float CapsuleRadius = 0.0f, CapsuleHalfHeight = 0.0f;
	if (Capsule) Capsule->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	FString NavDataName = TEXT("None");
	float NavMeshRadius = 0.0f, NavMeshHeight = 0.0f;
	if (Movement)
	{
		if (const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			if (const ANavigationData* NavData = NavSystem->GetNavDataForProps(Movement->GetNavAgentPropertiesRef()))
			{
				NavDataName = NavData->GetName();
				NavMeshRadius = NavData->GetConfig().AgentRadius;
				NavMeshHeight = NavData->GetConfig().AgentHeight;
			}
		}
	}
	const FString Quality = LastPathQuality.IsNone() ? TEXT("Clear") : LastPathQuality.ToString();
	return FString::Printf(TEXT("Nav: %s | Pass: %s | Quality: %s%s | StepProbe: %d | Capsule %.0f/%.0f Agent %.0f/%.0f Mesh %s %.0f/%.0f | Owner %s Goal %s | Stuck %s Repath %d Avoid RVO"),
		LastNavigationState.IsNone() ? TEXT("None") : *LastNavigationState.ToString(),
		LastClearanceFailure.IsNone() ? TEXT("LooseOK") : *LastClearanceFailure.ToString(),
		*Quality, bLastDynamicObstacleDetected ? TEXT("+DynamicObstacle") : TEXT(""), bLastRaisedStepProbeUsed,
		CapsuleRadius, CapsuleHalfHeight * 2.0f,
		Movement ? Movement->GetNavAgentPropertiesRef().AgentRadius : 0.0f,
		Movement ? Movement->GetNavAgentPropertiesRef().AgentHeight : 0.0f,
		*NavDataName, NavMeshRadius, NavMeshHeight, *CurrentMoveOwner.ToString(), *CurrentMoveDestination.ToCompactString(),
		*LastStuckReason.ToString(), DebugRepathCount);
#else
	return FString();
#endif
}
