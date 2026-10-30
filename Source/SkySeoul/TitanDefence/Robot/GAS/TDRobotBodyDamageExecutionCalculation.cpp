#include "TitanDefence/Robot/GAS/TDRobotBodyDamageExecutionCalculation.h"

#include "AbilitySystemComponent.h"
#include "Character/Abilities/Attributes/CharacterAttributeSetBase.h"
#include "GameplayEffectExtension.h"
#include "TitanDefence/Robot/Core/TDRobotLog.h"
#include "TitanDefence/Robot/GAS/TDRobotGameplayTags.h"

void UTDRobotBodyDamageExecutionCalculation::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const float Damage = Spec.GetSetByCallerMagnitude(
		TDRobotGameplayTags::Data_Damage, false, 0.0f);
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	AActor* Owner = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	const float HealthBefore = TargetASC
		? TargetASC->GetNumericAttribute(UCharacterAttributeSetBase::GetHealthAttribute())
		: 0.0f;

	if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		UE_LOG(LogTDRobotDamage, Warning,
			TEXT("Robot body-damage execution ignored invalid SetByCaller: owner='%s' effect='%s' Data.Damage=%g Health=%.2f->%.2f."),
			*GetNameSafe(Owner), *GetNameSafe(Spec.Def), Damage, HealthBefore, HealthBefore);
		return;
	}

	const float HealthAfter = FMath::Max(0.0f, HealthBefore - Damage);
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Robot body-damage execution: owner='%s' effect='%s' Data.Damage=%.2f Health=%.2f->%.2f."),
		*GetNameSafe(Owner), *GetNameSafe(Spec.Def), Damage, HealthBefore, HealthAfter);
	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UCharacterAttributeSetBase::GetHealthAttribute(),
		EGameplayModOp::Additive,
		-Damage));
}
