#include "BTTask_DM_MoveToGoal.h"

#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/AI/Move/DMCombatMoveDirector.h"
#include "DeathMatch/Navigation/DMTacticalAreaSubsystem.h"
#include "DeathMatch/Navigation/DMTacticalAreaVolume.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "AIController.h"

namespace
{
struct FDMMoveTaskMemory
{
	uint64 RequestId = 0;
	uint32 StartedCompletionSerial = 0;
};

void CleanupRoamState(UBehaviorTreeComponent& OwnerComp, const FBlackboardKeySelector& MoveGoalKey)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	if (UDMTacticalAreaSubsystem* Areas = OwnerComp.GetWorld()
		? OwnerComp.GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr)
	{
		Areas->ReleaseIncomingAI(AI ? AI->GetPawn() : nullptr);
	}
	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		Blackboard->ClearValue(DMBlackboardKeys::SelectedTacticalArea);
		Blackboard->ClearValue(MoveGoalKey.SelectedKeyName);
	}
}
}

UBTTask_DM_MoveToGoal::UBTTask_DM_MoveToGoal()
{
	NodeName = TEXT("DM Move To Goal");
	bNotifyTick = true;
	MoveGoalKey.SelectedKeyName = DMBlackboardKeys::MoveGoal;
	MoveGoalKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_MoveToGoal, MoveGoalKey));
}

uint16 UBTTask_DM_MoveToGoal::GetInstanceMemorySize() const { return sizeof(FDMMoveTaskMemory); }

EBTNodeResult::Type UBTTask_DM_MoveToGoal::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FDMMoveTaskMemory* Memory = reinterpret_cast<FDMMoveTaskMemory*>(NodeMemory);
	Memory->RequestId = 0;
	AAIController* AI = OwnerComp.GetAIOwner();
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	UDMCombatMoveDirector* Director = AI ? AI->FindComponentByClass<UDMCombatMoveDirector>() : nullptr;
	if (!Director || !BB || !BB->IsVectorValueSet(MoveGoalKey.SelectedKeyName)) return EBTNodeResult::Failed;
	Memory->StartedCompletionSerial = Director->GetCompletionSerial();
	Memory->RequestId = Director->SubmitMoveRequest(RequestOwner, Priority,
		BB->GetValueAsVector(MoveGoalKey.SelectedKeyName), AcceptanceRadius, bCombatMovement, true);
	return Memory->RequestId != 0 ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

void UBTTask_DM_MoveToGoal::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float)
{
	FDMMoveTaskMemory* Memory = reinterpret_cast<FDMMoveTaskMemory*>(NodeMemory);
	AAIController* AI = OwnerComp.GetAIOwner();
	UDMCombatMoveDirector* Director = AI ? AI->FindComponentByClass<UDMCombatMoveDirector>() : nullptr;
	if (!Director)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	if (Director->GetCompletionSerial() != Memory->StartedCompletionSerial
		&& Director->GetLastCompletedRequestId() == Memory->RequestId)
	{
		const bool bOwnedCompletion = Director->GetLastCompletedOwner() == RequestOwner;
		if (bOwnedCompletion && RequestOwner == EDMMoveRequestOwner::StrategicRoam)
		{
			APawn* Pawn = AI ? AI->GetPawn() : nullptr;
			UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
			ADMTacticalAreaVolume* Area = BB ? Cast<ADMTacticalAreaVolume>(BB->GetValueAsObject(DMBlackboardKeys::SelectedTacticalArea)) : nullptr;
			if (Pawn)
				if (UDMTacticalAreaSubsystem* Areas = OwnerComp.GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>())
				{
					if (Director->WasLastMoveSuccessful()) Areas->ReportRoamAreaReached(Pawn, Area);
					else Areas->RecordRoamAreaFailure(Pawn, Area, OwnerComp.GetWorld()->GetTimeSeconds(), RoamFailureCooldown);
					Areas->ReleaseIncomingAI(Pawn);
				}
			if (BB) BB->ClearValue(DMBlackboardKeys::SelectedTacticalArea);
		}
		Memory->RequestId = 0;
		FinishLatentTask(OwnerComp, bOwnedCompletion && Director->WasLastMoveSuccessful()
			? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
	}
	else if (!Director->IsMoveRequestActive(Memory->RequestId))
	{
		Memory->RequestId = 0;
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
}

EBTNodeResult::Type UBTTask_DM_MoveToGoal::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FDMMoveTaskMemory* Memory = reinterpret_cast<FDMMoveTaskMemory*>(NodeMemory);
	if (AAIController* AI = OwnerComp.GetAIOwner())
	{
		if (UDMCombatMoveDirector* Director = AI->FindComponentByClass<UDMCombatMoveDirector>())
			Director->CancelMoveRequest(Memory->RequestId, true);
		if (RequestOwner == EDMMoveRequestOwner::StrategicRoam)
			CleanupRoamState(OwnerComp, MoveGoalKey);
	}
	Memory->RequestId = 0;
	return EBTNodeResult::Aborted;
}
