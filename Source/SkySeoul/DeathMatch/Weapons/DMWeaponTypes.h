#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DeathMatch/Weapons/DMWeaponActor.h"
#include "DMWeaponTypes.generated.h"

/** One grantable / swappable DM weapon slot (identity + spawn/attach). Abilities live on the weapon actor. */
USTRUCT(BlueprintType)
struct FDMWeaponDefinition
{
	GENERATED_BODY()

	/** Slot id (e.g. Item.EquipItem.Weapon1). Used for acquire / swap identity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FGameplayTag WeaponId;

	/** Visual actor BP — owns AbilitySetWhileActive + Muzzle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<ADMWeaponActor> WeaponActorClass;

	/** Attach to owner skeletal mesh bone/socket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName AttachSocketName = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FTransform AttachRelativeTransform = FTransform::Identity;

	bool IsValidDef() const
	{
		// Use Get() so this stays safe if includes ever go back to forward-declare.
		return WeaponId.IsValid() && WeaponActorClass.Get() != nullptr;
	}
};

USTRUCT(BlueprintType)
struct FDMWeaponRuntimeSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	FDMWeaponDefinition Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<ADMWeaponActor> SpawnedActor = nullptr;
};
