#include "BTTask_DM_ClearCombatTarget.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UBTTask_DM_ClearCombatTarget::UBTTask_DM_ClearCombatTarget()
{
	NodeName = TEXT("DM Clear Combat Target");
}

EBTNodeResult::Type UBTTask_DM_ClearCombatTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	if (!AI) return EBTNodeResult::Failed;
	if (UDMCombatFireControlComponent* Fire = AI->FindComponentByClass<UDMCombatFireControlComponent>()) Fire->StopFire();
	UDMCombatKnowledgeComponent* Knowledge = AI->FindComponentByClass<UDMCombatKnowledgeComponent>();
	if (!Knowledge) return EBTNodeResult::Failed;
	Knowledge->ClearCombatTarget();
	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		BB->ClearValue(DMBlackboardKeys::CombatTarget);
		BB->ClearValue(DMBlackboardKeys::TargetActor);
		BB->SetValueAsBool(DMBlackboardKeys::TargetVisible, false);
		BB->SetValueAsBool(DMBlackboardKeys::HasRememberedTarget, false);
	}
	return EBTNodeResult::Succeeded;
}
