#include "DMGameplayAbilityBase.h"

#include "PartsCharacter/PartsCharacterBase.h"
#include "DeathMatch/Character/DeathMatchRobotCharacter.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "AIController.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"

UDMGameplayAbilityBase::UDMGameplayAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// AI uses AbilityTags + TryActivateAbilitiesByTag, not InputTag.
	ActivationPolicy = ESkySeoulAbilityActivationPolicy::OnInputTriggered;
}

bool UDMGameplayAbilityBase::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (bRequireCanShoot && ActorInfo)
	{
		if (const APartsCharacterBase* PartsChar = Cast<APartsCharacterBase>(ActorInfo->AvatarActor.Get()))
		{
			if (!PartsChar->CanShoot())
			{
				return false;
			}
		}
	}

	return true;
}

APartsCharacterBase* UDMGameplayAbilityBase::GetPartsCharacterFromActorInfo() const
{
	return CurrentActorInfo ? Cast<APartsCharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

ADeathMatchRobotCharacter* UDMGameplayAbilityBase::GetDeathMatchRobotFromActorInfo() const
{
	return CurrentActorInfo ? Cast<ADeathMatchRobotCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

AActor* UDMGameplayAbilityBase::GetDesiredTarget() const
{
    if (const auto* AI=Cast<ADeathMatchRobotAIController>(GetControllerFromActorInfo()))
        if (const auto* Knowledge=AI->FindComponentByClass<UDMCombatKnowledgeComponent>())
            if (AActor* Target = Knowledge->GetCombatTarget()) return Target;
	if (DesiredTarget.IsValid())
	{
		return DesiredTarget.Get();
	}

	if (const AController* Controller = GetControllerFromActorInfo())
	{
		if (const ADeathMatchRobotAIController* RobotAI = Cast<ADeathMatchRobotAIController>(Controller))
		{
			return RobotAI->GetFocusTarget();
		}
	}

	return nullptr;
}
