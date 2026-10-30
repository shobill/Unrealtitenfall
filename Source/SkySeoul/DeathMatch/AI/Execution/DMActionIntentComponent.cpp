#include "DMActionIntentComponent.h"

#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/Movement/DMMovementSpeedPolicyComponent.h"
#include "DeathMatch/AI/Movement/DMMovementPolicyTypes.h"
#include "DeathMatch/AI/Move/DMCombatMoveDirector.h"
#include "DeathMatch/AI/Combat/DMCombatPositionRegistry.h"
#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/Navigation/DMTacticalAreaSubsystem.h"
#include "DeathMatch/Weapons/DMAimZoomComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UDMActionIntentComponent::UDMActionIntentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UDMActionIntentComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshADSState();
}

uint64 UDMActionIntentComponent::BeginIntentRequest(EDMActionIntent Intent)
{
	const uint64 RequestId = NextRequestId++;
	FIntentRequest& Request = IntentRequests.Add(RequestId);
	Request.Intent = Intent;
	Request.Serial = NextSerial++;
	ApplyWinningIntent();
	return RequestId;
}

uint64 UDMActionIntentComponent::BeginIntentRequest(EDMActionIntent Intent,
	const FDMActionIntentExecutionSettings& ExecutionSettings)
{
	const uint64 RequestId = NextRequestId++;
	FIntentRequest& Request = IntentRequests.Add(RequestId);
	Request.Intent = Intent;
	Request.ExecutionSettings = ExecutionSettings;
	Request.bHasExecutionOverride = true;
	Request.Serial = NextSerial++;
	ApplyWinningIntent();
	return RequestId;
}

void UDMActionIntentComponent::UpdateIntentRequest(uint64 RequestId, EDMActionIntent Intent)
{
	FIntentRequest* Request = IntentRequests.Find(RequestId);
	if (!Request || (Request->Intent == Intent && !Request->bHasExecutionOverride)) return;
	Request->Intent = Intent;
	Request->bHasExecutionOverride = false;
	ApplyWinningIntent();
}

void UDMActionIntentComponent::UpdateIntentRequest(uint64 RequestId, EDMActionIntent Intent,
	const FDMActionIntentExecutionSettings& ExecutionSettings)
{
	FIntentRequest* Request = IntentRequests.Find(RequestId);
	if (!Request) return;
	const bool bUnchanged = Request->Intent == Intent && Request->bHasExecutionOverride
		&& Request->ExecutionSettings.bUseADSWhileMoving == ExecutionSettings.bUseADSWhileMoving
		&& Request->ExecutionSettings.bAllowFireWhileMoving == ExecutionSettings.bAllowFireWhileMoving;
	if (bUnchanged) return;
	Request->Intent = Intent;
	Request->ExecutionSettings = ExecutionSettings;
	Request->bHasExecutionOverride = true;
	ApplyWinningIntent();
}

void UDMActionIntentComponent::EndIntentRequest(uint64 RequestId)
{
	if (RequestId != 0 && IntentRequests.Remove(RequestId) > 0)
	{
		ApplyWinningIntent();
	}
}

void UDMActionIntentComponent::ResetIntentRequests()
{
	IntentRequests.Reset();
	ActiveBehaviorState = EDMBehaviorState::None;
	ApplyIntent(EDMActionIntent::None);
}

void UDMActionIntentComponent::ApplyWinningIntent()
{
	EDMActionIntent WinningIntent = EDMActionIntent::None;
	const FDMActionIntentExecutionSettings* WinningSettings = nullptr;
	int32 WinningPriority = TNumericLimits<int32>::Lowest();
	uint64 WinningSerial = 0;
	for (const TPair<uint64, FIntentRequest>& Pair : IntentRequests)
	{
		const int32 Priority = GetIntentPriority(Pair.Value.Intent);
		if (Priority > WinningPriority || (Priority == WinningPriority && Pair.Value.Serial > WinningSerial))
		{
			WinningPriority = Priority;
			WinningSerial = Pair.Value.Serial;
			WinningIntent = Pair.Value.Intent;
			WinningSettings = Pair.Value.bHasExecutionOverride ? &Pair.Value.ExecutionSettings : nullptr;
		}
	}
	TransitionBehaviorState(GetBehaviorStateForIntent(WinningIntent));
	ApplyIntent(WinningIntent, WinningSettings);
}

EDMBehaviorState UDMActionIntentComponent::GetBehaviorStateForIntent(EDMActionIntent Intent)
{
	switch (Intent)
	{
	case EDMActionIntent::Roam: return EDMBehaviorState::Roam;
	case EDMActionIntent::Observe:
	case EDMActionIntent::Investigate: return EDMBehaviorState::LostTarget;
	case EDMActionIntent::Hide: return EDMBehaviorState::Hide;
	case EDMActionIntent::Retreat: return EDMBehaviorState::Retreat;
	case EDMActionIntent::Attack:
	case EDMActionIntent::AttackMove:
	case EDMActionIntent::FastReposition:
	case EDMActionIntent::CombatCover: return EDMBehaviorState::Combat;
	default: return EDMBehaviorState::None;
	}
}

