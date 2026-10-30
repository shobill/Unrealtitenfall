// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/CharacterBase.h"
#include "C_EnemyBase.generated.h"

/**
 * 
 */
UCLASS()
class SKYSEOUL_API AC_EnemyBase : public ACharacterBase
{
	GENERATED_BODY()

	// ----------------------------------------------------------------------- //
		// Funcs

public:
	AC_EnemyBase(const class FObjectInitializer& ObjectInitializer);

protected:


private:

	// ----------------------------------------------------------------------- //
		// Value

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USceneComponent* FinishPoint;

	UFUNCTION(BlueprintCallable)
	virtual void EnemyDie();
	UFUNCTION(BlueprintCallable)
	void EnemyControllerDestroy();

protected:


private:


};
