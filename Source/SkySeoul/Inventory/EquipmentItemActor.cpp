// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/EquipmentItemActor.h"
#include "Inventory/EquipmentObject.h"
#include "Components/ArrowComponent.h"

AEquipmentItemActor::AEquipmentItemActor()
{
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	RootComponent = SkeletalMeshComponent;
	StaticMesh->SetupAttachment(SkeletalMeshComponent);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StartPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("StartPoint"));
	StartPoint->SetupAttachment(SkeletalMeshComponent);
	EndPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("EndPoint"));
	EndPoint->SetupAttachment(SkeletalMeshComponent);
}

void AEquipmentItemActor::BeginPlay()
{
	Super::BeginPlay();
}