#include "DMCombatMoveDirector.h"

#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "DeathMatch/AI/Execution/DMActionIntentTypes.h"
#include "Navigation/PathFollowingComponent.h"

UDMCombatMoveDirector::UDMCombatMoveDirector()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ADeathMatchRobotAIController* UDMCombatMoveDirector::GetOwnerRobotAI() const
{
	return Cast<ADeathMatchRobotAIController>(GetOwner());
}

uint64 UDMCombatMoveDirector::SubmitMoveRequest(EDMMoveRequestOwner RequestOwner, int32 Priority,
	const FVector& Goal, float AcceptanceRadius, bool bCombatMovement, bool bProjectDestinationToNavigation)
{
	ADeathMatchRobotAIController* OwnerAI = GetOwnerRobotAI();
	if (!OwnerAI || !OwnerAI->GetPawn() || RequestOwner == EDMMoveRequestOwner::None) return 0;
	if (!CanReplaceRequest(ActiveRequestOwner, ActiveRequestPriority, RequestOwner, Priority)) return 0;

	FVector ProjectedGoal = Goal;
	FName FailureReason;
	OwnerAI->SetNavigationMovementMode(bCombatMovement);
	if (!OwnerAI->ValidateMoveGoal(Goal, bCombatMovement, ProjectedGoal, FailureReason)) return 0;

	const EPathFollowingRequestResult::Type Result = OwnerAI->MoveToLocation(
		ProjectedGoal, FMath::Max(1.f, AcceptanceRadius), true, true,
		bProjectDestinationToNavigation, true, nullptr, false);
	if (Result == EPathFollowingRequestResult::Failed) return 0;

	ActiveRequestId = NextRequestId++;
	ActiveRequestOwner = RequestOwner;
	ActiveRequestPriority = Priority;
	ActiveRequestGoal = ProjectedGoal;
	OwnerAI->SetMoveDebugOwner(TEXT("BehaviorTree"), ProjectedGoal);
	return ActiveRequestId;
}

bool UDMCombatMoveDirector::CanReplaceRequest(EDMMoveRequestOwner ActiveOwner, int32 ActivePriority,
	EDMMoveRequestOwner IncomingOwner, int32 IncomingPriority)
{
	if (IncomingOwner == EDMMoveRequestOwner::None) return false;
	return ActiveOwner == EDMMoveRequestOwner::None || ActiveOwner == IncomingOwner || IncomingPriority >= ActivePriority;
}

void UDMCombatMoveDirector::CancelMoveRequest(uint64 RequestId, bool bStopMovement)
{
	if (RequestId == 0 || ActiveRequestId != RequestId) return;
	CancelMoveRequest(ActiveRequestOwner, bStopMovement);
}

void UDMCombatMoveDirector::CancelMoveRequest(EDMMoveRequestOwner RequestOwner, bool bStopMovement)
{
	if (ActiveRequestOwner != RequestOwner) return;
	LastCompletedOwner = ActiveRequestOwner;
	LastCompletedRequestId = ActiveRequestId;
	ActiveRequestId = 0;
	ActiveRequestOwner = EDMMoveRequestOwner::None;
	ActiveRequestPriority = TNumericLimits<int32>::Lowest();
	ActiveRequestGoal = FVector::ZeroVector;
	if (bStopMovement)
		if (ADeathMatchRobotAIController* OwnerAI = GetOwnerRobotAI()) OwnerAI->StopMovement();
}

void UDMCombatMoveDirector::CancelAllMoveRequests(bool bStopMovement)
{
	LastCompletedOwner = ActiveRequestOwner;
	LastCompletedRequestId = ActiveRequestId;
	ActiveRequestId = 0;
	ActiveRequestOwner = EDMMoveRequestOwner::None;
	ActiveRequestPriority = TNumericLimits<int32>::Lowest();
	ActiveRequestGoal = FVector::ZeroVector;
	if (bStopMovement)
		if (ADeathMatchRobotAIController* OwnerAI = GetOwnerRobotAI()) OwnerAI->StopMovement();
}

EDMMoveRequestOwner UDMCombatMoveDirector::GetAllowedOwnerForState(EDMBehaviorState State)
{
	switch (State)
	{
	case EDMBehaviorState::Roam: return EDMMoveRequestOwner::StrategicRoam;
	case EDMBehaviorState::LostTarget: return EDMMoveRequestOwner::Investigate;
	case EDMBehaviorState::Combat: return EDMMoveRequestOwner::Combat;
	case EDMBehaviorState::Retreat: return EDMMoveRequestOwner::Retreat;
	case EDMBehaviorState::Hide: return EDMMoveRequestOwner::EmergencyHide;
	default: return EDMMoveRequestOwner::None;
	}
}

bool UDMCombatMoveDirector::IsOwnerAllowedForState(EDMBehaviorState State, EDMMoveRequestOwner Owner)
{
	return Owner != EDMMoveRequestOwner::None && Owner == GetAllowedOwnerForState(State);
}

void UDMCombatMoveDirector::TransitionToBehaviorState(EDMBehaviorState NewState)
{
	if (ActiveRequestOwner != EDMMoveRequestOwner::None
		&& !IsOwnerAllowedForState(NewState, ActiveRequestOwner))
	{
		CancelMoveRequest(ActiveRequestId, true);
	}
}

void UDMCombatMoveDirector::NotifyPathFollowingCompleted(bool bSuccess)
{
	LastCompletedOwner = ActiveRequestOwner;
	LastCompletedRequestId = ActiveRequestId;
	bLastMoveSuccessful = bSuccess;
	++CompletionSerial;
	ActiveRequestId = 0;
	ActiveRequestOwner = EDMMoveRequestOwner::None;
	ActiveRequestPriority = TNumericLimits<int32>::Lowest();
	ActiveRequestGoal = FVector::ZeroVector;
}
