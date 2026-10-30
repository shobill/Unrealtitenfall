// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Styling/SlateBrush.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "ItemObject.generated.h"

struct FSlateBrush;
class UAbilitySetData;
class UStaticMesh;
class UCharacterGameplayAbility;
class UInventoryComponent;
class UGameplayEffect;
/**
 * 
 */

UENUM(BlueprintType)
enum class EItemType : uint8
{
	E_None UMETA(DisplayName = "None"),
	E_Item_Equip UMETA(DisplayName = "Equip"),
	E_Item_Consumable UMETA(DisplayName = "Consumable"),
	E_Item_QuestItem UMETA(DisplayName = "QuestItem"),
	E_Item_Miscellaneous UMETA(DisplayName = "Miscellaneous"),
	E_Item_Material UMETA(DisplayName = "Material"),
};
USTRUCT(BlueprintType)
struct FUpgradeInfo
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TMap<FGameplayTag, int32> ItemData;
};

USTRUCT(BlueprintType)
struct FAbilitySetData_GamePlayEffectUpgrade
{
	GENERATED_BODY()
public:

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UGameplayEffect> GameplayEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float EffectLevel = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FGameplayTag EffectTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FCurveTableRowHandle CurveData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<FUpgradeInfo> UpgradeInfo;
};

USTRUCT(BlueprintType)
struct FItemInfoData : public FTableRowBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item")
	FString ItemName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item")
	FSlateBrush ItemImage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item")
	TArray<FString> ItemDescription;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item", meta = (ClampMin = 0.0))
	int32 ItemCount = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item")
	EItemType ItemCategory = EItemType::E_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item", meta = (ClampMin = 0.0))
	float DropChance = 0.0f;


	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item")
	UStaticMesh* StaticMesh = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item")
	FGameplayTag ItemTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item")
	FGameplayTag ItemStateTag;

	FString GetItemName() { return ItemName; };
	FGameplayTag GetItemTag() { return ItemTag; };
	int32 GetItemCount() { return ItemCount; };
};

UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class SKYSEOUL_API UItemObject : public UObject
{
	GENERATED_BODY()
	
public:

	UPROPERTY(Transient)
	class UWorld* World;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FItemInfoData ItemInfo;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	UInventoryComponent* Inventory;


	UPROPERTY(EditAnywhere, Category = "Spawn")
	TSubclassOf<AActor> ActorToSpawn;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	FName AttachSocket;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	FTransform AttachTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability")
	UAbilitySetData* ItemAbilityData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ability")
	TArray<FAbilitySetData_GamePlayEffectUpgrade> ItemEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Ability")
	int32 ItemLevel = 1;

	UAbilitySetData* GetAbilityData();

	void ItemUse();
	void ItemUse(int32 Value);
	bool UseItemCheck();
};
