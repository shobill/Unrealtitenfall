// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Abilities/Execution/HealCalculation.h"
#include "Character/Abilities/Attributes/CharacterAttributeSetBase.h"

struct FMaxHplStatics
{
	FGameplayEffectAttributeCaptureDefinition BaseMaxHPDef;

	FMaxHplStatics()
	{
		BaseMaxHPDef = FGameplayEffectAttributeCaptureDefinition(UCharacterAttributeSetBase::GetMaxHealthAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
	}
};

static FMaxHplStatics& MaxHplStatics()
{
	static FMaxHplStatics Statics;
	return Statics;
}

UHealCalculation::UHealCalculation()
{
	RelevantAttributesToCapture.Add(FMaxHplStatics().BaseMaxHPDef);
}

void UHealCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	UAbilitySystemComponent* SourceAbilitySystemComponent = ExecutionParams.GetSourceAbilitySystemComponent();
	UAbilitySystemComponent* TargetAbilitySystemComponent = ExecutionParams.GetTargetAbilitySystemComponent();

	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags = SourceTags;
	EvaluateParameters.TargetTags = TargetTags;

	float BaseHeal = 0.0f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(MaxHplStatics().BaseMaxHPDef, EvaluateParameters, BaseHeal);

	const float BaseHealDone = BaseHeal;
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UCharacterAttributeSetBase::GetHealthAttribute(), EGameplayModOp::Additive, BaseHealDone));

}
