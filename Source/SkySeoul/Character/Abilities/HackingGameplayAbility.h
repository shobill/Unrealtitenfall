// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/Abilities/CharacterGameplayAbility.h"
#include "HackingGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class SKYSEOUL_API UHackingGameplayAbility : public UCharacterGameplayAbility
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HackingData")
	float HackingPercent = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HackingData")
	float CurrentHackingPercent = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HackingData")
	float MaxHackingPercent = 100.f;

	void HackingStart(AActor* Caller);
	void HackingAbort();
	void HackingEnd(AActor* Caller);



	virtual bool CommitCheck(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) override;
	virtual bool CommitAbilityCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) override;

};
