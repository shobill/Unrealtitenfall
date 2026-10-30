#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AIModuleBase.generated.h"

/** Shared component base retained for Sense and serialized class compatibility. */
UCLASS(Abstract)
class SKYSEOUL_API UAIModuleBase : public UActorComponent
{
    GENERATED_BODY()
};
