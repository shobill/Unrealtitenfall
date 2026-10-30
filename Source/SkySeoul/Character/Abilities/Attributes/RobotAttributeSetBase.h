// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "RobotAttributeSetBase.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
/**
 * 
 */
UCLASS()
class SKYSEOUL_API URobotAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:


	UPROPERTY(BlueprintReadOnly, Category = "Cost", ReplicatedUsing = OnRep_RobotCost)
	FGameplayAttributeData RobotCost;
	ATTRIBUTE_ACCESSORS(URobotAttributeSetBase, RobotCost)

	UPROPERTY(BlueprintReadOnly, Category = "HackingData", ReplicatedUsing = OnRep_CurrentHackingPercent)
	FGameplayAttributeData CurrentHackingPercent = 0.f;
	ATTRIBUTE_ACCESSORS(URobotAttributeSetBase, CurrentHackingPercent)

	UPROPERTY(BlueprintReadOnly, Category = "HackingData", ReplicatedUsing = OnRep_MaxHackingPercent)
	FGameplayAttributeData MaxHackingPercent = 100.f;
	ATTRIBUTE_ACCESSORS(URobotAttributeSetBase, MaxHackingPercent)

	UPROPERTY(BlueprintReadOnly, Category = "Attack", ReplicatedUsing = OnRep_MaxAttackRange)
	FGameplayAttributeData MaxAttackRange = 100.f;
	ATTRIBUTE_ACCESSORS(URobotAttributeSetBase, MaxAttackRange)


public:

	UFUNCTION()
	virtual void OnRep_RobotCost(const FGameplayAttributeData& OldRobotCost);
	UFUNCTION()
	virtual void OnRep_CurrentHackingPercent(const FGameplayAttributeData& OldCurrentHackingPercent);
	UFUNCTION()
	virtual void OnRep_MaxHackingPercent(const FGameplayAttributeData& OldMaxHackingPercent);
	UFUNCTION()
	virtual void OnRep_MaxAttackRange(const FGameplayAttributeData& OldMaxAttackRange);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
