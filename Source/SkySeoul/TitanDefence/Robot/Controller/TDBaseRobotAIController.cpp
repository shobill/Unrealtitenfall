#include "TitanDefence/Robot/Controller/TDBaseRobotAIController.h"

#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "TitanDefence/Robot/Character/TDBaseRobotCharacter.h"
#include "TitanDefence/Robot/Core/TDRobotLog.h"

namespace
{
	const TCHAR* NavigationQueryResultToString(ENavigationQueryResult::Type Result)
	{
		switch (Result)
		{
		case ENavigationQueryResult::Success: return TEXT("Success");
		case ENavigationQueryResult::Fail: return TEXT("Fail");
		case ENavigationQueryResult::Error: return TEXT("Error");
		case ENavigationQueryResult::Invalid:
		default: return TEXT("Invalid");
		}
	}
}

ATDBaseRobotAIController::ATDBaseRobotAIController()
{
	bWantsPlayerState = false;
}

void ATDBaseRobotAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ResetQueuedMove();
	GetWorldTimerManager().ClearTimer(DeferredInitialMoveTimerHandle);
	bInitialMoveDeferred = false;

	PossessedRobot = Cast<ATDBaseRobotCharacter>(InPawn);
	if (!PossessedRobot.IsValid())
	{
		UE_LOG(LogTDRobot, Warning,
			TEXT("Controller %s possessed unsupported pawn %s."),
			*GetNameSafe(this), *GetNameSafe(InPawn));
		return;
	}

	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Controller '%s' possessed robot '%s': autoMove=%s initialTarget='%s'."),
		*GetNameSafe(this), *GetNameSafe(InPawn),
		PossessedRobot->ShouldAutoMoveOnBeginPlay() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(PossessedRobot->GetInitialMoveTargetActor()));

	AActor* InitialTarget = PossessedRobot->GetInitialMoveTargetActor();
	if (PossessedRobot->ShouldAutoMoveOnBeginPlay() && IsValid(InitialTarget))
	{
		bInitialMoveDeferred = true;
		UE_LOG(LogTDRobotMovement, Log,
			TEXT("Initial move deferred until BeginPlay: %s"),
			*BuildNavigationLogContext(InitialTarget, 0.0f, 0));
		if (PossessedRobot->HasActorBegunPlay())
		{
			NotifyRobotBeginPlay(PossessedRobot.Get());
		}
	}
	else if (PossessedRobot->ShouldAutoMoveOnBeginPlay())
	{
		UE_LOG(LogTDRobotMovement, Warning,
			TEXT("Controller '%s' cannot defer the automatic move for robot '%s' because initial target '%s' is invalid."),
			*GetNameSafe(this), *GetNameSafe(InPawn), *GetNameSafe(InitialTarget));
	}
	else
	{
		UE_LOG(LogTDRobotMovement, Log,
			TEXT("Robot '%s' automatic movement is disabled."), *GetNameSafe(InPawn));
	}
}

void ATDBaseRobotAIController::OnUnPossess()
{
	ATDBaseRobotCharacter* PreviousRobot = PossessedRobot.Get();
	const bool bHadActiveMove = ActiveMoveRequestId.IsValid();
	CancelQueuedMove(TEXT("Controller unpossessed robot"), false);
	GetWorldTimerManager().ClearTimer(DeferredInitialMoveTimerHandle);
	bInitialMoveDeferred = false;
	InvalidateActiveMove();
	StopMovement();

	if (PreviousRobot && bHadActiveMove)
	{
		PreviousRobot->TransitionMovementState(
			ETDRobotMovementState::Failed,
			ETDRobotMovementEndReason::MissingController);
	}

	PossessedRobot.Reset();
	Super::OnUnPossess();
}

void ATDBaseRobotAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelQueuedMove(TEXT("Controller EndPlay"), false);
	GetWorldTimerManager().ClearTimer(DeferredInitialMoveTimerHandle);
	bInitialMoveDeferred = false;
	InvalidateActiveMove();
	StopMovement();
	PossessedRobot.Reset();
	Super::EndPlay(EndPlayReason);
}

