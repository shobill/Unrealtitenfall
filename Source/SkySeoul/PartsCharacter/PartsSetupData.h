#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PartsCharacter/PartsTypes.h"
#include "PartsSetupData.generated.h"

/**
 * Extensible parts catalog. Add/remove rows without changing C++.
 * Assign meshes / sockets / ParentTag / destroy effects per row.
 */
UCLASS(BlueprintType)
class SKYSEOUL_API UPartsSetupData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TArray<FPartsDefinition> Parts;

	/** Builds the default DM set (Head + L/R arm chain). Meshes left empty for BP. */
	UFUNCTION(BlueprintCallable, Category = "Parts")
	static void FillDefaultDeathMatchParts(TArray<FPartsDefinition>& OutParts);
};
