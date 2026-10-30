// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "WeaponAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
/**
 * 
 */
UCLASS()
class SKYSEOUL_API UWeaponAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:

	UPROPERTY(BlueprintReadOnly, Category = "WeaponLevel", ReplicatedUsing = OnRep_WeaponLevel)
	FGameplayAttributeData WeaponLevel;
	ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, WeaponLevel)
	UPROPERTY(BlueprintReadOnly, Category = "CurrentMagazine", ReplicatedUsing = OnRep_CurrentMagazine)
	FGameplayAttributeData CurrentMagazine;
	ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, CurrentMagazine)
	UPROPERTY(BlueprintReadOnly, Category = "MaxMagazine", ReplicatedUsing = OnRep_MaxMagazine)
	FGameplayAttributeData MaxMagazine;
	ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, MaxMagazine)


	UPROPERTY(BlueprintReadOnly, Category = "ReloadTime", ReplicatedUsing = OnRep_ReloadTime)
	FGameplayAttributeData ReloadTime;
	ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, ReloadTime)
	UPROPERTY(BlueprintReadOnly, Category = "FireTime", ReplicatedUsing = OnRep_FireTime)
	FGameplayAttributeData FireTime;
	ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, FireTime)
	UPROPERTY(BlueprintReadOnly, Category = "WeaponPower", ReplicatedUsing = OnRep_WeaponPowerLevel)
	FGameplayAttributeData WeaponPowerLevel;
	ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, WeaponPowerLevel)
	UPROPERTY(BlueprintReadOnly, Category = "WeaponDistance")
	FGameplayAttributeData Distance;
	ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, Distance)

public:
	UFUNCTION()
	virtual void OnRep_WeaponLevel(const FGameplayAttributeData& OldWeaponLevel);
	UFUNCTION()
	virtual void OnRep_CurrentMagazine(const FGameplayAttributeData& OldCurrentMagazine);
	UFUNCTION()
	virtual void OnRep_MaxMagazine(const FGameplayAttributeData& OldMaxMagazine);
	UFUNCTION()
	virtual void OnRep_ReloadTime(const FGameplayAttributeData& OldReloadTime);
	UFUNCTION()
	virtual void OnRep_FireTime(const FGameplayAttributeData& OldFireTime);
	UFUNCTION()
	virtual void OnRep_WeaponPowerLevel(const FGameplayAttributeData& OldWeaponPowerLevel);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
