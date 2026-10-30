// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/PhoneBoothActor.h"
#include "Components/WidgetComponent.h"

APhoneBoothActor::APhoneBoothActor()
{
	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));
	WidgetComponent->SetupAttachment(ActorMesh);
}

void APhoneBoothActor::OnInteract_Implementation(AActor* Caller)
{
}

void APhoneBoothActor::StartFocus_Implementation()
{
}

void APhoneBoothActor::EndFocus_Implementation()
{
}