void ATDBaseRobotAIController::NotifyRobotBeginPlay(ATDBaseRobotCharacter* Robot)
{
	if (!bInitialMoveDeferred || PossessedRobot.Get() != Robot)
	{
		return;
	}
	if (DeferredInitialMoveTimerHandle.IsValid())
	{
		return;
	}

	DeferredInitialMoveTimerHandle = GetWorldTimerManager().SetTimerForNextTick(
		this, &ATDBaseRobotAIController::StartDeferredInitialMove);
}

void ATDBaseRobotAIController::StartDeferredInitialMove()
{
	DeferredInitialMoveTimerHandle.Invalidate();
	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	if (!bInitialMoveDeferred || !Robot)
	{
		return;
	}

	bInitialMoveDeferred = false;
	AActor* InitialTarget = Robot->GetInitialMoveTargetActor();
	if (!IsValid(InitialTarget))
	{
		UE_LOG(LogTDRobotMovement, Warning,
			TEXT("Queued move cancelled: reason='Initial target became invalid before the first post-BeginPlay tick' %s"),
			*BuildNavigationLogContext(InitialTarget, 0.0f, 0));
		Robot->SetCurrentMoveTargetActor(nullptr);
		Robot->TransitionMovementState(
			ETDRobotMovementState::Failed,
			ETDRobotMovementEndReason::InvalidTarget);
		return;
	}

	Robot->StartMovingToInitialTarget();
}

bool ATDBaseRobotAIController::RequestMoveToActor(AActor* TargetActor, float AcceptanceRadius)
{
	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	if (!Robot)
	{
		UE_LOG(LogTDRobot, Warning,
			TEXT("Controller %s ignored a move request because it has no Titan Defence robot."),
			*GetNameSafe(this));
		return false;
	}

	CancelQueuedMove(TEXT("Replaced by a new move request"), false);
	InvalidateActiveMove();
	StopMovement();
	Robot->SetCurrentMoveTargetActor(TargetActor);

	if (TargetActor == nullptr)
	{
		Robot->TransitionMovementState(ETDRobotMovementState::Idle);
		return true;
	}

	if (!IsValid(TargetActor))
	{
		UE_LOG(LogTDRobot, Warning,
			TEXT("Robot %s cannot move because its target is invalid."),
			*GetNameSafe(Robot));
		Robot->SetCurrentMoveTargetActor(nullptr);
		Robot->TransitionMovementState(
			ETDRobotMovementState::Failed,
			ETDRobotMovementEndReason::InvalidTarget);
		return false;
	}

	return StartMoveNow(TargetActor, AcceptanceRadius);
}

