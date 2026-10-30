// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/ActorBase.h"
#include "A_GrenadeBase.generated.h"

class URadialForceComponent;
class UParticleSystem;
class USoundBase;

/**
 * 
 */
UCLASS()
class SKYSEOUL_API AA_GrenadeBase : public AActorBase
{
	GENERATED_BODY()

	// ----------------------------------------------------------------------- //
		// Funcs

public:
	AA_GrenadeBase();

	UFUNCTION(BlueprintCallable)
	void GetInRadiusActors(TArray<FHitResult>& HitResults);

protected:


private:


	// ----------------------------------------------------------------------- //
		// Value

public:
	UPROPERTY(EditDefaultsOnly, Category = "Components")
	URadialForceComponent* RadialForceComp;
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UParticleSystem* ExplosionEffect;
	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	USoundBase* ExplosionSound;

	UPROPERTY(EditAnywhere, Category = "Explode Setting")
	float Radius;
	UPROPERTY(EditAnywhere, Category = "Explode Setting")
	float ExplodeDamage;

protected:


private:


};
