// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/EquipmentObject.h"
#include "GunWeaponObject.generated.h"

/**
 * 
 */
UCLASS()
class SKYSEOUL_API UGunWeaponObject : public UEquipmentObject
{
	GENERATED_BODY()
	
protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine")
	FGameplayTag BulletTag;

public:

};
