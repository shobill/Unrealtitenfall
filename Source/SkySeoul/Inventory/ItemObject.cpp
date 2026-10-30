// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/ItemObject.h"
#include "Inventory/InventoryComponent.h"

UAbilitySetData* UItemObject::GetAbilityData()
{
	return ItemAbilityData;
}

void UItemObject::ItemUse()
{
	ItemInfo.ItemCount--;
}

void UItemObject::ItemUse(int32 Value)
{
	ItemInfo.ItemCount -= Value;
}

bool UItemObject::UseItemCheck()
{
	if (ItemInfo.ItemCount - 1 < 0)
		return false;
	else
		return true;
}