bool ATDBaseRobotAIController::StartMoveNow(
	AActor* TargetActor,
	float AcceptanceRadius)
{
	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	if (!Robot || !IsValid(TargetActor))
	{
		return false;
	}

	if (!GetPathFollowingComponent())
	{
		UE_LOG(LogTDRobotMovement, Error,
			TEXT("Move request rejected because PathFollowingComponent is missing: %s"),
			*BuildNavigationLogContext(TargetActor, 0.0f, 0));
		Robot->TransitionMovementState(
			ETDRobotMovementState::Failed,
			ETDRobotMovementEndReason::RequestRejected);
		return false;
	}

	FAIMoveRequest MoveRequest(TargetActor);
	MoveRequest.SetAcceptanceRadius(FMath::Max(0.0f, AcceptanceRadius));
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(false);
	MoveRequest.SetProjectGoalLocation(true);
	MoveRequest.SetReachTestIncludesAgentRadius(true);
	MoveRequest.SetReachTestIncludesGoalRadius(true);

	const UCharacterMovementComponent* MovementComponent = Robot->GetCharacterMovement();
	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	const FNavAgentProperties& AgentProperties = GetNavAgentPropertiesRef();
	const FVector AgentLocation = GetNavAgentLocation();
	const ANavigationData* NavData = NavigationSystem
		? NavigationSystem->GetNavDataForProps(AgentProperties, AgentLocation)
		: nullptr;
	if (!NavData)
	{
		return QueueMoveUntilNavigationIsReady(TargetActor, AcceptanceRadius);
	}

	Robot->TransitionMovementState(ETDRobotMovementState::Requesting);
	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Move request: robot='%s' actorLocation=%s agentLocation=%s target='%s' goal=%s distance=%.1f acceptance=%.1f navSystem=%s navData='%s' pathFollowing=%s movementMode=%d maxWalkSpeed=%.1f velocity=%s agentRadius=%.2f agentHeight=%.2f agentStepHeight=%.2f canCrouch=%s canJump=%s canWalk=%s canSwim=%s canFly=%s."),
		*GetNameSafe(Robot), *Robot->GetActorLocation().ToCompactString(),
		*AgentLocation.ToCompactString(),
		*GetNameSafe(TargetActor), *TargetActor->GetActorLocation().ToCompactString(),
		FVector::Dist(Robot->GetActorLocation(), TargetActor->GetActorLocation()),
		FMath::Max(0.0f, AcceptanceRadius),
		NavigationSystem ? TEXT("valid") : TEXT("missing"),
		*GetNameSafe(NavData),
		GetPathFollowingComponent() ? TEXT("valid") : TEXT("missing"),
		MovementComponent ? static_cast<int32>(MovementComponent->MovementMode) : -1,
		MovementComponent ? MovementComponent->MaxWalkSpeed : 0.0f,
		*Robot->GetVelocity().ToCompactString(), AgentProperties.AgentRadius, AgentProperties.AgentHeight,
		AgentProperties.AgentStepHeight,
		AgentProperties.bCanCrouch ? TEXT("true") : TEXT("false"),
		AgentProperties.bCanJump ? TEXT("true") : TEXT("false"),
		AgentProperties.bCanWalk ? TEXT("true") : TEXT("false"),
		AgentProperties.bCanSwim ? TEXT("true") : TEXT("false"),
		AgentProperties.bCanFly ? TEXT("true") : TEXT("false"));

	if (NavigationSystem)
	{
		const FVector QueryExtent = NavData->GetDefaultQueryExtent();
		FNavLocation ProjectedStart;
		FNavLocation ProjectedGoal;
		const bool bStartProjected = NavigationSystem->ProjectPointToNavigation(
			AgentLocation, ProjectedStart, QueryExtent, NavData);
		const bool bGoalProjected = NavigationSystem->ProjectPointToNavigation(
			TargetActor->GetActorLocation(), ProjectedGoal, QueryExtent, NavData);
		UE_LOG(LogTDRobotMovement, Warning,
			TEXT("Navigation projection: robot='%s' navData='%s' queryExtent=%s startProjected=%s startNav=%s startOffset=%.2f goalProjected=%s goalNav=%s goalOffset=%.2f."),
			*GetNameSafe(Robot), *GetNameSafe(NavData), *QueryExtent.ToCompactString(),
			bStartProjected ? TEXT("true") : TEXT("false"),
			bStartProjected ? *ProjectedStart.Location.ToCompactString() : TEXT("invalid"),
			bStartProjected ? FVector::Dist(AgentLocation, ProjectedStart.Location) : -1.0f,
			bGoalProjected ? TEXT("true") : TEXT("false"),
			bGoalProjected ? *ProjectedGoal.Location.ToCompactString() : TEXT("invalid"),
			bGoalProjected ? FVector::Dist(TargetActor->GetActorLocation(), ProjectedGoal.Location) : -1.0f);
	}
	FPathFindingQuery DiagnosticQuery;
	const bool bQueryBuilt = BuildPathfindingQuery(MoveRequest, DiagnosticQuery);
	if (bQueryBuilt && NavigationSystem)
	{
		const FPathFindingResult DiagnosticResult = NavigationSystem->FindPathSync(
			AgentProperties, DiagnosticQuery, EPathFindingMode::Regular);
		const int32 PathPointCount = DiagnosticResult.Path.IsValid()
			? DiagnosticResult.Path->GetPathPoints().Num()
			: 0;
		const float PathLength = DiagnosticResult.Path.IsValid()
			? DiagnosticResult.Path->GetLength()
			: 0.0f;
		UE_LOG(LogTDRobotMovement, Warning,
			TEXT("Synchronous path diagnostic: robot='%s' queryBuilt=true result=%s partial=%s pathValid=%s points=%d length=%.2f queryStart=%s queryEnd=%s allowPartial=%s requireNavigableEnd=%s navData='%s'."),
			*GetNameSafe(Robot), NavigationQueryResultToString(DiagnosticResult.Result),
			DiagnosticResult.IsPartial() ? TEXT("true") : TEXT("false"),
			DiagnosticResult.Path.IsValid() ? TEXT("true") : TEXT("false"),
			PathPointCount, PathLength, *DiagnosticQuery.StartLocation.ToCompactString(),
			*DiagnosticQuery.EndLocation.ToCompactString(),
			DiagnosticQuery.bAllowPartialPaths ? TEXT("true") : TEXT("false"),
			DiagnosticQuery.bRequireNavigableEndLocation ? TEXT("true") : TEXT("false"),
			*GetNameSafe(DiagnosticQuery.NavData.Get()));
	}
	else
	{
		UE_LOG(LogTDRobotMovement, Error,
			TEXT("Pathfinding query build failed: robot='%s' queryBuilt=%s navSystem=%s navData='%s' pathFollowing=%s."),
			*GetNameSafe(Robot), bQueryBuilt ? TEXT("true") : TEXT("false"),
			NavigationSystem ? TEXT("valid") : TEXT("missing"), *GetNameSafe(NavData),
			GetPathFollowingComponent() ? TEXT("valid") : TEXT("missing"));
	}

	const FPathFollowingRequestResult RequestResult = MoveTo(MoveRequest);
	switch (RequestResult.Code)
	{
	case EPathFollowingRequestResult::RequestSuccessful:
		ActiveMoveRequestId = RequestResult.MoveId;
		ActiveTargetActor = TargetActor;
		TargetActor->OnDestroyed.AddUniqueDynamic(this, &ATDBaseRobotAIController::HandleActiveTargetDestroyed);
		Robot->TransitionMovementState(ETDRobotMovementState::Moving);
		UE_LOG(LogTDRobotMovement, Log,
			TEXT("MoveTo accepted: robot='%s' target='%s' request='%s'."),
			*GetNameSafe(Robot), *GetNameSafe(TargetActor), *ActiveMoveRequestId.ToString());
		return true;

	case EPathFollowingRequestResult::AlreadyAtGoal:
		Robot->TransitionMovementState(
			ETDRobotMovementState::Arrived,
			ETDRobotMovementEndReason::Arrived);
		UE_LOG(LogTDRobotMovement, Log,
			TEXT("Robot %s is already within the acceptance radius of %s."),
			*GetNameSafe(Robot), *GetNameSafe(TargetActor));
		return true;

	case EPathFollowingRequestResult::Failed:
	default:
		Robot->TransitionMovementState(
			ETDRobotMovementState::Failed,
			GetPathFollowingComponent()
				? ETDRobotMovementEndReason::NoPath
				: ETDRobotMovementEndReason::RequestRejected);
		UE_LOG(LogTDRobotMovement, Warning,
			TEXT("MoveTo failed: robot='%s' target='%s' navSystem=%s pathFollowing=%s movementMode=%d. Check NavMesh coverage, collision, target projection, and movement mode."),
			*GetNameSafe(Robot), *GetNameSafe(TargetActor),
			NavigationSystem ? TEXT("valid") : TEXT("missing"),
			GetPathFollowingComponent() ? TEXT("valid") : TEXT("missing"),
			MovementComponent ? static_cast<int32>(MovementComponent->MovementMode) : -1);
		return false;
	}
}

