#pragma once

#include "CoreMinimal.h"
#include "Controller/PlayerControllerBase.h"
#include "GameplayTagContainer.h"
#include "DeathMatchPlayerController.generated.h"

/**
 * DeathMatch player controller (approach A).
 * Wheel → weapon cycle. RightMouse hold → ADS zoom.
 * BP: IMC + Move/Look/Jump; InputConfig on pawn includes Wheel + Fire + RMB tags.
 */
UCLASS()
class SKYSEOUL_API ADeathMatchPlayerController : public APlayerControllerBase
{
	GENERATED_BODY()

public:
	ADeathMatchPlayerController();

protected:
	virtual void Input_AbilityInputTagPressed(FGameplayTag InputTag) override;
	virtual void Input_AbilityInputTagReleased(FGameplayTag InputTag) override;

	/** Defaults to InputTag.Mouse.WheelUp */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeathMatch|Weapon")
	FGameplayTag WeaponCycleNextInputTag;

	/** Defaults to InputTag.Mouse.WheelDown */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeathMatch|Weapon")
	FGameplayTag WeaponCyclePrevInputTag;

	/** Defaults to InputTag.Mouse.RightMouseClick — hold to ADS */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DeathMatch|Aim")
	FGameplayTag AimZoomInputTag;
};
