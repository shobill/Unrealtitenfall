#pragma once
#include "Character/Abilities/AbilitySetData.h"
#include "TDGrappleAbilitySet.generated.h"

/** 기존 AbilitySetData 형식을 사용하는 기본 그래플링 능력 묶음입니다. */
UCLASS()
class SKYSEOUL_API UTDGrappleAbilitySet : public UAbilitySetData
{
	GENERATED_BODY()
public:
	UTDGrappleAbilitySet(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
