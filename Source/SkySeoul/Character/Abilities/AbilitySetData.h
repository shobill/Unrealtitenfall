// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "AbilitySetData.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UAttributeSet;
class UCharacterAbilitySystemComponent;

USTRUCT(BlueprintType)
struct FAbilitySetData_GameplayAbility
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayAbility> Ability = nullptr;

	UPROPERTY(EditDefaultsOnly)
	int32 AbilityLevel = 1;

	UPROPERTY(EditDefaultsOnly)
	FName AbilityName;

	UPROPERTY(EditDefaultsOnly, Meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag EventTag;
};

USTRUCT(BlueprintType)
struct FAbilitySetData_GameplayEffect
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FName EffectName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float EffectLevel = 1.0f;
};

USTRUCT(BlueprintType)
struct FAbilitySetData_AttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UAttributeSet> AttributeSet;

};
USTRUCT(BlueprintType)
struct FAbilitySetData_GrantedHandles
{
	GENERATED_BODY()

public:

	void AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle);
	void AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle);
	void AddAttributeSet(UAttributeSet* Set);

	void TakeFromAbilitySystem(UCharacterAbilitySystemComponent* CharacterASC);

protected:

	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY()
	TArray<FActiveGameplayEffectHandle> GameplayEffectHandles;

	UPROPERTY()
	TArray<TObjectPtr<UAttributeSet>> GrantedAttributeSets;
};


/**
 * 
 */
UCLASS()
class SKYSEOUL_API UAbilitySetData : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:

	UAbilitySetData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Grants the ability set to the specified ability system component.
	// The returned handles can be used later to take away anything that was granted.
	void GiveToAbilitySystem(UAbilitySystemComponent* CharacterASC, FAbilitySetData_GrantedHandles* OutGrantedHandles, UObject* SourceObject = nullptr) const;

	UFUNCTION(BlueprintCallable)
	int32 GetAbilityCount();
	
	UFUNCTION(BlueprintCallable)
	FGameplayTag GetAbilityEventTag(int32 Value);

	UFUNCTION(BlueprintCallable)
	TArray<FText> SelectAbilityText();

protected:

	// Gameplay abilities to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Abilities", meta = (TitleProperty = Ability))
	TArray<FAbilitySetData_GameplayAbility> DefaultGrantedGameplayAbilities;

	// Gameplay abilities to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Abilities", meta = (TitleProperty = Ability))
	TArray<FAbilitySetData_GameplayAbility> SelectGrantedGameplayAbilities;

	// Gameplay effects to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects", meta = (TitleProperty = GameplayEffect))
	TArray<FAbilitySetData_GameplayEffect> GrantedGameplayEffects;

	// Attribute sets to grant when this ability set is granted.
	UPROPERTY(EditDefaultsOnly, Category = "Attribute Sets", meta = (TitleProperty = AttributeSet))
	TArray<FAbilitySetData_AttributeSet> GrantedAttributes;

};
