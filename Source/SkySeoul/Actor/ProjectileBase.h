// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/ActorBase.h"
#include "ProjectileBase.generated.h"

class UPrimitiveComponent;
class UProjectileMovementComponent;
class ACharacterBase;
/**
 * 
 */
UCLASS()
class SKYSEOUL_API AProjectileBase : public AActor
{
	GENERATED_BODY()

	// ----------------------------------------------------------------------- //
		// Funcs

public:
	AProjectileBase();

	UPROPERTY(EditAnywhere, Category = "Projectile Option")
	float ProjectileMaxSpeed;
	UPROPERTY(EditAnywhere, Category = "Projectile Option")
	float ProjectileInitialSpeed;
protected:


private:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent,
		AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	void GetTargetData(AActor* Actor);

	// ----------------------------------------------------------------------- //
		// Value

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* ActorMesh;

protected:


private:
	UPROPERTY(EditAnywhere, Category = "Projectile Option")
	UProjectileMovementComponent* PMC;

	ACharacterBase* HitTarget;
};
