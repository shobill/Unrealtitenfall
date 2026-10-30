#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "DMPerceptionStimuliSourceComponent.generated.h"

/**
 * UE5.6: bAutoRegisterAsSource is protected on the engine component.
 * Subclass sets auto-register + Sight so pawns are visible to AI Perception
 * even when BeginPlay RegisterForSense runs before the PerceptionSystem exists.
 */
UCLASS(ClassGroup = (DeathMatch), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UDMPerceptionStimuliSourceComponent : public UAIPerceptionStimuliSourceComponent
{
	GENERATED_BODY()

public:
	UDMPerceptionStimuliSourceComponent(const FObjectInitializer& ObjectInitializer);
};
