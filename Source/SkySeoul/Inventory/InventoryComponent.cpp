// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/InventoryComponent.h"
#include "ItemObject.h"
#include "ItemActor.h"
#include "AbilitySystemBlueprintLibrary.h"

// Sets default values for this component's properties
UInventoryComponent::UInventoryComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	for (UItemObject* Item : DefalutItemInventory)
	{
		DefaultAddItem(Item);
	}
	// ...
	
}

bool UInventoryComponent::DefaultAddItem(UItemObject* Item)
{
	if (Item == nullptr)
		return false;
	if (Item->ItemInfo.ItemCategory != EItemType::E_Item_Equip)
	{
		for (UItemObject* InventoryItem : ItemInventory)
		{
			if (InventoryItem->ItemInfo.GetItemName() == Item->ItemInfo.GetItemName())
			{
				InventoryItem->ItemInfo.ItemCount += Item->ItemInfo.ItemCount;
				OnInventoryUpdated.Broadcast();
				return true;
			}
		}
	}
	Item->World = GetWorld();
	Item->Inventory = this;
	ItemInventory.AddUnique(Item);
	OnInventoryUpdated.Broadcast();
	return true;
}

bool UInventoryComponent::AddItem(UItemObject* Item)
{
	if (Item == nullptr)
		return false;
	FGameplayEventData Payload;
	Payload.OptionalObject = Item;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), FGameplayTag::RequestGameplayTag(FName("Ability.Player.ItemAdd")), Payload);
	if (Item->ItemInfo.ItemCategory != EItemType::E_Item_Equip)
	{
		for (UItemObject* InventoryItem : ItemInventory)
		{
			if (InventoryItem->ItemInfo.GetItemName() == Item->ItemInfo.GetItemName())
			{
				InventoryItem->ItemInfo.ItemCount += Item->ItemInfo.ItemCount;
				OnInventoryUpdated.Broadcast();
				return true;
			}
		}
	}
	Item->World = GetWorld();
	Item->Inventory = this;
	ItemInventory.AddUnique(Item);
	OnInventoryUpdated.Broadcast();
	return true;
}

bool UInventoryComponent::RemoveItem(UItemObject* Item)
{
	if (Item == nullptr)
		return false;

	Item->World = nullptr;
	Item->Inventory = nullptr;
	ItemInventory.RemoveSingle(Item);
	OnInventoryUpdated.Broadcast();
	return true;
}

UItemObject* UInventoryComponent::FindItem(FGameplayTag Tag) const
{
	for (auto TargetItem : ItemInventory)
	{
		if (TargetItem->ItemInfo.ItemTag == Tag)
			return TargetItem;
	}

	return nullptr;
}

void UInventoryComponent::UseItem(UItemObject* Item)
{
	Item->ItemUse();
	if (Item->ItemInfo.GetItemCount() == 0)
		RemoveItem(Item);
	OnInventoryUpdated.Broadcast();
}

void UInventoryComponent::UseItem(FGameplayTag Tag)
{
	UItemObject* Item = FindItem(Tag);
	UseItem(Item);
}


int32 UInventoryComponent::UseItemValue(UItemObject* Item, int32 Value)
{
	if (Item == nullptr)
		return 0;

	int32 Count = Item->ItemInfo.GetItemCount();
	if (Count - Value >= 0)
	{
		Item->ItemUse(Value);
		Count = Value;
	}
	else
		Item->ItemUse(Count);

	if (Item->ItemInfo.GetItemCount() == 0)
		RemoveItem(Item);
	OnInventoryUpdated.Broadcast();

	return Count;
}

bool UInventoryComponent::ItemCheck(UItemObject* Item, int32 Value)
{
	if (Item == nullptr)
		return false;

	int32 Count = Item->ItemInfo.GetItemCount();
	if (Count < Value)
		return false;
	else
		return true;
}

void UInventoryComponent::ItemDrop()
{
	FVector ActorLocation = GetOwner()->GetActorLocation();
	for (UItemObject* Item : ItemInventory)
	{
		FVector TempLocation = ActorLocation;
		TempLocation.X += FMath::FRandRange(0.f, 30.f);
		TempLocation.Y += FMath::FRandRange(0.f, 30.f);
		FHitResult Result;
		FVector StartLoc = TempLocation;
		FVector EndLoc = TempLocation;
		EndLoc.Z = -10000;
		GetWorld()->LineTraceSingleByChannel(Result, StartLoc, EndLoc, ECC_WorldStatic, FCollisionQueryParams::DefaultQueryParam);
		AItemActor* ItemActor = GetWorld()->SpawnActor<AItemActor>(DropItem, Result.Location, FRotator::ZeroRotator);

		ItemActor->Iteminitialization(Item);
	}
	ItemInventory.Empty();
}