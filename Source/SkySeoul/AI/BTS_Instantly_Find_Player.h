// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTS_Instantly_Find_Player.generated.h"

/**
 * 
 */
UCLASS()
class SKYSEOUL_API UBTS_Instantly_Find_Player : public UBTService_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTS_Instantly_Find_Player();

protected:
	virtual void TickNode
	(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds
	) override;
};
