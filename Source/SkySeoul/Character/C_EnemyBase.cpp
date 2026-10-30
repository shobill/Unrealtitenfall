// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/C_EnemyBase.h"
#include "BrainComponent.h"
#include "Controller/AIC_EnemyControllerBase.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "Inventory/InventoryComponent.h"

AC_EnemyBase::AC_EnemyBase(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	FinishPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	FinishPoint->SetupAttachment(RootComponent);
}

void AC_EnemyBase::EnemyDie()
{
	GameplayTagContainer.AddTag(DeadTag);
	GetAbilitySystemComponent()->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	AAIController* EnemyController = Cast<AAIController>(GetController());
	if (EnemyController)
	{
		EnemyController->GetBrainComponent()->StopLogic(FString("Die"));
	}
	UCapsuleComponent* EnemyCapsule = GetCapsuleComponent();
	if (EnemyCapsule)
	{
		EnemyCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	InventoryComponent->ItemDrop();
}

void AC_EnemyBase::EnemyControllerDestroy()
{
	AAIController* EnemyController = Cast<AAIController>(GetController());
	if (EnemyController)
	{
		EnemyController->Destroy();
	}
}
