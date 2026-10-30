// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Character/Abilities/Attributes/CharacterAttributeSetBase.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "ProjectPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStateUpdated);

/**
 * 
 */
UCLASS()
class SKYSEOUL_API AProjectPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Abilities")
	class UCharacterAbilitySystemComponent* AbilitySystemComponent;
	UPROPERTY()
	class UCharacterAttributeSetBase* AttributeSetBase;

	FGameplayTag DeadTag;
	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle CharacterLevelChangedDelegateHandle;
	FDelegateHandle SpChangedDelegateHandle;
	FDelegateHandle ShieldChangedDelegateHandle;

	virtual void BeginPlay() override;
	virtual void HealthChanged(const FOnAttributeChangeData& Data);
	virtual void MaxHealthChanged(const FOnAttributeChangeData& Data);
	virtual void CharacterLevelChanged(const FOnAttributeChangeData& Data);
	virtual void SpChanged(const FOnAttributeChangeData& Data);
	virtual void ShieldChanged(const FOnAttributeChangeData& Data);

	virtual void StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

public:
	AProjectPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UCharacterAttributeSetBase* GatAttributeSetBase() const;

	UFUNCTION(BlueprintCallable, Category = "PlayerState")
	bool IsAlive() const;

	UFUNCTION(BlueprintCallable, Category = "PlayerState")
	void ShowAbilityConfirmCancelText(bool ShowText);

	UFUNCTION(BlueprintCallable, Category = "PlayerState|Attributes")
	float GetHealth() const;
	UFUNCTION(BlueprintCallable, Category = "PlayerState|Attributes")
	float GetMaxHealth() const;
	UFUNCTION(BlueprintCallable, Category = "PlayerState|Attributes")
	float GetSp() const;
	UFUNCTION(BlueprintCallable, Category = "PlayerState|Attributes")
	float GetMaxSp() const;
	UFUNCTION(BlueprintCallable, Category = "PlayerState|Attributes")
	float GetShield() const;
	UFUNCTION(BlueprintCallable, Category = "PlayerState|Attributes")
	float GetMaxShield() const;
	UFUNCTION(BlueprintCallable, Category = "PlayerState|Attributes")
	int32 GetCharacterLevel() const;

	FTimerHandle HitTimer;
	void CharacterHit();
	void RemoveHitTag();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer PlayerStateTagContainer;


	//DELEGATE
	UPROPERTY(BlueprintAssignable)
	FOnStateUpdated OnStateUpdated;


};
