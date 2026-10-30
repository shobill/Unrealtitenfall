#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "DMTacticalNavRegionTypes.h"
#include "DMTacticalNavRegionVolume.generated.h"

class UDMTacticalEventProfile;

/** Optional map bounds and compact map-owned tuning for Tactical Nav Regions. */
UCLASS(Blueprintable, meta=(DisplayName="DeathMatch Tactical Nav Region Volume"))
class SKYSEOUL_API ADMTacticalNavRegionVolume : public AVolume
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Nav Region") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Nav Region") FDMTacticalNavRegionSettings RegionSettings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Nav Region|Events") TMap<EDMTacticalRegionEventType, TObjectPtr<UDMTacticalEventProfile>> EventProfiles;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Nav Region|Debug", meta=(ClampMin="0", ClampMax="4")) int32 DefaultDebugLevel = 0;
};
