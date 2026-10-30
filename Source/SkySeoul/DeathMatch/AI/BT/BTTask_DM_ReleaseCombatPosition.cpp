#include "BTTask_DM_ReleaseCombatPosition.h"
#include "DeathMatch/AI/Combat/DMCombatPositionRegistry.h"
#include "AIController.h"
UBTTask_DM_ReleaseCombatPosition::UBTTask_DM_ReleaseCombatPosition() { NodeName = TEXT("DM Release Combat Position"); }
EBTNodeResult::Type UBTTask_DM_ReleaseCombatPosition::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	UDMCombatPositionRegistry* Registry = OwnerComp.GetWorld() ? OwnerComp.GetWorld()->GetSubsystem<UDMCombatPositionRegistry>() : nullptr;
	if (!AI || !Registry) return EBTNodeResult::Failed;
	Registry->Release(AI);
	return EBTNodeResult::Succeeded;
}
