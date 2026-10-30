// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/ActorBase.h"

AActorBase::AActorBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	ActorMesh = CreateDefaultSubobject<UStaticMeshComponent>
		(TEXT("Mesh"));
	ActorMesh->SetupAttachment(RootComponent);
}

void AActorBase::BeginPlay()
{
	Super::BeginPlay();

}

void AActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

