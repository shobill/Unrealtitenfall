#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TitanDefence/Robot/Destruction/TDRobotPartDataAsset.h"
#include "TitanDefence/Robot/Destruction/TDRobotPartSeparationTypes.h"
#include "TDRobotPartSeparationComponent.generated.h"

class ATDRobotPartDebrisActor;
class USkeletalMeshComponent;

/** Non-editable lookup data resolved from the DataAsset and component settings. */
struct FTDRobotResolvedPart
{
	const FTDRobotPartDefinition* Definition = nullptr;
	FTDRobotPartSeparationSettings SeparationSettings;
	TArray<FGameplayTag> ChildPartTags;
	int32 HierarchyDepth = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FTDRobotPartSeparationSucceeded,
	FGameplayTag, RequestedPart,
	AActor*, Requester,
	const TArray<FGameplayTag>&, SeparatedParts,
	const TArray<ATDRobotPartDebrisActor*>&, CreatedDebris);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FTDRobotPartSeparationFailed,
	FGameplayTag, RequestedPart,
	AActor*, Requester,
	ETDRobotPartSeparationResult, FailureReason);

/** Opt-in, data-driven part separation for Titan Defence robots. */
UCLASS(ClassGroup = (TitanDefence), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UTDRobotPartSeparationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDRobotPartSeparationComponent();

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Part Separation",
		meta = (
			DisplayName = "Destroy Robot Part",
			AdvancedDisplay = "Requester,HitLocation,HitDirection,ImpulseMultiplier",
			CPP_Default_Requester = "None",
			CPP_Default_HitLocation = "0,0,0",
			CPP_Default_HitDirection = "0,0,0",
			CPP_Default_ImpulseMultiplier = "1.0"))
	ETDRobotPartSeparationResult DestroyRobotPart(
		FGameplayTag PartTag,
		AActor* Requester,
		FVector HitLocation,
		FVector HitDirection,
		float ImpulseMultiplier);

	bool IsPartSeparated(FGameplayTag PartTag) const;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|Part Separation")
	FTDRobotPartSeparationSucceeded OnPartSeparationSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|Part Separation")
	FTDRobotPartSeparationFailed OnPartSeparationFailed;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Titan Defence|Robot|Part Separation")
	bool bSeparationEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Titan Defence|Robot|Part Separation")
	TObjectPtr<UTDRobotPartDataAsset> PartDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Titan Defence|Robot|Part Separation")
	FTDRobotPartSeparationSettings DefaultSeparationSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Titan Defence|Robot|Part Separation")
	TArray<FTDRobotPartSeparationOverride> PerPartSettingsOverrides;

private:
	ETDRobotPartSeparationResult BuildResolvedParts(
		FGameplayTag RequestedPart,
		bool bRequireSkeletalMesh);
	void CollectHierarchy(FGameplayTag RootPartTag, TArray<FGameplayTag>& OutParts) const;
	USkeletalMeshComponent* ResolveOwnerMesh() const;
	ETDRobotPartSeparationResult FailRequest(
		FGameplayTag RequestedPart,
		AActor* Requester,
		ETDRobotPartSeparationResult FailureReason,
		const FString& Detail,
		const FTDRobotPartDefinition* FailedDefinition = nullptr);

	TMap<FGameplayTag, FTDRobotResolvedPart> ResolvedParts;
	TSet<FGameplayTag> DestroyedParts;
	TSet<FGameplayTag> PartsBeingSeparated;
	TWeakObjectPtr<UTDRobotPartDataAsset> ResolvedDataAsset;
	bool bResolvedPartsValid = false;
	bool bSkeletalMeshValidated = false;
};
