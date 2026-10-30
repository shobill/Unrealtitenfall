#include "TitanDefence/Robot/State/Tests/TDRobotStateTestTypes.h"

#include "Character/Abilities/Attributes/CharacterAttributeSetBase.h"
#include "TitanDefence/Robot/Damage/TDRobotPartDamageComponent.h"
#include "TitanDefence/Robot/GAS/TDRobotAttributeSet.h"
#include "TitanDefence/Robot/GAS/TDRobotBodyDamageExecutionCalculation.h"
#include "TitanDefence/Robot/State/TDRobotStateComponent.h"

UTDRobotInitialAttributesTestEffect::UTDRobotInitialAttributesTestEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo& MaxHealthModifier = Modifiers.AddDefaulted_GetRef();
	MaxHealthModifier.Attribute = UCharacterAttributeSetBase::GetMaxHealthAttribute();
	MaxHealthModifier.ModifierOp = EGameplayModOp::Override;
	MaxHealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(100.0f));

	FGameplayModifierInfo& HealthModifier = Modifiers.AddDefaulted_GetRef();
	HealthModifier.Attribute = UCharacterAttributeSetBase::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Override;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(100.0f));
}

UTDRobotBodyDamageTestEffect::UTDRobotBodyDamageTestEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition& Execution = Executions.AddDefaulted_GetRef();
	Execution.CalculationClass = UTDRobotBodyDamageExecutionCalculation::StaticClass();
}

UTDRobotExternalHealthDamageTestEffect::UTDRobotExternalHealthDamageTestEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& HealthModifier = Modifiers.AddDefaulted_GetRef();
	HealthModifier.Attribute = UCharacterAttributeSetBase::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Additive;
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-25.0f));
}

ATDRobotStateTestCharacter::ATDRobotStateTestCharacter()
{
	GetRobotStateComponent()->InitialAttributesEffectClass =
		UTDRobotInitialAttributesTestEffect::StaticClass();
	GetRobotStateComponent()->BodyDamageEffectClass =
		UTDRobotBodyDamageTestEffect::StaticClass();
}

ATDRobotMissingDamageEffectTestCharacter::ATDRobotMissingDamageEffectTestCharacter()
{
	GetRobotStateComponent()->InitialAttributesEffectClass =
		UTDRobotInitialAttributesTestEffect::StaticClass();
}

void UTDRobotStateTestObserver::ForwardExternalDamage(
	const FTDRobotExternalGameplayEffectDamage& DamageRequest)
{
	++ExternalDamageCount;
	LastExternalDamage = DamageRequest;
	if (!PartDamageComponent)
	{
		return;
	}

	if (DamageRequest.bHasValidHitResult)
	{
		PartDamageComponent->ProcessPointDamage(
			DamageRequest.Damage,
			DamageRequest.HitResult,
			DamageRequest.InstigatorController,
			DamageRequest.SourceActor,
			nullptr);
	}
	else
	{
		PartDamageComponent->ProcessGenericDamage(
			DamageRequest.Damage,
			DamageRequest.InstigatorController,
			DamageRequest.SourceActor,
			nullptr);
	}
}

