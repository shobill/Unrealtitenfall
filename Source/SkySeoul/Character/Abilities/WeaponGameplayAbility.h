// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/Abilities/CharacterGameplayAbility.h"
#include "WeaponGameplayAbility.generated.h"

class UGunWeaponObject;
class UWeaponAttributeSet;
/**
 * 
 */
UCLASS()
class SKYSEOUL_API UWeaponGameplayAbility : public UCharacterGameplayAbility
{
	GENERATED_BODY()

protected:

	UGunWeaponObject* WeaponObject;
	const UWeaponAttributeSet* WeaponAttribute;
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float FireDelayTimeSecs;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FireDistance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FireLevel;

	FTransform GetEndLocation();



	FHitResult WeaponTrace(const FVector& StartTrace, const FVector& EndTrace, float SweepRadius, bool bIsSimulated, TArray<FHitResult>& OutHitResults) const;

	UFUNCTION(BlueprintCallable)
	void OnFire();

	UFUNCTION(BlueprintImplementableEvent)
	void TargetHit(const FGameplayAbilityTargetDataHandle& TargetData);

	void SetWeaponObject(const FGameplayAbilityActorInfo* ActorInfo);

	virtual bool CommitCheck(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) override;
	virtual bool CommitAbilityCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) override;

};
