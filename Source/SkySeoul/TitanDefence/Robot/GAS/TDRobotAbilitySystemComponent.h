#pragma once

#include "CoreMinimal.h"
#include "Character/Abilities/CharacterAbilitySystemComponent.h"
#include "TDRobotAbilitySystemComponent.generated.h"

/** Ability-system extension point owned directly by a Titan Defence robot. */
UCLASS()
class SKYSEOUL_API UTDRobotAbilitySystemComponent : public UCharacterAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UTDRobotAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

