// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Character/Abilities/AbilitySetData.h"
#include "GameplayEffectTypes.h"
#include "EquipmentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEquipUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSelectUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponUp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponDown);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponSlotUpdated);


class UItemObject;
class ASkySeoulCharacter;
class UInventoryComponent;
class AEquipmentItemActor;

USTRUCT(BlueprintType)
struct FItemAbilityData
{
	GENERATED_BODY()
public:

	UItemObject* Item;
	FAbilitySetData_GrantedHandles GrantedHandles;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SKYSEOUL_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UEquipmentComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TMap<FGameplayTag, UItemObject*> WeaponMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TArray<UItemObject*> QuickSlotItem;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UInventoryComponent* CharacterInvnetoryComponent;

	UPROPERTY(BlueprintAssignable)
	FOnEquipUpdated OnEquipUpdated;

	UPROPERTY(BlueprintAssignable)
	FOnSelectUpdated OnSelectUpdated;

	UPROPERTY(BlueprintAssignable)
	FOnWeaponUp OnWeaponUp;

	UPROPERTY(BlueprintAssignable)
	FOnWeaponDown OnWeaponDown;


protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	TObjectPtr<class UCharacterAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<class UCharacterAttributeSetBase> AttributeSetBase;
	TArray<FItemAbilityData> AbilityHandles;
	UAttributeSet* SelectAttribute;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Action")
	UAbilitySetData* SelectedAbilityActionData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	UItemObject* SelectedItem;


	UPROPERTY(EditDefaultsOnly)
	ASkySeoulCharacter* OwnerCharacter;

	UPROPERTY(EditDefaultsOnly)
	AActor* EquipActor;

	FDelegateHandle WeaponSlotUpdateHandle;

	UPROPERTY(BlueprintAssignable)
	FOnWeaponSlotUpdated WeaponSlotUpdate;

	void AddAbilityData(UItemObject* Item);
	void RemoveAbility(UItemObject* Item);

	void UpdateWeaponSlot(const FOnAttributeChangeData& Data);

public:

	AActor* GetEquipActor() { return EquipActor; };

	void SpawnItem();

	void QuickSlotInitialize();

	UFUNCTION(BlueprintCallable)
	void SetSelectItem(UItemObject* Item);

	UFUNCTION(BlueprintCallable)
	UItemObject* GetSelectedItem() const { return SelectedItem; } ;


	UFUNCTION(BlueprintCallable)
	void SetQuickSlot(int32 Index, UItemObject* Object);

	UFUNCTION(BlueprintCallable)
	void RemoveQuickSlot(UItemObject* Object);

	UFUNCTION(BlueprintCallable)
	bool UseSelectedItem();

	UFUNCTION(BlueprintCallable)
	UItemObject* FindEquipItemObject(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable)
	void OnEquip(FGameplayTag Tag, UItemObject* Object);

	UFUNCTION(BlueprintCallable)
	void UnEquip(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable)
	void WeaponChangeUp();

	UFUNCTION(BlueprintCallable)
	void WeaponChangeDown();

	UFUNCTION(BlueprintCallable)
	UAbilitySetData* GetSelectedAbilityData();

	UFUNCTION(BlueprintCallable)
	bool ItemUpgrade(UItemObject* Item, int32 SelectedIndex);
};