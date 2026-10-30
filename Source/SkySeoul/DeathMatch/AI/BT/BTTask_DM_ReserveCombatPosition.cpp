#include "BTTask_DM_ReserveCombatPosition.h"
#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/AI/Combat/DMCombatPositionRegistry.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "AIController.h"

UBTTask_DM_ReserveCombatPosition::UBTTask_DM_ReserveCombatPosition()
{
	NodeName = TEXT("DM Reserve Combat Position");
	LocationKey.SelectedKeyName = DMBlackboardKeys::MoveGoal;
	LocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_ReserveCombatPosition, LocationKey));
}
EBTNodeResult::Type UBTTask_DM_ReserveCombatPosition::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	UWorld* World = OwnerComp.GetWorld();
	UDMCombatPositionRegistry* Registry = World ? World->GetSubsystem<UDMCombatPositionRegistry>() : nullptr;
	if (!AI || !BB || !Registry || !BB->IsVectorValueSet(LocationKey.SelectedKeyName)) return EBTNodeResult::Failed;
	TArray<FVector> Goals{BB->GetValueAsVector(LocationKey.SelectedKeyName)};
	return Registry->Reserve(AI, Goals, Radius, World->GetTimeSeconds(), Lifetime)
		? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}
