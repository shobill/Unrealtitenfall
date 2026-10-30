#include "DMBTExecutionServices.h"

#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/AI/Execution/DMActionIntentComponent.h"
#include "DeathMatch/AI/Execution/DMCombatFocusComponent.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

namespace
{
struct FDMOwnedRequestMemory
{
	uint64 RequestId = 0;
	bool bHasExecutionOverride = false;
	bool bLastADS = false;
	bool bLastMovingFire = false;
};

template <typename T>
T* Component(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	return AI ? AI->FindComponentByClass<T>() : nullptr;
}

void UpdateFocus(UBehaviorTreeComponent& OwnerComp, uint64 RequestId, EDMFocusTargetSource Source)
{
	UDMCombatFocusComponent* Focus = Component<UDMCombatFocusComponent>(OwnerComp);
	const UDMCombatKnowledgeComponent* Knowledge = Component<UDMCombatKnowledgeComponent>(OwnerComp);
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Focus || RequestId == 0)
	{
		return;
	}
	switch (Source)
	{
	case EDMFocusTargetSource::CombatTarget:
		Focus->UpdateActorFocus(RequestId, Knowledge ? Knowledge->GetCombatTarget() : nullptr);
		break;
	case EDMFocusTargetSource::LastKnownTargetLocation:
		Focus->UpdateLocationFocus(RequestId, Knowledge ? Knowledge->GetLastKnownThreatLocation() : FVector::ZeroVector);
		break;
	case EDMFocusTargetSource::MoveGoal:
		Focus->UpdateLocationFocus(RequestId, Blackboard
			? Blackboard->GetValueAsVector(DMBlackboardKeys::MoveGoal) : FVector::ZeroVector);
		break;
	}
}
}

UBTService_DM_ActionIntent::UBTService_DM_ActionIntent()
{
	NodeName = TEXT("DM Action Intent");
	Interval = .1f;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
	bNotifyTick = true;
	ShouldADSKey.SelectedKeyName = DMBlackboardKeys::ShouldADS;
	ShouldADSKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_DM_ActionIntent, ShouldADSKey));
	FireWhileMovingKey.SelectedKeyName = DMBlackboardKeys::FireWhileMoving;
	FireWhileMovingKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_DM_ActionIntent, FireWhileMovingKey));
}

uint16 UBTService_DM_ActionIntent::GetInstanceMemorySize() const { return sizeof(FDMOwnedRequestMemory); }

FDMActionIntentExecutionSettings UBTService_DM_ActionIntent::ChooseExecutionSettings(
	bool bHasFixedOverride, const FDMActionIntentExecutionSettings& FixedSettings,
	bool bUseBlackboardSettings, bool bBlackboardADS, bool bBlackboardMovingFire,
	bool& bOutHasOverride)
{
	bOutHasOverride = bHasFixedOverride || bUseBlackboardSettings;
	if (bHasFixedOverride) return FixedSettings;
	FDMActionIntentExecutionSettings Result;
	Result.bUseADSWhileMoving = bBlackboardADS;
	Result.bAllowFireWhileMoving = bBlackboardMovingFire;
	return Result;
}

void UBTService_DM_ActionIntent::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FDMOwnedRequestMemory* Memory = reinterpret_cast<FDMOwnedRequestMemory*>(NodeMemory);
	if (UDMActionIntentComponent* Intent = Component<UDMActionIntentComponent>(OwnerComp))
	{
		const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
		FDMActionIntentExecutionSettings FixedSettings;
		FixedSettings.bUseADSWhileMoving = bUseADSWhileMoving;
		FixedSettings.bAllowFireWhileMoving = bAllowFireWhileMoving;
		bool bHasOverride = false;
		const FDMActionIntentExecutionSettings Settings = ChooseExecutionSettings(
			bOverrideExecutionSettings, FixedSettings, bUseBlackboardExecutionSettings,
			Blackboard && Blackboard->GetValueAsBool(ShouldADSKey.SelectedKeyName),
			Blackboard && Blackboard->GetValueAsBool(FireWhileMovingKey.SelectedKeyName), bHasOverride);
		Memory->bHasExecutionOverride = bHasOverride;
		Memory->bLastADS = Settings.bUseADSWhileMoving;
		Memory->bLastMovingFire = Settings.bAllowFireWhileMoving;
		if (bHasOverride)
		{
			Memory->RequestId = Intent->BeginIntentRequest(ActionIntent, Settings);
		}
		else Memory->RequestId = Intent->BeginIntentRequest(ActionIntent);
	}
}

void UBTService_DM_ActionIntent::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FDMOwnedRequestMemory* Memory = reinterpret_cast<FDMOwnedRequestMemory*>(NodeMemory);
	if (UDMActionIntentComponent* Intent = Component<UDMActionIntentComponent>(OwnerComp))
	{
		Intent->EndIntentRequest(Memory->RequestId);
	}
	Memory->RequestId = 0;
}

