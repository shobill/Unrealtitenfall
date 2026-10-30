// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Abilities/Execution/SkySeoulEffectExecution.h"
#include "SkySeoulEffectExecution.h"
#include "Character/Abilities/Attributes/CharacterAttributeSetBase.h"

struct FDamageStatics
{
	FGameplayEffectAttributeCaptureDefinition BaseDamageDef;
	FGameplayEffectAttributeCaptureDefinition BaseShieldDef;

	FDamageStatics()
	{
		BaseDamageDef = FGameplayEffectAttributeCaptureDefinition(UCharacterAttributeSetBase::GetDamageAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
		BaseShieldDef = FGameplayEffectAttributeCaptureDefinition(UCharacterAttributeSetBase::GetShieldAttribute(), EGameplayEffectAttributeCaptureSource::Target, true);
	}
};

static FDamageStatics& DamageStatics()
{
	static FDamageStatics Statics;
	return Statics;
}



USkySeoulEffectExecution::USkySeoulEffectExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().BaseDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().BaseShieldDef);
}

void USkySeoulEffectExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	UAbilitySystemComponent* SourceAbilitySystemComponent = ExecutionParams.GetSourceAbilitySystemComponent();
	UAbilitySystemComponent* TargetAbilitySystemComponent = ExecutionParams.GetTargetAbilitySystemComponent();

	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags = SourceTags;
	EvaluateParameters.TargetTags = TargetTags;

	float BaseDamage = 0.0f;
	float BaseShield = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().BaseDamageDef, EvaluateParameters, BaseDamage);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().BaseShieldDef, EvaluateParameters, BaseShield);

	if (BaseShield > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UCharacterAttributeSetBase::GetShieldAttribute(), EGameplayModOp::Additive, -BaseDamage));
		if (BaseShield - BaseDamage > 0.f)
		{
			BaseDamage = 0;
		}
		else 
		{
			BaseDamage -= BaseShield;
		}
	}
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UCharacterAttributeSetBase::GetHealthAttribute(), EGameplayModOp::Additive, -BaseDamage));
}
