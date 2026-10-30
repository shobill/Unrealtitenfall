#include "DMBTDataNodes.h"

#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/AI/State/DMCombatStatusComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

namespace
{
UDMCombatKnowledgeComponent* Knowledge(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	return AI ? AI->FindComponentByClass<UDMCombatKnowledgeComponent>() : nullptr;
}
UDMCombatFireControlComponent* Fire(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	return AI ? AI->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr;
}
UDMCombatStatusComponent* Status(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	return AI ? AI->FindComponentByClass<UDMCombatStatusComponent>() : nullptr;
}
}

UBTDecorator_DM_ReactiveStateCondition::UBTDecorator_DM_ReactiveStateCondition()
{
	bCreateNodeInstance = true;
	bTickIntervals = true;
	FlowAbortMode = EBTFlowAbortMode::Both;
	INIT_DECORATOR_NODE_NOTIFY_FLAGS();
}

void UBTDecorator_DM_ReactiveStateCondition::OnBecomeRelevant(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	SetNextTickTime(NodeMemory, FMath::Max(0.02f, ReevaluationInterval));
}

void UBTDecorator_DM_ReactiveStateCondition::TickNode(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float)
{
	ConditionalFlowAbort(OwnerComp, EBTDecoratorAbortRequest::ConditionResultChanged);
	SetNextTickTime(NodeMemory, FMath::Max(0.02f, ReevaluationInterval));
}

UBTService_DM_UpdateAIState::UBTService_DM_UpdateAIState() { NodeName = TEXT("DM Update AI State"); Interval = .2f; }
void UBTService_DM_UpdateAIState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8*, float)
{
	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		const UDMCombatStatusComponent* S = Status(OwnerComp);
		BB->SetValueAsEnum(DMBlackboardKeys::AIState,
			S ? static_cast<uint8>(S->GetAIState()) : 0);
	}
}

UBTService_DM_UpdateCombatBlackboard::UBTService_DM_UpdateCombatBlackboard() { NodeName = TEXT("DM Update Combat Blackboard"); Interval = .1f; }
void UBTService_DM_UpdateCombatBlackboard::TickNode(UBehaviorTreeComponent& OwnerComp, uint8*, float)
{
	UDMCombatKnowledgeComponent* K = Knowledge(OwnerComp);
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!K || !BB) return;
	BB->SetValueAsObject(DMBlackboardKeys::CombatTarget, K->GetCombatTarget());
	BB->SetValueAsObject(DMBlackboardKeys::TargetActor, K->GetCombatTarget());
	BB->SetValueAsBool(DMBlackboardKeys::TargetVisible, K->HasVisibleTarget());
	BB->SetValueAsBool(DMBlackboardKeys::HasRememberedTarget, K->HasRememberedTarget());
	BB->SetValueAsVector(DMBlackboardKeys::LastKnownTargetLocation, K->GetLastKnownThreatLocation());
	BB->SetValueAsVector(DMBlackboardKeys::TargetLocation, K->GetLastKnownThreatLocation());
	BB->SetValueAsVector(DMBlackboardKeys::LastDamageOrigin, K->GetDamageOrigin());
	BB->SetValueAsEnum(DMBlackboardKeys::ThreatAwareness, static_cast<uint8>(K->GetThreatAwareness()));
}

UBTService_DM_UpdateHealthRisk::UBTService_DM_UpdateHealthRisk() { NodeName = TEXT("DM Update Health Risk"); Interval = .25f; }
void UBTService_DM_UpdateHealthRisk::TickNode(UBehaviorTreeComponent& OwnerComp, uint8*, float)
{
	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		const UDMCombatStatusComponent* S = Status(OwnerComp);
		BB->SetValueAsFloat(DMBlackboardKeys::HealthRatio, S ? S->GetHealthRatio() : 1.f);
		BB->SetValueAsEnum(DMBlackboardKeys::RiskTier,
			S ? static_cast<uint8>(S->GetRiskTier()) : 0);
	}
}

UBTService_DM_UpdateFireState::UBTService_DM_UpdateFireState() { NodeName = TEXT("DM Update Fire State"); Interval = .1f; }
void UBTService_DM_UpdateFireState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8*, float)
{
	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		const UDMCombatFireControlComponent* F = Fire(OwnerComp);
		BB->SetValueAsBool(DMBlackboardKeys::ShouldFire, F && F->IsFireActive());
	}
}

