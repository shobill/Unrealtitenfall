#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DMWeaponActor.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UArrowComponent;
class USceneComponent;
class UAbilitySetData;

/**
 * DM weapon visual + owned AbilitySet.
 * Attach to character mesh; Muzzle = fire start.
 * AbilitySetWhileActive is granted to the owner ASC while this weapon is selected.
 */
UCLASS()
class SKYSEOUL_API ADMWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	ADMWeaponActor();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon")
	FTransform GetMuzzleTransform() const;

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon")
	FVector GetMuzzleLocation() const;

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon")
	FVector GetMuzzleForward() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	UAbilitySetData* GetAbilitySetWhileActive() const { return AbilitySetWhileActive; }

	/** Distance with full damage and no distance-based fire delay. */
	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	float GetEffectiveRange() const { return FMath::Max(1.0f, EffectiveRange); }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	float GetMaxFireRange() const { return FMath::Max(GetEffectiveRange(), MaxFireRange); }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	float GetFireProbabilityAtDistance(float Distance) const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	float GetDamageMultiplierAtDistance(float Distance) const;

	/**
	 * Current hip/ADS spread in degrees after recovery.
	 * Call before the shot; then NotifyShotFired after the shot.
	 */
	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon|Accuracy")
	float GetSpreadDegreesForShot(float WorldTimeSeconds, bool bIsAds);

	/** Apply bloom after a successful shot. */
	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon|Accuracy")
	void NotifyShotFired(float WorldTimeSeconds);

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Weapon|Accuracy")
	void ResetSpread();

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon|Accuracy")
	float GetCurrentSpreadDegrees() const { return CurrentSpreadDegrees; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USceneComponent> Root;

	/** Optional SK mesh (gun). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** Optional SM if not using SK. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponStaticMesh;

	/** Fire start. Place at barrel tip in BP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UArrowComponent> Muzzle;

	/**
	 * Abilities this weapon grants while equipped/active (Fire etc.).
	 * Set on the weapon BP — Loadout reads this on select / clears on swap or unequip.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Abilities")
	TObjectPtr<UAbilitySetData> AbilitySetWhileActive;

	/** Full damage and fire probability up to this distance (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistics", meta = (ClampMin = "1.0"))
	float EffectiveRange = 3000.0f;

	/** Zero preserves existing BP range; values below EffectiveRange are clamped up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistics", meta = (ClampMin = "0.0"))
	float MaxFireRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinFireProbabilityAtMaxRange = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinDamageMultiplierAtMaxRange = 0.5f;

	// --- Accuracy / 집탄률 ---

	/** Minimum spread (first shots / fully recovered). Degrees half-angle cone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0"))
	float BaseSpreadDegrees = 0.35f;

	/** Cap while spraying. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0"))
	float MaxSpreadDegrees = 5.5f;

	/** Extra degrees added per shot after ShotsBeforeBloom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0"))
	float SpreadPerShotDegrees = 0.55f;

	/** How fast spread returns toward Base while not firing (deg/sec). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0"))
	float SpreadRecoveryPerSecond = 7.0f;

	/** First N shots keep base grouping before bloom starts (N inclusive). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0"))
	int32 ShotsBeforeBloom = 3;

	/** If idle longer than this, consecutive-shot counter resets (deg grouping "fresh"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0"))
	float BurstResetSeconds = 0.4f;

	/** Multiply spread while ADS (AimZoom). 1 = no ADS benefit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AdsSpreadScale = 0.45f;

protected:
	void RecoverSpreadToTime(float WorldTimeSeconds);

	float CurrentSpreadDegrees = 0.0f;
	int32 ConsecutiveShots = 0;
	float LastShotTimeSeconds = -1000.0f;
	bool bSpreadInitialized = false;
};
