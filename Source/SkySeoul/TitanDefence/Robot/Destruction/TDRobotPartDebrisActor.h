#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TitanDefence/Robot/Destruction/TDRobotPartSeparationTypes.h"
#include "TDRobotPartDebrisActor.generated.h"

class UPhysicalMaterial;
class UStaticMesh;
class UStaticMeshComponent;

/** Non-interactive physics representation of one detached robot part. */
UCLASS(BlueprintType, NotBlueprintable)
class SKYSEOUL_API ATDRobotPartDebrisActor : public AActor
{
	GENERATED_BODY()

public:
	ATDRobotPartDebrisActor();

	/** Configures the mesh, collision and mass-aware separation impulse. */
	bool InitializeDebris(
		UStaticMesh* InDebrisMesh,
		const FTDRobotPartSeparationSettings& SeparationSettings,
		AActor* SourceRobot,
		const FVector& HitLocation,
		const FVector& HitDirection,
		float ImpulseMultiplier);

	UStaticMeshComponent* GetDebrisMeshComponent() const { return DebrisMeshComponent; }

private:
	void IgnoreSourceRobot(AActor* SourceRobot);

	UPROPERTY(VisibleAnywhere, Category = "Debris")
	TObjectPtr<UStaticMeshComponent> DebrisMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> RuntimePhysicalMaterial;
};