bool ATDBaseRobotAIController::QueueMoveUntilNavigationIsReady(
	AActor* TargetActor,
	float AcceptanceRadius)
{
	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	if (!Robot || !IsValid(TargetActor))
	{
		return false;
	}

	if (QueuedTargetActor.Get() == TargetActor)
	{
		return true;
	}

	ResetQueuedMove();
	QueuedTargetActor = TargetActor;
	QueuedAcceptanceRadius = FMath::Max(0.0f, AcceptanceRadius);
	NavigationWaitStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	NavigationRetryCount = 0;
	TargetActor->OnDestroyed.AddUniqueDynamic(
		this, &ATDBaseRobotAIController::HandleQueuedTargetDestroyed);

	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	BindNavigationGenerationEvent(NavigationSystem);
	const float RetryInterval = FMath::Max(0.01f, NavigationRetryInterval);
	GetWorldTimerManager().SetTimer(
		NavigationRetryTimerHandle,
		this,
		&ATDBaseRobotAIController::RetryQueuedMove,
		RetryInterval,
		true,
		RetryInterval);

	Robot->TransitionMovementState(
		ETDRobotMovementState::WaitingForNavigation,
		ETDRobotMovementEndReason::NavigationDataNotReady);
	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Navigation data not ready; move request queued: %s"),
		*BuildNavigationLogContext(TargetActor, 0.0f, 0));
	return true;
}

