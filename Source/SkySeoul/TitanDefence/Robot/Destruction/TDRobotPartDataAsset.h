#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TDRobotPartDataAsset.generated.h"

class UStaticMesh;

/** Static, author-authored information for one Titan Defence robot part. */
USTRUCT(BlueprintType)
struct SKYSEOUL_API FTDRobotPartDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FGameplayTag PartTag;

	/** Empty for a hierarchy root. Descendants are resolved only through this relationship. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FGameplayTag ParentPartTag;

	/** Bone to hide and to use when evaluating the animated part transform. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FName BoneName = NAME_None;

	/** Static mesh spawned after this part is separated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	/** Part transform in the source skeletal mesh's component space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FTransform MeshSpaceTransform = FTransform::Identity;

	/** Reserved for a future part-damage component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part", meta = (ClampMin = "0.01"))
	float MaxHealth = 100.0f;
};

/** Static part definitions shared by Titan Defence robot Blueprints. */
UCLASS(BlueprintType)
class SKYSEOUL_API UTDRobotPartDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TArray<FTDRobotPartDefinition> Parts;
};
