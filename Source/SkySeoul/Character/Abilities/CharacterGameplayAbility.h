// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CharacterGameplayAbility.generated.h"

/**
 * 
 */

UENUM(BlueprintType)
enum class ESkySeoulAbilityActivationPolicy : uint8
{
	// Try to activate the ability when the input is triggered.
	OnInputTriggered,

	// Continually try to activate the ability while the input is active.
	WhileInputActive,

	// Try to activate the ability when an avatar is assigned.
	OnSpawn
};

UCLASS()
class SKYSEOUL_API UCharacterGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UCharacterGameplayAbility();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	FGameplayTag InputTag;


	UFUNCTION(BlueprintCallable, Category = "Ability")
	UCharacterAbilitySystemComponent* GetCharacterAbilitySystemComponentFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Ability")
	APlayerController* GetPlayerControllerFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Ability")
	AController* GetControllerFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Ability")
	class ACharacterBase* GetCharacterFromActorInfo() const;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability")
	ESkySeoulAbilityActivationPolicy ActivationPolicy = ESkySeoulAbilityActivationPolicy::OnInputTriggered;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ability")
	bool ActivateAbilityOnGranted = false;

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfor, const FGameplayAbilitySpec& Spec) override;

	ESkySeoulAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }
};