void UBTService_DM_ActionIntent::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float)
{
	if (bOverrideExecutionSettings || !bUseBlackboardExecutionSettings) return;
	FDMOwnedRequestMemory* Memory = reinterpret_cast<FDMOwnedRequestMemory*>(NodeMemory);
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	UDMActionIntentComponent* Intent = Component<UDMActionIntentComponent>(OwnerComp);
	if (!Blackboard || !Intent || Memory->RequestId == 0) return;
	const bool bADS = Blackboard->GetValueAsBool(ShouldADSKey.SelectedKeyName);
	const bool bMovingFire = Blackboard->GetValueAsBool(FireWhileMovingKey.SelectedKeyName);
	if (Memory->bHasExecutionOverride && Memory->bLastADS == bADS
		&& Memory->bLastMovingFire == bMovingFire) return;
	FDMActionIntentExecutionSettings Settings;
	Settings.bUseADSWhileMoving = bADS;
	Settings.bAllowFireWhileMoving = bMovingFire;
	Intent->UpdateIntentRequest(Memory->RequestId, ActionIntent, Settings);
	Memory->bHasExecutionOverride = true;
	Memory->bLastADS = bADS;
	Memory->bLastMovingFire = bMovingFire;
}

UBTService_DM_MaintainFocus::UBTService_DM_MaintainFocus()
{
	NodeName = TEXT("DM Maintain Focus");
	Interval = .1f;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
}

uint16 UBTService_DM_MaintainFocus::GetInstanceMemorySize() const { return sizeof(FDMOwnedRequestMemory); }

void UBTService_DM_MaintainFocus::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FDMOwnedRequestMemory* Memory = reinterpret_cast<FDMOwnedRequestMemory*>(NodeMemory);
	if (UDMCombatFocusComponent* Focus = Component<UDMCombatFocusComponent>(OwnerComp))
	{
		Memory->RequestId = FocusTarget == EDMFocusTargetSource::CombatTarget
			? Focus->BeginActorFocus(nullptr) : Focus->BeginLocationFocus(FVector::ZeroVector);
		UpdateFocus(OwnerComp, Memory->RequestId, FocusTarget);
	}
}

void UBTService_DM_MaintainFocus::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FDMOwnedRequestMemory* Memory = reinterpret_cast<FDMOwnedRequestMemory*>(NodeMemory);
	if (UDMCombatFocusComponent* Focus = Component<UDMCombatFocusComponent>(OwnerComp))
	{
		Focus->EndFocusRequest(Memory->RequestId);
	}
	Memory->RequestId = 0;
}

void UBTService_DM_MaintainFocus::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float)
{
	UpdateFocus(OwnerComp, reinterpret_cast<FDMOwnedRequestMemory*>(NodeMemory)->RequestId, FocusTarget);
}

UBTService_DM_MaintainFire::UBTService_DM_MaintainFire()
{
	NodeName = TEXT("DM Maintain Fire");
	Interval = .05f;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
	bNotifyTick = true;
}

uint16 UBTService_DM_MaintainFire::GetInstanceMemorySize() const { return sizeof(FDMOwnedRequestMemory); }

void UBTService_DM_MaintainFire::BeginOrRefreshOwnedRequest(
	UDMCombatFireControlComponent* Fire, uint64& RequestId)
{
	if (!Fire)
	{
		return;
	}
	if (RequestId == 0)
	{
		RequestId = Fire->BeginFireRequest();
		return;
	}
	if (!Fire->IsFireActive())
	{
		Fire->RefreshFireRequest(RequestId);
	}
}

void UBTService_DM_MaintainFire::EndOwnedRequest(
	UDMCombatFireControlComponent* Fire, uint64& RequestId)
{
	if (Fire && RequestId != 0)
	{
		Fire->EndFireRequest(RequestId);
	}
	RequestId = 0;
}

void UBTService_DM_MaintainFire::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FDMOwnedRequestMemory* Memory = reinterpret_cast<FDMOwnedRequestMemory*>(NodeMemory);
	BeginOrRefreshOwnedRequest(Component<UDMCombatFireControlComponent>(OwnerComp), Memory->RequestId);
}

void UBTService_DM_MaintainFire::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FDMOwnedRequestMemory* Memory = reinterpret_cast<FDMOwnedRequestMemory*>(NodeMemory);
	EndOwnedRequest(Component<UDMCombatFireControlComponent>(OwnerComp), Memory->RequestId);
}

void UBTService_DM_MaintainFire::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float)
{
	if (UDMCombatFireControlComponent* Fire = Component<UDMCombatFireControlComponent>(OwnerComp))
	{
		FDMOwnedRequestMemory* Memory = reinterpret_cast<FDMOwnedRequestMemory*>(NodeMemory);
		if (Memory->RequestId != 0 && !Fire->IsFireActive())
		{
			Fire->RefreshFireRequest(Memory->RequestId);
		}
	}
}
