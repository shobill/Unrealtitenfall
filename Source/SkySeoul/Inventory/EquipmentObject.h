// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/ItemObject.h"
#include "Engine/CurveTable.h"
#include "EquipmentObject.generated.h"

class UAttributeSet;


/**
 * 
 */
UCLASS()
class SKYSEOUL_API UEquipmentObject : public UItemObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TSubclassOf<UAttributeSet> AttributeSet;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category = "Ability")
	UAttributeSet* EquipAttribute;
};
