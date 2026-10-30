// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActorBase.generated.h"

UCLASS()
class SKYSEOUL_API AActorBase : public AActor
{
	GENERATED_BODY()

	// ----------------------------------------------------------------------- //
		// Funcs

public:
	AActorBase();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:

	// ----------------------------------------------------------------------- //
		// Value

public:


protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* ActorMesh;

private:

};
