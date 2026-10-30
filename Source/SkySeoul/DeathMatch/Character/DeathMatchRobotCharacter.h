#pragma once

#include "CoreMinimal.h"
#include "PartsCharacter/PartsCharacterBase.h"
#include "GameplayTagContainer.h"
#include "DeathMatchRobotCharacter.generated.h"

class UDMWeaponLoadoutComponent;
class UDMPerceptionStimuliSourceComponent;
class UDMAimTargetComponent;
class UDMAimZoomComponent;
class ANavigationData;

/**
 * DeathMatch robot pawn.
 * AimTarget (Body/Head) implements IAISightTargetInterface for multi-point sight.
 * AimZoom = ADS state for AnimBP + Fire gate (no camera zoom).
 */
UCLASS()
class SKYSEOUL_API ADeathMatchRobotCharacter : public APartsCharacterBase
{
	GENERATED_BODY()

public:
	ADeathMatchRobotCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PossessedBy(AController* NewController) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Ability")
	bool TryActivateDeathMatchAbility(FGameplayTag AbilityTag);

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Ability")
	bool TryActivateDeathMatchFire();

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	UDMWeaponLoadoutComponent* GetWeaponLoadout() const { return WeaponLoadout; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	UDMAimTargetComponent* GetAimTarget() const { return AimTarget; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	UDMAimZoomComponent* GetAimZoom() const { return AimZoom; }

	/** Re-reads the scaled collision capsule; safe to call after an intentional runtime capsule resize. */
	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Navigation")
	void SynchronizeNavAgentWithCapsule();

	/** Non-shipping, one-shot compatibility diagnostic. Possess/nav rebuild call this automatically. */
	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Navigation")
	void RecheckNavigationCompatibility(bool bForce = true);

	/** Removes dead robots from RVO and Pawn blocking, then restores the original response on respawn. */
	void SetDeathNavigationState(bool bDead);

protected:
	UFUNCTION()
	void HandleNavigationGenerationFinished(ANavigationData* NavData);
    void OnPartsDamageApplied(const FPartsHitDamageResult& Result, AController* EventInstigator, AActor* Causer, const FVector& HitLocation, FName Part);
	void EnsureBodyHealth();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Weapon")
	TObjectPtr<UDMWeaponLoadoutComponent> WeaponLoadout;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Aim")
	TObjectPtr<UDMAimTargetComponent> AimTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Aim")
	TObjectPtr<UDMAimZoomComponent> AimZoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Perception")
	TObjectPtr<UDMPerceptionStimuliSourceComponent> PerceptionStimuliSource;

	TWeakObjectPtr<ANavigationData> LastDiagnosedNavData;
	TEnumAsByte<ECollisionResponse> LivingPawnCollisionResponse = ECR_Block;
	bool bCapturedLivingPawnCollisionResponse = false;
};
