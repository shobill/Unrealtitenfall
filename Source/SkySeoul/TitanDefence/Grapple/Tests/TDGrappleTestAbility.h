#pragma once
#include "TitanDefence/Grapple/GAS/TDGameplayAbility_Grapple.h"
#include "TDGrappleTestAbility.generated.h"

/** 테스트에서만 카메라 대신 테스트 Actor의 정면으로 조준합니다. */
UCLASS(NotBlueprintable)
class UTDGrappleTestAbility : public UTDGameplayAbility_Grapple
{
	GENERATED_BODY()
protected:
	virtual bool GetAimRay(const FGameplayAbilityActorInfo* ActorInfo, FVector& Start, FVector& Direction) const override
	{
		if (!ActorInfo || !ActorInfo->AvatarActor.IsValid()) return false;
		Start = ActorInfo->AvatarActor->GetActorLocation();
		Direction = ActorInfo->AvatarActor->GetActorForwardVector();
		return true;
	}
};