UBTDecorator_DM_HasVisibleTarget::UBTDecorator_DM_HasVisibleTarget() { NodeName = TEXT("DM Has Visible Target"); }
bool UBTDecorator_DM_HasVisibleTarget::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8*) const
{
	const UDMCombatKnowledgeComponent* K = Knowledge(OwnerComp);
	const UWorld* World = OwnerComp.GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (K && K->HasVisibleTarget())
	{
		LastVisibleTime = Now;
		return true;
	}
	return ExitGracePeriod > 0.f && Now - LastVisibleTime <= ExitGracePeriod;
}

UBTDecorator_DM_HasRememberedTarget::UBTDecorator_DM_HasRememberedTarget() { NodeName = TEXT("DM Has Remembered Target"); }
bool UBTDecorator_DM_HasRememberedTarget::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8*) const
{ const UDMCombatKnowledgeComponent* K = Knowledge(OwnerComp); return K && K->HasRememberedTarget(); }

UBTDecorator_DM_HasValidCombatTarget::UBTDecorator_DM_HasValidCombatTarget() { NodeName = TEXT("DM Has Valid Combat Target"); }
bool UBTDecorator_DM_HasValidCombatTarget::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8*) const
{ const UDMCombatKnowledgeComponent* K = Knowledge(OwnerComp); return K && IsValid(K->GetCombatTarget()); }

UBTDecorator_DM_IsDead::UBTDecorator_DM_IsDead() { NodeName = TEXT("DM Is Dead"); }
bool UBTDecorator_DM_IsDead::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8*) const
{ const UDMCombatStatusComponent* S = Status(OwnerComp); return S && S->IsDead(); }

UBTDecorator_DM_IsEmergency::UBTDecorator_DM_IsEmergency() { NodeName = TEXT("DM Is Emergency"); }
bool UBTDecorator_DM_IsEmergency::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8*) const
{ const UDMCombatStatusComponent* S = Status(OwnerComp); return S && S->IsEmergency(); }

UBTDecorator_DM_ShouldRetreat::UBTDecorator_DM_ShouldRetreat() { NodeName = TEXT("DM Should Retreat"); }
bool UBTDecorator_DM_ShouldRetreat::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8*) const
{ const UDMCombatStatusComponent* S = Status(OwnerComp); return S && S->ShouldRetreat(); }

UBTDecorator_DM_NeedsCombatCover::UBTDecorator_DM_NeedsCombatCover() { NodeName = TEXT("DM Needs Combat Cover"); }
bool UBTDecorator_DM_NeedsCombatCover::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8*) const
{ const UDMCombatStatusComponent* S = Status(OwnerComp); return S && S->NeedsCombatCover(); }

UBTDecorator_DM_CanFire::UBTDecorator_DM_CanFire() { NodeName = TEXT("DM Can Fire"); }
bool UBTDecorator_DM_CanFire::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8*) const
{ const UDMCombatFireControlComponent* F = Fire(OwnerComp); return F && F->CanStartFire(); }

UBTDecorator_DM_HasClearShot::UBTDecorator_DM_HasClearShot()
{
	NodeName = TEXT("DM Has Clear Shot");
	bCreateNodeInstance = true;
	bTickIntervals = true;
	FlowAbortMode = EBTFlowAbortMode::Both;
	INIT_DECORATOR_NODE_NOTIFY_FLAGS();
}

bool UBTDecorator_DM_HasClearShot::CalculateRawConditionValue(
	UBehaviorTreeComponent& OwnerComp, uint8*) const
{
	const UDMCombatFireControlComponent* F = Fire(OwnerComp);
	return F && F->HasClearShotToCombatTarget();
}

void UBTDecorator_DM_HasClearShot::OnBecomeRelevant(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	SetNextTickTime(NodeMemory, FMath::Max(0.02f, ReevaluationInterval));
}

void UBTDecorator_DM_HasClearShot::TickNode(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float)
{
	ConditionalFlowAbort(OwnerComp, EBTDecoratorAbortRequest::ConditionResultChanged);
	SetNextTickTime(NodeMemory, FMath::Max(0.02f, ReevaluationInterval));
}

UBTDecorator_DM_ShouldInvestigate::UBTDecorator_DM_ShouldInvestigate() { NodeName = TEXT("DM Should Investigate"); }
bool UBTDecorator_DM_ShouldInvestigate::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8*) const
{ const UDMCombatStatusComponent* S = Status(OwnerComp); return S && S->ShouldInvestigate(); }

UBTDecorator_DM_ShouldDisengage::UBTDecorator_DM_ShouldDisengage() { NodeName = TEXT("DM Should Disengage"); }
bool UBTDecorator_DM_ShouldDisengage::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8*) const
{ const UDMCombatStatusComponent* S = Status(OwnerComp); return S && S->ShouldDisengage(); }
