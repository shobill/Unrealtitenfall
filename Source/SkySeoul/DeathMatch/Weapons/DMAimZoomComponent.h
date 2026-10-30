#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMAimZoomComponent.generated.h"

class UCameraComponent;
class USpringArmComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDMAimingChanged, bool, bIsAiming);

/**
 * DeathMatch ADS state: player (RMB + camera zoom) and AI (combat aim pose).
 * Stores and presents the requested aim state. Player input or the AI ActionIntent policy owns
 * the request; this component does not reinterpret AI intents.
 */
UCLASS(ClassGroup = (DeathMatch), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UDMAimZoomComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDMAimZoomComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Aim")
	void SetAiming(bool bNewAiming);

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	bool IsAiming() const { return bIsAiming; }

	/** 0 = hip, 1 = fully zoomed (for crosshair / AnimBP). */
	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	float GetAimAlpha() const { return AimAlpha; }

	/** Player / robot — first AimZoom on actor. */
	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	static UDMAimZoomComponent* FindOnActor(const AActor* Actor);

	UPROPERTY(BlueprintAssignable, Category = "DeathMatch|Aim")
	FOnDMAimingChanged OnAimingChanged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeathMatch|Aim", meta = (ClampMin = "1.0", ClampMax = "170.0"))
	float AimFOV = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeathMatch|Aim", meta = (ClampMin = "0.0"))
	float AimArmLength = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeathMatch|Aim", meta = (ClampMin = "0.1"))
	float ZoomInterpSpeed = 12.0f;

	/** If true, sets SkySeoulCharacter PlayerActionState to Zoom while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeathMatch|Aim")
	bool bSetPlayerActionZoomState = true;

	/** Player camera FOV/arm. Off for AI (state + AimAlpha only). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DeathMatch|Aim")
	bool bApplyCameraZoom = true;

protected:
	void CacheCameraDefaults();
	void ApplyZoom(float Alpha);
	UCameraComponent* GetFollowCamera() const;
	USpringArmComponent* GetCameraBoom() const;

	bool bIsAiming = false;
	float AimAlpha = 0.0f;

	float DefaultFOV = 90.0f;
	float DefaultArmLength = 400.0f;
	bool bCachedDefaults = false;
};
