// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

class UItemObject;
class AItemActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SKYSEOUL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UInventoryComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxInventorySize = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced)
	TArray<UItemObject*> ItemInventory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced)
	TArray<UItemObject*> DefalutItemInventory;

	UPROPERTY(BlueprintAssignable)
	FOnInventoryUpdated OnInventoryUpdated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AItemActor> DropItem;
public:
	virtual bool DefaultAddItem(UItemObject* Item);

	UFUNCTION(BlueprintCallable)
	virtual bool AddItem(UItemObject* Item);
	UFUNCTION(BlueprintCallable)
	virtual bool RemoveItem(UItemObject* Item);

	UFUNCTION(BlueprintCallable)
	UItemObject* FindItem(FGameplayTag Tag) const ;

	UFUNCTION(BlueprintCallable)
	void UseItem(UItemObject* Item);
	void UseItem(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable)
	int32 UseItemValue(UItemObject* Item, int32 Value);

	UFUNCTION(BlueprintCallable)
	bool ItemCheck(UItemObject* Item, int32 Value);

	UFUNCTION(BlueprintCallable)
	void ItemDrop();
};