void ATDBaseRobotAIController::RetryQueuedMove()
{
	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	AActor* TargetActor = QueuedTargetActor.Get();
	if (!Robot || !IsValid(TargetActor))
	{
		CancelQueuedMove(TEXT("Robot or target became invalid"), true);
		return;
	}

	++NavigationRetryCount;
	StartQueuedMoveIfReady(TEXT("retry timer"));
	if (QueuedTargetActor.IsValid())
	{
		UE_LOG(LogTDRobotMovement, Verbose,
			TEXT("Waiting for navigation data: %s"),
			*BuildNavigationLogContext(
				TargetActor, GetNavigationWaitElapsed(), NavigationRetryCount));
	}
}

void ATDBaseRobotAIController::StartQueuedMoveIfReady(const TCHAR* AvailabilitySource)
{
	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	AActor* TargetActor = QueuedTargetActor.Get();
	if (!Robot || !IsValid(TargetActor))
	{
		CancelQueuedMove(TEXT("Robot or target became invalid"), true);
		return;
	}

	const float WaitElapsed = GetNavigationWaitElapsed();
	if (WaitElapsed >= FMath::Max(0.1f, NavigationMaximumWaitTime))
	{
		UE_LOG(LogTDRobotMovement, Error,
			TEXT("Navigation wait timed out: %s"),
			*BuildNavigationLogContext(TargetActor, WaitElapsed, NavigationRetryCount));
		ResetQueuedMove();
		Robot->SetCurrentMoveTargetActor(nullptr);
		Robot->TransitionMovementState(
			ETDRobotMovementState::Failed,
			ETDRobotMovementEndReason::NavigationWaitTimedOut);
		return;
	}

	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	BindNavigationGenerationEvent(NavigationSystem);
	const FNavAgentProperties& AgentProperties = GetNavAgentPropertiesRef();
	const ANavigationData* NavData = NavigationSystem
		? NavigationSystem->GetNavDataForProps(AgentProperties, GetNavAgentLocation())
		: nullptr;
	if (!NavData)
	{
		return;
	}

	const float AcceptanceRadius = QueuedAcceptanceRadius;
	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Navigation data became available: source='%s' %s"),
		AvailabilitySource,
		*BuildNavigationLogContext(TargetActor, WaitElapsed, NavigationRetryCount));
	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Queued move request started: %s"),
		*BuildNavigationLogContext(TargetActor, WaitElapsed, NavigationRetryCount));
	ResetQueuedMove();
	StartMoveNow(TargetActor, AcceptanceRadius);
}

void ATDBaseRobotAIController::CancelQueuedMove(const TCHAR* Reason, bool bUpdateRobotState)
{
	const bool bHadQueuedMove = QueuedTargetActor.IsValid()
		|| NavigationRetryTimerHandle.IsValid()
		|| bInitialMoveDeferred
		|| DeferredInitialMoveTimerHandle.IsValid();
	if (!bHadQueuedMove)
	{
		return;
	}

	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	AActor* TargetActor = QueuedTargetActor.IsValid()
		? QueuedTargetActor.Get()
		: (Robot ? Robot->GetInitialMoveTargetActor() : nullptr);
	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Queued move cancelled: reason='%s' %s"),
		Reason,
		*BuildNavigationLogContext(
			TargetActor, GetNavigationWaitElapsed(), NavigationRetryCount));

	ResetQueuedMove();
	GetWorldTimerManager().ClearTimer(DeferredInitialMoveTimerHandle);
	bInitialMoveDeferred = false;
	if (bUpdateRobotState && Robot)
	{
		Robot->SetCurrentMoveTargetActor(nullptr);
		Robot->TransitionMovementState(
			ETDRobotMovementState::Idle,
			ETDRobotMovementEndReason::Cancelled);
	}
}

