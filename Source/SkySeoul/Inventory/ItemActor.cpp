// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/ItemActor.h"
#include "Inventory/ItemObject.h"
#include "Components/WidgetComponent.h"
#include "Character/SkySeoulCharacter.h"
#include "Inventory/InventoryComponent.h"

// Sets default values
AItemActor::AItemActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));
	WidgetComponent->SetupAttachment(StaticMesh);
}

// Called when the game starts or when spawned
void AItemActor::BeginPlay()
{
	Super::BeginPlay();
	if (ItemObject)
		Iteminitialization(ItemObject);
}

// Called every frame
void AItemActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AItemActor::Iteminitialization(UItemObject* item)
{
	if (item == nullptr)
		return;
	ItemObject = item;
	StaticMesh->SetStaticMesh(ItemObject->ItemInfo.StaticMesh);
	StaticMesh->SetCollisionProfileName(TEXT("ItemActor"));
	SetWidgetText();
}

void AItemActor::AddInventory(AActor* Caller)
{
	ASkySeoulCharacter* ItemCharacter = Cast<ASkySeoulCharacter>(Caller);
	if (ItemCharacter)
	{
		UInventoryComponent* Inventory = ItemCharacter->InventoryComponent;
		Inventory->AddItem(ItemObject);
		Destroy();
	}
}

void AItemActor::OnInteract_Implementation(AActor* Caller)
{
	AddInventory(Caller);
}

void AItemActor::StartFocus_Implementation()
{
	WidgetComponent->SetVisibility(true);
}

void AItemActor::EndFocus_Implementation()
{
	WidgetComponent->SetVisibility(false);
}

