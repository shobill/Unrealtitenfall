#include "BTTask_DM_DisengageCombat.h"

#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/AI/Combat/DMCombatPositionRegistry.h"
#include "DeathMatch/AI/Execution/DMActionIntentComponent.h"
#include "DeathMatch/AI/Execution/DMCombatFocusComponent.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/AI/Move/DMCombatMoveDirector.h"
#include "DeathMatch/AI/State/DMCombatStatusComponent.h"
#include "DeathMatch/Navigation/DMTacticalAreaSubsystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UBTTask_DM_DisengageCombat::UBTTask_DM_DisengageCombat()
{
	NodeName = TEXT("DM Disengage Combat");
}

EBTNodeResult::Type UBTTask_DM_DisengageCombat::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	if (!AI)
	{
		return EBTNodeResult::Failed;
	}
	const UDMCombatStatusComponent* Status = AI->FindComponentByClass<UDMCombatStatusComponent>();
	const bool bDead = Status && Status->IsDead();
	UDMCombatFireControlComponent* Fire = AI->FindComponentByClass<UDMCombatFireControlComponent>();
	UDMCombatKnowledgeComponent* Knowledge = AI->FindComponentByClass<UDMCombatKnowledgeComponent>();
	UDMCombatMoveDirector* Move = AI->FindComponentByClass<UDMCombatMoveDirector>();

	if (bDead)
	{
		if (Fire) Fire->ClearFireRequests();
		if (UDMActionIntentComponent* Intent = AI->FindComponentByClass<UDMActionIntentComponent>()) Intent->ResetIntentRequests();
		if (UDMCombatFocusComponent* Focus = AI->FindComponentByClass<UDMCombatFocusComponent>()) Focus->ResetFocusRequests();
		if (Move) Move->CancelAllMoveRequests(true);
		if (Knowledge) Knowledge->ResetKnowledge();
	}
	else
	{
		// Complete combat exit: preserve the new Roam branch request and let old BT services
		// release only their own Fire/Focus/Intent request IDs during OnCeaseRelevant.
		if (Fire) Fire->SetTarget(nullptr);
		if (Move) Move->TransitionToBehaviorState(EDMBehaviorState::Roam);
		if (Knowledge) Knowledge->ClearCombatTarget();
	}
	if (UDMCombatPositionRegistry* Registry = OwnerComp.GetWorld() ? OwnerComp.GetWorld()->GetSubsystem<UDMCombatPositionRegistry>() : nullptr) Registry->Release(AI);
	if (UDMTacticalAreaSubsystem* Areas = OwnerComp.GetWorld() ? OwnerComp.GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr)
	{
		Areas->ReleaseIncomingAI(AI->GetPawn());
	}
	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		Blackboard->ClearValue(DMBlackboardKeys::CombatTarget);
		Blackboard->ClearValue(DMBlackboardKeys::TargetActor);
		Blackboard->ClearValue(DMBlackboardKeys::SelectedTacticalArea);
		Blackboard->ClearValue(DMBlackboardKeys::MoveGoal);
	}
	return EBTNodeResult::Succeeded;
}
