// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/EquipmentComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "AttributeSet.h"

#include "Character/Abilities/CharacterAbilitySystemComponent.h"
#include "Character/Abilities/AbilitySetData.h"
#include "ProjectPlayerState.h"
#include "Character/SkySeoulCharacter.h"
#include "Inventory/ItemObject.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentObject.h"
#include "Character/Abilities/Attributes/WeaponAttributeSet.h"
#include "Inventory/ItemActor.h"


// Sets default values for this component's properties
UEquipmentComponent::UEquipmentComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	OwnerCharacter = Cast<ASkySeoulCharacter>(GetOwner());
	if (OwnerCharacter)
	{
		CharacterInvnetoryComponent = OwnerCharacter->InventoryComponent;
		AProjectPlayerState* PS = OwnerCharacter->GetPlayerState<AProjectPlayerState>();
		if (PS)
		{
			AbilitySystemComponent = Cast<UCharacterAbilitySystemComponent>(PS->GetAbilitySystemComponent());
			AttributeSetBase = PS->GatAttributeSetBase();
		}
	}
	WeaponChangeUp();
	QuickSlotInitialize();
	OnSelectUpdated.Broadcast();
}

void UEquipmentComponent::SetSelectItem(UItemObject* Item)
{
	if (SelectAttribute != nullptr)
		AbilitySystemComponent->RemoveSpawnedAttribute(SelectAttribute);
	if (Item == SelectedItem)
		return;

	RemoveAbility(SelectedItem);
	SelectedItem = Item;
	AddAbilityData(Item);
	if (Item == nullptr)
	{
		SelectedAbilityActionData = nullptr;
		OwnerCharacter->SetPlayerActionState(FGameplayTag::EmptyTag);
	}
	else
	{
		OwnerCharacter->SetPlayerActionState(Item->ItemInfo.ItemStateTag);
	}

	OwnerCharacter->SelectNumberReset();
	SpawnItem();
	OnSelectUpdated.Broadcast();
}

void UEquipmentComponent::SpawnItem()
{
	if (SelectedItem == nullptr)
	{
		if (EquipActor != nullptr)
		{
			EquipActor->Destroy();
			EquipActor = nullptr;
		}
		return;
	}
	if (EquipActor != nullptr)
	{
		EquipActor->Destroy();
		EquipActor = nullptr;
	}
	if (SelectedItem->ActorToSpawn == nullptr)
		return;

	EquipActor = GetWorld()->SpawnActor(SelectedItem->ActorToSpawn);
	EquipActor->SetActorTransform(SelectedItem->AttachTransform);
	EquipActor->AttachToComponent(OwnerCharacter->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, SelectedItem->AttachSocket);
}

void UEquipmentComponent::QuickSlotInitialize()
{
	for (int32 index = 0; index < 4; index++)
	{
		QuickSlotItem.AddDefaulted();
	}
}

void UEquipmentComponent::SetQuickSlot(int32 Index, UItemObject* Object)
{
	RemoveQuickSlot(Object);

	QuickSlotItem[Index] = Object;
	OnEquipUpdated.Broadcast();
}

void UEquipmentComponent::RemoveQuickSlot(UItemObject* Object)
{
	if (QuickSlotItem.Contains(Object))
	{
		QuickSlotItem[QuickSlotItem.Find(Object)] = nullptr;
	}
	OnEquipUpdated.Broadcast();
}

bool UEquipmentComponent::UseSelectedItem()
{
	if (SelectedItem == nullptr)
		return false;
	if (SelectedItem->ItemInfo.ItemCategory != EItemType::E_Item_Consumable)
		return false;
	if (!SelectedItem->UseItemCheck())
		return false;

	CharacterInvnetoryComponent->UseItem(SelectedItem);
	if (SelectedItem->ItemInfo.GetItemCount() <= 0)
	{
		RemoveQuickSlot(SelectedItem);
		SetSelectItem(nullptr);
	}
	OnEquipUpdated.Broadcast();
	return true;
}

UItemObject* UEquipmentComponent::FindEquipItemObject(FGameplayTag Tag)
{
	return WeaponMap[Tag];
}

void UEquipmentComponent::OnEquip(FGameplayTag Tag, UItemObject* Object)
{
	if (Object->ItemInfo.ItemCategory != EItemType::E_Item_Equip)
		return;

	if (WeaponMap.Contains(Tag))
		UnEquip(Tag);

	CharacterInvnetoryComponent->RemoveItem(Object);
	WeaponMap.Add(Tag, Object); 
	WeaponChangeUp();

	OnEquipUpdated.Broadcast();
}

void UEquipmentComponent::UnEquip(FGameplayTag Tag)
{
	if(WeaponMap.Contains(Tag))
		CharacterInvnetoryComponent->AddItem(WeaponMap[Tag]);
	WeaponMap.Remove(Tag);
	WeaponChangeUp();

	OnEquipUpdated.Broadcast();
}

