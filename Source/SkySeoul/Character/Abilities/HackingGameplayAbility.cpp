// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Abilities/HackingGameplayAbility.h"

void UHackingGameplayAbility::HackingStart(AActor* Caller)
{
}

void UHackingGameplayAbility::HackingAbort()
{
}

void UHackingGameplayAbility::HackingEnd(AActor* Caller)
{
}

bool UHackingGameplayAbility::CommitCheck(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, OUT FGameplayTagContainer* OptionalRelevantTags)
{
	if (!Super::CommitCheck(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags))
	{
		return false;
	}

	return true;
}

bool UHackingGameplayAbility::CommitAbilityCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, OUT FGameplayTagContainer* OptionalRelevantTags)
{
	Super::CommitAbilityCost(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags);
	check(ActorInfo);


	return true;
}
