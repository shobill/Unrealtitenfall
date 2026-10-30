#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "DeathMatch/Weapons/DMWeaponTypes.h"
#include "Character/Abilities/AbilitySetData.h"
#include "DMWeaponLoadoutComponent.generated.h"

class ACharacter;
class USkeletalMeshComponent;
class ADMWeaponActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDMWeaponAcquired, FGameplayTag, WeaponId, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDMActiveWeaponChanged, FGameplayTag, WeaponId, int32, SlotIndex);

/**
 * DeathMatch weapon acquire + wheel swap (no campaign inventory).
 * AcquireWeapon → CycleWeaponNext (WheelUp) / CycleWeaponPrev (WheelDown).
 */
UCLASS(ClassGroup = (DeathMatch), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UDMWeaponLoadoutComponent : public UActorComponent
{
    friend class FDMAdaptiveSafetyTest;
	GENERATED_BODY()

public:
	UDMWeaponLoadoutComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeathMatch|Weapon")
	TArray<FDMWeaponDefinition> StartingWeapons;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeathMatch|Weapon")
	bool bGrantStartingWeaponsOnBeginPlay = true;

	UPROPERTY(BlueprintAssignable, Category = "DeathMatch|Weapon")
	FOnDMWeaponAcquired OnWeaponAcquired;

	UPROPERTY(BlueprintAssignable, Category = "DeathMatch|Weapon")
	FOnDMActiveWeaponChanged OnActiveWeaponChanged;

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon")
	bool AcquireWeapon(const FDMWeaponDefinition& Definition);

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon")
	bool RemoveWeapon(FGameplayTag WeaponId);

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon")
	bool HasWeapon(FGameplayTag WeaponId) const;

	/** Mouse wheel up. */
	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon")
	bool CycleWeaponNext();

	/** Mouse wheel down. */
	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon")
	bool CycleWeaponPrev();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon")
	bool SelectWeaponByIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon")
	bool SelectWeaponById(FGameplayTag WeaponId);

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	int32 GetActiveWeaponIndex() const { return ActiveIndex; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	int32 GetOwnedWeaponCount() const { return OwnedWeapons.Num(); }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	FGameplayTag GetActiveWeaponId() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	ADMWeaponActor* GetActiveWeaponActor() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	FTransform GetActiveMuzzleTransform() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	bool GetOwnedWeaponDefinition(int32 Index, FDMWeaponDefinition& OutDef) const;

	/** Call after ASC is ready (e.g. PossessedBy) so active weapon AbilitySet can grant. */
	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon")
	void RefreshActiveWeaponAbilities();

protected:
    friend class FDMWeaponFireIntegrationTest;
    friend class FDMCombatDebugObservationTest;
	void GrantStartingWeapons();
	void ClearAllWeapons();
	void ApplyActiveWeaponVisuals();
	void ClearActiveWeaponAbilities();
	USkeletalMeshComponent* GetOwnerMesh() const;
	int32 FindOwnedIndex(FGameplayTag WeaponId) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Weapon")
	TArray<FDMWeaponRuntimeSlot> OwnedWeapons;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Weapon")
	int32 ActiveIndex = INDEX_NONE;

	UPROPERTY()
	FAbilitySetData_GrantedHandles ActiveWeaponAbilityHandles;

	bool bHasActiveWeaponAbilityHandles = false;
};
