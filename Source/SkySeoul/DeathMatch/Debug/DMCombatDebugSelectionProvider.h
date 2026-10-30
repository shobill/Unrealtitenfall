#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DMCombatDebugSelectionProvider.generated.h"

/** Optional local provider used by combat debug rendering to choose one actor. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDMCombatDebugSelectionProvider : public UInterface
{
	GENERATED_BODY()
};

class SKYSEOUL_API IDMCombatDebugSelectionProvider
{
	GENERATED_BODY()

public:
	virtual AActor* GetCombatDebugSelectedActor() const = 0;
};