void UDMActionIntentComponent::TransitionBehaviorState(EDMBehaviorState NewState)
{
	if (NewState == ActiveBehaviorState)
	{
		return;
	}

	AAIController* AI = Cast<AAIController>(GetOwner());
	if (NewState != EDMBehaviorState::None)
	{
		if (UDMCombatMoveDirector* Move = AI ? AI->FindComponentByClass<UDMCombatMoveDirector>() : nullptr)
		{
			Move->TransitionToBehaviorState(NewState);
		}
	}

	if (ActiveBehaviorState == EDMBehaviorState::Roam)
	{
		if (UDMTacticalAreaSubsystem* Areas = GetWorld() ? GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr)
		{
			Areas->ReleaseIncomingAI(AI ? AI->GetPawn() : nullptr);
		}
		if (UBlackboardComponent* Blackboard = AI ? AI->GetBlackboardComponent() : nullptr)
		{
			Blackboard->ClearValue(DMBlackboardKeys::SelectedTacticalArea);
			Blackboard->ClearValue(DMBlackboardKeys::MoveGoal);
		}
	}

	const bool bLeavingPositionState = ActiveBehaviorState == EDMBehaviorState::Combat
		|| ActiveBehaviorState == EDMBehaviorState::Retreat || ActiveBehaviorState == EDMBehaviorState::Hide;
	if (bLeavingPositionState)
	{
		if (UDMCombatPositionRegistry* Registry = GetWorld() ? GetWorld()->GetSubsystem<UDMCombatPositionRegistry>() : nullptr)
		{
			Registry->Release(AI);
		}
	}

	if (NewState == EDMBehaviorState::Roam)
	{
		if (UDMCombatKnowledgeComponent* Knowledge = AI ? AI->FindComponentByClass<UDMCombatKnowledgeComponent>() : nullptr)
		{
			Knowledge->ClearCombatTarget();
		}
		if (UDMCombatFireControlComponent* Fire = AI ? AI->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr)
		{
			Fire->SetTarget(nullptr);
		}
	}

	ActiveBehaviorState = NewState;
}

void UDMActionIntentComponent::ApplyIntent(EDMActionIntent Intent,
	const FDMActionIntentExecutionSettings* ExecutionSettings)
{
	ActiveIntent = Intent;
	bHasActiveExecutionOverride = ExecutionSettings != nullptr;
	ActiveExecutionSettings = ExecutionSettings ? *ExecutionSettings : FDMActionIntentExecutionSettings();
	RefreshADSState();
	if (UDMMovementSpeedPolicyComponent* Speed = GetOwner() ? GetOwner()->FindComponentByClass<UDMMovementSpeedPolicyComponent>() : nullptr)
	{
		Speed->SetActionIntent(Intent);
	}
	if (UDMCombatFireControlComponent* Fire = GetOwner() ? GetOwner()->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr)
	{
		Fire->SetActionIntent(Intent, ExecutionSettings
			? TOptional<bool>(ExecutionSettings->bAllowFireWhileMoving) : TOptional<bool>());
	}
}

bool UDMActionIntentComponent::IntentRequiresADS(EDMActionIntent Intent)
{
	return Intent == EDMActionIntent::Observe || Intent == EDMActionIntent::Attack
		|| Intent == EDMActionIntent::AttackMove || Intent == EDMActionIntent::CombatCover
		|| Intent == EDMActionIntent::Retreat || Intent == EDMActionIntent::Investigate;
}

bool UDMActionIntentComponent::IntentUsesADSWhileMovingByDefault(EDMActionIntent Intent)
{
	return Intent == EDMActionIntent::Attack || Intent == EDMActionIntent::AttackMove
		|| Intent == EDMActionIntent::CombatCover;
}

bool UDMActionIntentComponent::ShouldUseADS(EDMActionIntent Intent, bool bIsMoving,
	const FDMActionIntentExecutionSettings* ExecutionSettings)
{
	if (!IntentRequiresADS(Intent)) return false;
	if (!bIsMoving) return true;
	return ExecutionSettings
		? ExecutionSettings->bUseADSWhileMoving
		: IntentUsesADSWhileMovingByDefault(Intent);
}

int32 UDMActionIntentComponent::GetIntentPriority(EDMActionIntent Intent)
{
	switch (Intent)
	{
	case EDMActionIntent::Retreat: return 500;
	case EDMActionIntent::Hide: return 450;
	case EDMActionIntent::CombatCover: return 400;
	case EDMActionIntent::Attack:
	case EDMActionIntent::AttackMove: return 300;
	case EDMActionIntent::FastReposition: return 200;
	case EDMActionIntent::Observe: return 150;
	case EDMActionIntent::Investigate: return 140;
	case EDMActionIntent::Roam: return 100;
	default: return 0;
	}
}

void UDMActionIntentComponent::RefreshADSState()
{
	AAIController* AI = Cast<AAIController>(GetOwner());
	AActor* Avatar = AI ? AI->GetPawn() : GetOwner();
	if (UDMAimZoomComponent* AimZoom = UDMAimZoomComponent::FindOnActor(Avatar))
	{
		const bool bIsMoving = Avatar && Avatar->GetVelocity().SizeSquared2D()
			> FMath::Square(1.0 + UE_KINDA_SMALL_NUMBER);
		AimZoom->SetAiming(ShouldUseADS(ActiveIntent, bIsMoving,
			bHasActiveExecutionOverride ? &ActiveExecutionSettings : nullptr));
	}
}