void ATDBaseRobotAIController::ResetQueuedMove()
{
	if (AActor* TargetActor = QueuedTargetActor.Get())
	{
		TargetActor->OnDestroyed.RemoveDynamic(
			this, &ATDBaseRobotAIController::HandleQueuedTargetDestroyed);
	}
	if (UNavigationSystemV1* NavigationSystem = BoundNavigationSystem.Get())
	{
		NavigationSystem->OnNavigationGenerationFinishedDelegate.RemoveDynamic(
			this, &ATDBaseRobotAIController::HandleNavigationGenerationFinished);
	}
	GetWorldTimerManager().ClearTimer(NavigationRetryTimerHandle);
	QueuedTargetActor.Reset();
	BoundNavigationSystem.Reset();
	QueuedAcceptanceRadius = 0.0f;
	NavigationWaitStartTime = -1.0;
	NavigationRetryCount = 0;
}

void ATDBaseRobotAIController::BindNavigationGenerationEvent(
	UNavigationSystemV1* NavigationSystem)
{
	if (!NavigationSystem || BoundNavigationSystem.Get() == NavigationSystem)
	{
		return;
	}

	if (UNavigationSystemV1* PreviousNavigationSystem = BoundNavigationSystem.Get())
	{
		PreviousNavigationSystem->OnNavigationGenerationFinishedDelegate.RemoveDynamic(
			this, &ATDBaseRobotAIController::HandleNavigationGenerationFinished);
	}
	BoundNavigationSystem = NavigationSystem;
	NavigationSystem->OnNavigationGenerationFinishedDelegate.AddUniqueDynamic(
		this, &ATDBaseRobotAIController::HandleNavigationGenerationFinished);
}

float ATDBaseRobotAIController::GetNavigationWaitElapsed() const
{
	return NavigationWaitStartTime >= 0.0 && GetWorld()
		? static_cast<float>(GetWorld()->GetTimeSeconds() - NavigationWaitStartTime)
		: 0.0f;
}

FString ATDBaseRobotAIController::BuildNavigationLogContext(
	AActor* TargetActor,
	float WaitTime,
	int32 RetryCount) const
{
	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	const FNavAgentProperties& AgentProperties = GetNavAgentPropertiesRef();
	const ANavigationData* NavData = NavigationSystem
		? NavigationSystem->GetNavDataForProps(AgentProperties, GetNavAgentLocation())
		: nullptr;
	return FString::Printf(
		TEXT("robot='%s' controller='%s' target='%s' wait=%.2fs retries=%d navSystem='%s' navData='%s' agentRadius=%.2f agentHeight=%.2f"),
		*GetNameSafe(PossessedRobot.Get()), *GetNameSafe(this), *GetNameSafe(TargetActor),
		WaitTime, RetryCount, *GetNameSafe(NavigationSystem), *GetNameSafe(NavData),
		AgentProperties.AgentRadius, AgentProperties.AgentHeight);
}

void ATDBaseRobotAIController::CancelCurrentMove()
{
	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	CancelQueuedMove(TEXT("Movement cancellation requested"), false);
	GetWorldTimerManager().ClearTimer(DeferredInitialMoveTimerHandle);
	bInitialMoveDeferred = false;
	InvalidateActiveMove();
	StopMovement();

	if (Robot)
	{
		Robot->SetCurrentMoveTargetActor(nullptr);
		Robot->TransitionMovementState(
			ETDRobotMovementState::Idle,
			ETDRobotMovementEndReason::Cancelled);
		UE_LOG(LogTDRobot, Verbose, TEXT("Robot %s movement was cancelled."), *GetNameSafe(Robot));
	}
}

bool ATDBaseRobotAIController::ClearMoveTarget()
{
	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	CancelQueuedMove(TEXT("Move target cleared"), false);
	GetWorldTimerManager().ClearTimer(DeferredInitialMoveTimerHandle);
	bInitialMoveDeferred = false;
	InvalidateActiveMove();
	StopMovement();

	if (!Robot)
	{
		return false;
	}

	Robot->SetCurrentMoveTargetActor(nullptr);
	Robot->TransitionMovementState(ETDRobotMovementState::Idle);
	return true;
}

void ATDBaseRobotAIController::InterruptCurrentMove(
	ETDRobotMovementState InterruptionState,
	ETDRobotMovementEndReason EndReason)
{
	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	CancelQueuedMove(TEXT("Movement interrupted"), false);
	GetWorldTimerManager().ClearTimer(DeferredInitialMoveTimerHandle);
	bInitialMoveDeferred = false;
	InvalidateActiveMove();
	StopMovement();

	if (Robot)
	{
		Robot->TransitionMovementState(InterruptionState, EndReason);
	}
}

void ATDBaseRobotAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	const bool bIsActiveRequest = ActiveMoveRequestId.IsValid()
		&& ActiveMoveRequestId.IsEquivalent(RequestID);
	if (bIsActiveRequest)
	{
		ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
		UnbindActiveTarget();
		ActiveMoveRequestId = FAIRequestID::InvalidRequest;

		if (Robot)
		{
			const bool bArrived = Result.IsSuccess();
			Robot->TransitionMovementState(
				bArrived ? ETDRobotMovementState::Arrived : ETDRobotMovementState::Failed,
				bArrived ? ETDRobotMovementEndReason::Arrived : GetFailureReason(Result));

			UE_LOG(LogTDRobotMovement, Log,
				TEXT("Robot '%s' move request '%s' completed: result=%s code=%d location=%s target='%s'."),
				*GetNameSafe(Robot), *RequestID.ToString(),
				bArrived ? TEXT("Arrived") : TEXT("Failed"),
				static_cast<int32>(Result.Code), *Robot->GetActorLocation().ToCompactString(),
				*GetNameSafe(Robot->GetCurrentMoveTargetActor()));
		}
	}
	else
	{
		UE_LOG(LogTDRobot, VeryVerbose,
			TEXT("Controller %s ignored stale move result %s."),
			*GetNameSafe(this), *RequestID.ToString());
	}

	Super::OnMoveCompleted(RequestID, Result);
}

ETDRobotMovementEndReason ATDBaseRobotAIController::GetFailureReason(
	const FPathFollowingResult& Result) const
{
	switch (Result.Code)
	{
	case EPathFollowingResult::Blocked:
	case EPathFollowingResult::OffPath:
		return ETDRobotMovementEndReason::NoPath;

	case EPathFollowingResult::Aborted:
		return ETDRobotMovementEndReason::Aborted;

	case EPathFollowingResult::Invalid:
	default:
		return ETDRobotMovementEndReason::RequestRejected;
	}
}

void ATDBaseRobotAIController::InvalidateActiveMove()
{
	ActiveMoveRequestId = FAIRequestID::InvalidRequest;
	UnbindActiveTarget();
}

void ATDBaseRobotAIController::UnbindActiveTarget()
{
	if (AActor* TargetActor = ActiveTargetActor.Get())
	{
		TargetActor->OnDestroyed.RemoveDynamic(this, &ATDBaseRobotAIController::HandleActiveTargetDestroyed);
	}
	ActiveTargetActor.Reset();
}

void ATDBaseRobotAIController::HandleActiveTargetDestroyed(AActor* DestroyedActor)
{
	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	ActiveMoveRequestId = FAIRequestID::InvalidRequest;
	ActiveTargetActor.Reset();
	StopMovement();

	if (Robot)
	{
		Robot->SetCurrentMoveTargetActor(nullptr);
		Robot->TransitionMovementState(
			ETDRobotMovementState::Failed,
			ETDRobotMovementEndReason::InvalidTarget);
		UE_LOG(LogTDRobot, Warning,
			TEXT("Robot %s movement failed because target %s was destroyed."),
			*GetNameSafe(Robot), *GetNameSafe(DestroyedActor));
	}
}

void ATDBaseRobotAIController::HandleQueuedTargetDestroyed(AActor* DestroyedActor)
{
	ATDBaseRobotCharacter* Robot = PossessedRobot.Get();
	UE_LOG(LogTDRobotMovement, Log,
		TEXT("Queued move cancelled: reason='Target destroyed' %s"),
		*BuildNavigationLogContext(
			DestroyedActor, GetNavigationWaitElapsed(), NavigationRetryCount));
	ResetQueuedMove();
	if (Robot)
	{
		Robot->SetCurrentMoveTargetActor(nullptr);
		Robot->TransitionMovementState(
			ETDRobotMovementState::Failed,
			ETDRobotMovementEndReason::InvalidTarget);
	}
}

void ATDBaseRobotAIController::HandleNavigationGenerationFinished(
	ANavigationData* GeneratedNavData)
{
	if (!QueuedTargetActor.IsValid())
	{
		return;
	}

	UE_LOG(LogTDRobotMovement, VeryVerbose,
		TEXT("Navigation generation event received from '%s'."),
		*GetNameSafe(GeneratedNavData));
	StartQueuedMoveIfReady(TEXT("generation event"));
}