void UEquipmentComponent::WeaponChangeUp()
{
	if (WeaponMap.IsEmpty())
	{
		SetSelectItem(nullptr);
		return;
	}
	if (SelectedItem == nullptr)
	{
		SetSelectItem(WeaponMap.begin().Value());
		return;
	}

	auto Inter = WeaponMap.begin();
	while (Inter != WeaponMap.end())
	{
		if (Inter.Key() == SelectedItem->ItemInfo.GetItemTag())
		{
			++Inter;
			if (Inter == WeaponMap.end())
			{
				SetSelectItem(WeaponMap.begin().Value());
				OnWeaponUp.Broadcast();
				return;
			}
			else
			{
				SetSelectItem(Inter.Value());
				OnWeaponUp.Broadcast();
				return;
			}
			break;
		}
		++Inter;
	}
	SetSelectItem(WeaponMap.begin().Value());
}

void UEquipmentComponent::WeaponChangeDown()
{
	if (WeaponMap.IsEmpty())
	{
		SetSelectItem(nullptr);
		return;
	}
	if (SelectedItem == nullptr)
	{
		SetSelectItem(WeaponMap.begin().Value());
		return;
	}

	auto Inter = WeaponMap.begin();
	auto BackInter = WeaponMap.begin();
	while (Inter != WeaponMap.end())
	{
		++Inter;
		if (Inter.Key() == SelectedItem->ItemInfo.GetItemTag())
		{
			break;
		}
		if (Inter == WeaponMap.end())
			break;
		++BackInter;
	}
	OnWeaponDown.Broadcast();
	SetSelectItem(BackInter.Value());
}

UAbilitySetData* UEquipmentComponent::GetSelectedAbilityData()
{
	return SelectedAbilityActionData;
}

bool UEquipmentComponent::ItemUpgrade(UItemObject* Item, int32 SelectedIndex)
{
	if (Item == nullptr)
		return false;
	int32 EffectLevel = Item->ItemEffects[SelectedIndex].EffectLevel;
	if (EffectLevel > Item->ItemEffects[SelectedIndex].UpgradeInfo.Num())
		return false;
	EffectLevel--;
	for (auto Keys : Item->ItemEffects[SelectedIndex].UpgradeInfo[EffectLevel].ItemData)
	{
		UItemObject* FindItem = CharacterInvnetoryComponent->FindItem(Keys.Key);
		if (CharacterInvnetoryComponent->ItemCheck(FindItem, Keys.Value))
			continue;
		return false;
	}
	for (auto Keys : Item->ItemEffects[SelectedIndex].UpgradeInfo[EffectLevel].ItemData)
	{
		UItemObject* FindItem = CharacterInvnetoryComponent->FindItem(Keys.Key);
		CharacterInvnetoryComponent->UseItemValue(FindItem, Keys.Value);
	}
	Item->ItemEffects[SelectedIndex].EffectLevel++;
	RemoveAbility(SelectedItem);
	AddAbilityData(SelectedItem);

	return true;
}

void UEquipmentComponent::AddAbilityData(UItemObject* Item)
{
	if (Item == nullptr)
		return;
	if (Item->GetAbilityData() == nullptr)
		return;

	SelectedAbilityActionData = Item->GetAbilityData();
	FItemAbilityData& NewHandle = AbilityHandles.AddDefaulted_GetRef();
	NewHandle.Item = Item;
	SelectedAbilityActionData->GiveToAbilitySystem(AbilitySystemComponent, &NewHandle.GrantedHandles);

	if (UEquipmentObject* EquipItem = Cast<UEquipmentObject>(Item))
	{
		if (IsValid(EquipItem->AttributeSet))
		{
			if (EquipItem->EquipAttribute == nullptr)
			{
				EquipItem->EquipAttribute = NewObject<UAttributeSet>(AbilitySystemComponent->GetOwner(), EquipItem->AttributeSet);
				WeaponSlotUpdateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Cast<UWeaponAttributeSet>(EquipItem->EquipAttribute)->GetCurrentMagazineAttribute()).AddUObject(this, &UEquipmentComponent::UpdateWeaponSlot);
			}
			SelectAttribute = EquipItem->EquipAttribute;
			AbilitySystemComponent->AddAttributeSetSubobject(SelectAttribute);
		}
	}

	for (int32 EffectIndex = 0; EffectIndex < Item->ItemEffects.Num(); ++EffectIndex)
	{
		const FAbilitySetData_GamePlayEffectUpgrade& ItemEffect = Item->ItemEffects[EffectIndex];
		if (!IsValid(ItemEffect.GameplayEffect))
			continue;
		const UGameplayEffect* GameplayEffect = ItemEffect.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle GameplayEffectHandle = AbilitySystemComponent->ApplyGameplayEffectToSelf(GameplayEffect, ItemEffect.EffectLevel, AbilitySystemComponent->MakeEffectContext());
		NewHandle.GrantedHandles.AddGameplayEffectHandle(GameplayEffectHandle);
	}
}

void UEquipmentComponent::RemoveAbility(UItemObject* Item)
{
	if (Item == nullptr)
		return;

	for (auto It = AbilityHandles.CreateIterator(); It; It++)
	{
		FItemAbilityData& Entry = *It;
		if (Entry.Item == Item)
		{
			if (AbilitySystemComponent)
			{
				Entry.GrantedHandles.TakeFromAbilitySystem(AbilitySystemComponent);
			}
		}
		It.RemoveCurrent();
	}
}

void UEquipmentComponent::UpdateWeaponSlot(const FOnAttributeChangeData& Data)
{
	WeaponSlotUpdate.Broadcast();
}
