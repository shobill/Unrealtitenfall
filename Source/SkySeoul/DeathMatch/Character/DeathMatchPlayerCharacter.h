#pragma once

#include "CoreMinimal.h"
#include "Character/SkySeoulCharacter.h"
#include "DeathMatchPlayerCharacter.generated.h"

class UDMWeaponLoadoutComponent;
class UDMAimZoomComponent;
class UDMPerceptionStimuliSourceComponent;
class UDMAimTargetComponent;

/**
 * DeathMatch player pawn.
 * AimTarget (Body/Head) implements IAISightTargetInterface for multi-point sight.
 */
UCLASS()
class SKYSEOUL_API ADeathMatchPlayerCharacter : public ASkySeoulCharacter
{
	GENERATED_BODY()

public:
	ADeathMatchPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void PossessedBy(AController* NewController) override;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Weapon")
	UDMWeaponLoadoutComponent* GetWeaponLoadout() const { return WeaponLoadout; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	UDMAimZoomComponent* GetAimZoom() const { return AimZoom; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	UDMAimTargetComponent* GetAimTarget() const { return AimTarget; }

protected:
	void ApplyThirdPersonCameraRig();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Weapon")
	TObjectPtr<UDMWeaponLoadoutComponent> WeaponLoadout;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Aim")
	TObjectPtr<UDMAimZoomComponent> AimZoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Aim")
	TObjectPtr<UDMAimTargetComponent> AimTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Perception")
	TObjectPtr<UDMPerceptionStimuliSourceComponent> PerceptionStimuliSource;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeathMatch|Camera")
	FVector ThirdPersonCameraSocketOffset = FVector(0.0f, 60.0f, 55.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeathMatch|Camera", meta = (ClampMin = "50.0"))
	float ThirdPersonCameraArmLength = 320.0f;
};
