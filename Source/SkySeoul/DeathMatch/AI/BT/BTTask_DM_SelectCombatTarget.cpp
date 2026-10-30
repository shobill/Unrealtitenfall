#include "BTTask_DM_SelectCombatTarget.h"

#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/AI/Targeting/DMCombatTargetSelectionPolicy.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_DM_SelectCombatTarget::UBTTask_DM_SelectCombatTarget()
{
	NodeName = TEXT("DM Select Combat Target");
}

EBTNodeResult::Type UBTTask_DM_SelectCombatTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(OwnerComp.GetAIOwner());
	UDMCombatKnowledgeComponent* Knowledge = AI ? AI->GetCombatKnowledge() : nullptr;
	const UDMCombatTargetSelectionPolicy* Policy = AI ? AI->GetTargetSelectionPolicy() : nullptr;
	AActor* Selected = Knowledge && Policy ? Policy->SelectTarget(*Knowledge, AI->GetPawn()) : nullptr;
	if (!Selected)
	{
		return EBTNodeResult::Failed;
	}
	Knowledge->SetCombatTarget(Selected);
	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		Blackboard->SetValueAsObject(DMBlackboardKeys::CombatTarget, Selected);
		Blackboard->SetValueAsObject(DMBlackboardKeys::TargetActor, Selected);
	}
	return EBTNodeResult::Succeeded;
}
