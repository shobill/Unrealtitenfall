#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "DMTacticalGridTypes.h"
#include "DMTacticalAnalysisVolume.generated.h"

/** Optional highest-priority analysis bounds and per-map tactical grid tuning. */
UCLASS(Blueprintable, meta=(DisplayName="DeathMatch Tactical Analysis Volume (Deprecated)", DeprecationMessage="Use DMTacticalNavRegionVolume."))
class SKYSEOUL_API ADMTacticalAnalysisVolume : public AVolume
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid", meta=(ToolTip="When enabled, this volume takes priority over NavMeshBoundsVolume and supplies per-map build/runtime settings."))
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Grid")
    FDMTacticalGridSettings TacticalGridSettings;
};

