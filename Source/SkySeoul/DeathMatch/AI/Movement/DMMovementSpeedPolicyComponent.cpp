#include "DMMovementSpeedPolicyComponent.h"
#include "DeathMatch/AI/Execution/DMActionIntentTypes.h"

#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
FName SourceName(EDMMovementSpeedSource Source)
{
	switch (Source)
	{
	case EDMMovementSpeedSource::Roam: return TEXT("Roam");
	case EDMMovementSpeedSource::MovementAction: return TEXT("MovementAction");
	case EDMMovementSpeedSource::ADS: return TEXT("ADS");
	case EDMMovementSpeedSource::Firing: return TEXT("Firing");
	case EDMMovementSpeedSource::ActionIntent: return TEXT("ActionIntent");
	default: return TEXT("ExternalStatus");
	}
}
}

UDMMovementSpeedPolicyComponent::UDMMovementSpeedPolicyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDMMovementSpeedPolicyComponent::BeginPlay()
{
	Super::BeginPlay();
	CaptureBaseSpeed();
}

void UDMMovementSpeedPolicyComponent::CaptureBaseSpeed()
{
	const AAIController* AI = Cast<AAIController>(GetOwner());
	const ACharacter* Character = AI ? Cast<ACharacter>(AI->GetPawn()) : Cast<ACharacter>(GetOwner());
	if (Character && Character->GetCharacterMovement() && BaseWalkSpeed <= 0.f)
	{
		BaseWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
	}
}

void UDMMovementSpeedPolicyComponent::SetSpeedMultiplier(EDMMovementSpeedSource Source, float Multiplier)
{
	Multipliers.FindOrAdd(Source) = FMath::Clamp(FMath::IsFinite(Multiplier) ? Multiplier : 1.f, 0.f, 10.f);
	ApplyPolicy();
}

void UDMMovementSpeedPolicyComponent::ClearSpeedMultiplier(EDMMovementSpeedSource Source)
{
	Multipliers.Remove(Source);
	ApplyPolicy();
}

void UDMMovementSpeedPolicyComponent::ClearAllSpeedMultipliers()
{
	Multipliers.Reset();
	ApplyPolicy();
}

float UDMMovementSpeedPolicyComponent::GetActionIntentMultiplier(EDMActionIntent Intent)
{
	switch (Intent)
	{
	case EDMActionIntent::AttackMove:
	case EDMActionIntent::CombatCover:
		return .75f;
	case EDMActionIntent::Retreat:
		return .9f;
	case EDMActionIntent::Investigate:
		return .8f;
	default:
		return 1.f;
	}
}

void UDMMovementSpeedPolicyComponent::SetActionIntent(EDMActionIntent Intent)
{
	if (Intent == EDMActionIntent::None)
	{
		ClearSpeedMultiplier(EDMMovementSpeedSource::ActionIntent);
	}
	else
	{
		SetSpeedMultiplier(EDMMovementSpeedSource::ActionIntent, GetActionIntentMultiplier(Intent));
	}
}

void UDMMovementSpeedPolicyComponent::ApplyPolicy()
{
	CaptureBaseSpeed();
	EDMMovementSpeedSource LimitingSource = EDMMovementSpeedSource::ExternalStatus;
	AppliedMultiplier = CalculateAppliedMultiplier(Multipliers, &LimitingSource);
	LimitingSourceName = AppliedMultiplier < 1.f ? SourceName(LimitingSource) : NAME_None;

	const AAIController* AI = Cast<AAIController>(GetOwner());
	ACharacter* Character = AI ? Cast<ACharacter>(AI->GetPawn()) : Cast<ACharacter>(GetOwner());
	if (Character && Character->GetCharacterMovement() && BaseWalkSpeed > 0.f)
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * AppliedMultiplier;
	}
}

float UDMMovementSpeedPolicyComponent::CalculateAppliedMultiplier(
	const TMap<EDMMovementSpeedSource, float>& InMultipliers, EDMMovementSpeedSource* OutLimitingSource)
{
	float Result = 1.f;
	EDMMovementSpeedSource Limiter = EDMMovementSpeedSource::ExternalStatus;
	for (const TPair<EDMMovementSpeedSource, float>& Pair : InMultipliers)
	{
		const float SafeValue = FMath::Clamp(FMath::IsFinite(Pair.Value) ? Pair.Value : 1.f, 0.f, 10.f);
		if (SafeValue < Result) { Result = SafeValue; Limiter = Pair.Key; }
	}
	if (OutLimitingSource) *OutLimitingSource = Limiter;
	return Result;
}
