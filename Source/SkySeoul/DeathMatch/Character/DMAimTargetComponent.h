#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Perception/AISightTargetInterface.h"
#include "DMAimTargetComponent.generated.h"

class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EDMAimSensorSlot : uint8
{
	Body UMETA(DisplayName = "Body"),
	Head UMETA(DisplayName = "Head"),
};

/**
 * Aim / visibility sensors (Body + Head).
 * Implements IAISightTargetInterface so AI Sight uses Head→Body LOS
 * instead of only the capsule center (engine prefers Component interface).
 */
UCLASS(ClassGroup = (DeathMatch), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UDMAimTargetComponent
	: public USceneComponent
	, public IAISightTargetInterface
{
	GENERATED_BODY()

public:
	UDMAimTargetComponent();

    virtual void BeginPlay() override;
    virtual void OnRegister() override;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	FVector GetSensorLocation(EDMAimSensorSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	FVector GetBodyLocation() const { return GetSensorLocation(EDMAimSensorSlot::Body); }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	FVector GetHeadLocation() const { return GetSensorLocation(EDMAimSensorSlot::Head); }

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Aim")
	void CollectSensorLocations(TArray<FVector>& OutLocations) const;

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Aim")
	bool FindVisibleSensorFrom(
		const FVector& ObserverLocation,
		const AActor* IgnoreActor,
		FVector& OutSeenLocation,
		int32& OutLosChecks,
		float& OutSightStrength) const;

	/** Native-only diagnostic variant (not Blueprint-exposed). */
	bool FindVisibleSensorFrom(
		const FVector& ObserverLocation,
		const AActor* IgnoreActor,
		FVector& OutSeenLocation,
		int32& OutLosChecks,
		float& OutSightStrength,
		FString* OutFailReason) const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	FVector GetPreferredAimLocation() const;

	/** Shooter eye/muzzle → visible Body, then Head, then sanitized preferred. */
	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Aim")
	FVector GetCombatAimLocationFrom(const FVector& ObserverLocation, const AActor* IgnoreActor) const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	USceneComponent* GetBodySensor() const { return BodySensor; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Aim")
	USceneComponent* GetHeadSensor() const { return HeadSensor; }

	static UDMAimTargetComponent* FindOnActor(const AActor* Actor);

	static bool QueryCanBeSeenFrom(
		const AActor* TargetActor,
		const FVector& ObserverLocation,
		const AActor* IgnoreActor,
		FVector& OutSeenLocation,
		int32& OutLosChecks,
		float& OutSightStrength);

	// IAISightTargetInterface
	virtual bool CanBeSeenFrom(
		const FVector& ObserverLocation,
		FVector& OutSeenLocation,
		int32& NumberOfLoSChecksPerformed,
		float& OutSightStrength,
		const AActor* IgnoreActor = nullptr,
		const bool* bWasVisible = nullptr,
		int32* UserData = nullptr) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Aim")
	TObjectPtr<USceneComponent> BodySensor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Aim")
	TObjectPtr<USceneComponent> HeadSensor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Aim|Attach")
	FName BodyAttachBone = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Aim|Attach")
	FName HeadAttachBone = NAME_None;

	/** Only auto-snaps when Body/HeadAttachBone are set. Default stays on capsule (actor space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Aim|Attach")
	bool bAutoAttachToMeshBones = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Aim")
	TEnumAsByte<ECollisionChannel> VisibilityTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Aim")
	bool bFallbackToActorLocationIfSensorsBlocked = true;

	/** Reject Body/Head sensors below this height above actor location (fixes mesh-root/feet aims). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Aim|Sanity", meta = (ClampMin = "0.0"))
	float MinSensorHeightAboveActor = 35.0f;

	/** Reject sensors that drift too far sideways from actor (mesh -90 yaw side offset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Aim|Sanity", meta = (ClampMin = "0.0"))
	float MaxSensorPlanarOffset = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Aim|Sanity", meta = (ClampMin = "0.0"))
	float FallbackChestHeight = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Aim|Sanity", meta = (ClampMin = "0.0"))
	float FallbackHeadHeight = 88.0f;

protected:
	void TryAutoAttachSensors();
	USkeletalMeshComponent* ResolveOwnerMesh() const;
	bool IsSensorLocationSane(const FVector& SensorWorldLoc) const;
	FVector GetSanitizedActorChestLocation() const;
	FVector GetSanitizedActorHeadLocation() const;
	bool IsLocationVisibleFrom(
		const FVector& ObserverLocation,
		const FVector& TargetLocation,
		const AActor* IgnoreActor,
		int32& InOutLosChecks,
		FString* OutBlockerName = nullptr) const;
};
