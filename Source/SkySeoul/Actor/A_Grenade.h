// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/A_GrenadeBase.h"
#include "A_Grenade.generated.h"

/**
 * 
 */
UCLASS()
class SKYSEOUL_API AA_Grenade : public AA_GrenadeBase
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable)
	void Explosion();
};
