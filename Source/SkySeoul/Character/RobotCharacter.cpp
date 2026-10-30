// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/RobotCharacter.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "AbilitySystemBlueprintLibrary.h"

#include "SkySeoulCharacter.h"
#include "Controller/RobotAIController.h"

// Sets default values
ARobotCharacter::ARobotCharacter(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	InteractionWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidgetComponent"));
	InteractionWidgetComponent->SetupAttachment(GetMesh());
	InteractionWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidgetComponent->SetVisibility(false);

	ButtonWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ButtonWidgetComponent"));
	ButtonWidgetComponent->SetupAttachment(GetMesh());
	ButtonWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	ButtonWidgetComponent->SetVisibility(false);

	Tags.Add(TEXT("Player_Robot"));
}

// Called when the game starts or when spawned
void ARobotCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ARobotCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ARobotCharacter::HackingStart(AActor* Caller)
{
	GetWorld()->GetTimerManager().ClearTimer(HackingAbortTimer);
	if (HackingPercent >= 1.0f)
	{
		HackingEnd(Caller);
	}
	else
	{
		//FGameplayEventData Payload;
		//UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Caller, FGameplayTag::RequestGameplayTag(FName("Ability.Player.Robot.HackingStart")), Payload);
		OwnerCharacter = Cast<ACharacter>(Caller);
		ButtonWidgetComponent->SetVisibility(true);
		CurrentHackingPercent = FMath::Clamp(FMath::Lerp(CurrentHackingPercent, CurrentHackingPercent + 1.f, 1.f), 0.f, MaxHackingPercent);
		HackingPercent = FMath::Clamp(CurrentHackingPercent / MaxHackingPercent, 0.f, 1.0f);
		GetWorld()->GetTimerManager().SetTimer(HackingAbortTimer, this, &ARobotCharacter::HackingAbort, 1.0f, true);
	}
	HackingUpdate.Broadcast();
}

void ARobotCharacter::HackingAbort()
{
	if (OwnerCharacter != nullptr && IsHacking == true)
	{
		ASkySeoulCharacter* PlayerCharacter = Cast<ASkySeoulCharacter>(OwnerCharacter);
		if (PlayerCharacter)
		{
			PlayerCharacter->PlayerActionState = EPlayerActionState::E_None;
		}
	}
	IsHacking = false;
	CurrentHackingPercent = FMath::Clamp( FMath::Lerp(CurrentHackingPercent, CurrentHackingPercent - 10.f, 1.f),0.f,MaxHackingPercent);
	HackingPercent = FMath::Clamp(CurrentHackingPercent / MaxHackingPercent, 0.f, 1.0f);
	if (HackingPercent <= 0.f)
		GetWorld()->GetTimerManager().ClearTimer(HackingAbortTimer);

	HackingUpdate.Broadcast();
}

void ARobotCharacter::HackingEnd(AActor* Caller)
{
	ActionState = ERobotActionState::E_FollowCharacter;
	RobotState = ERobotState::E_PreemptiveAttack;
	InteractionWidgetComponent->SetVisibility(false);
	ASkySeoulCharacter* PlayerCharacter = Cast<ASkySeoulCharacter>(Caller);
	if (PlayerCharacter)
	{
		PlayerCharacter->PlayerActionState = EPlayerActionState::E_None;
		//FGameplayEventData Payload;
		//UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Caller, FGameplayTag::RequestGameplayTag(FName("Ability.Player.Robot.HackingEnd")), Payload);
		PlayerCharacter->AddRobot(this);
		OwnerCharacter = PlayerCharacter;
	}
	RemoveTag(FName("NotHacked"));
	GetWorld()->GetTimerManager().ClearTimer(HackingAbortTimer);
	ARobotAIController* RobotController = Cast< ARobotAIController>(GetController());
	if (RobotController)
	{
		RobotController->SetRobotGoalLocation(Caller);
	}
}

void ARobotCharacter::MoveAction(FVector GoalLocation)
{
	ARobotAIController* RobotController = Cast< ARobotAIController>(GetController());
	if (RobotController)
	{
		RobotController->SetRobotGoalLocation(GoalLocation);
	}
}

void ARobotCharacter::SetRobotActionState(ERobotActionState Value)
{
	ActionState = Value;
	RobotStateUpdate.Broadcast();
}

void ARobotCharacter::RobotExplosion(TArray<FHitResult>& HitResult)
{
	FVector StartLocation = GetActorLocation();
	FVector EndLocation = StartLocation;
	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(300.f);
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);
	GetWorld()->SweepMultiByChannel(HitResult,StartLocation,EndLocation,FQuat::Identity,ECollisionChannel::ECC_Pawn, CollisionShape, CollisionParams);
}

void ARobotCharacter::RobotDie()
{
	SetSelect(false);
	GetAbilitySystemComponent()->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));

	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	GetMesh()->SetSimulatePhysics(true);
	ActionState = ERobotActionState::E_Explosion;
	AAIController* RobotController = Cast<AAIController>(GetController());
	if (RobotController)
	{
		RobotController->GetBrainComponent()->StopLogic(FString("Die"));
	}
	UCapsuleComponent* RobotCapsule = GetCapsuleComponent();
	if (RobotCapsule)
	{
		RobotCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (OwnerCharacter)
	{
		Cast<ASkySeoulCharacter>(OwnerCharacter)->RemoveRobot(this);
	}
	InteractionWidgetComponent->SetVisibility(false);
	ButtonWidgetComponent->SetVisibility(false);
}

void ARobotCharacter::RobotControllerDestroy()
{
	AAIController* RobotController = Cast<AAIController>(GetController());
	if (RobotController)
	{
		RobotController->Destroy();
	}
}

void ARobotCharacter::RobotIsFree()
{
	SetSelect(false);
	ActionState = ERobotActionState::E_Idle;
	GetWorld()->GetTimerManager().SetTimer(HackingAbortTimer, this, &ARobotCharacter::HackingAbort, 1.0f, true);
	ARobotAIController* RobotController = Cast<ARobotAIController>(GetController());
	if (RobotController)
	{
		RobotController->SetOwnerCharacter(nullptr);
	}
}

void ARobotCharacter::SetSelect(bool Value)
{
	IsSelect = Value;
	PlayerSelectUpdate.Broadcast();
}

void ARobotCharacter::StateChange(ERobotState State)
{
	RobotState = State;
	RobotStateUpdate.Broadcast();
}

void ARobotCharacter::OnInteract_Implementation(AActor* Caller)
{
	ASkySeoulCharacter* PlayerCharacter = Cast<ASkySeoulCharacter>(Caller);
	if (PlayerCharacter)
	{
		if (PlayerCharacter->RobotCostCheck(RobotCost))
			return;
		if (ActionState == ERobotActionState::E_Idle)
		{
			PlayerCharacter->PlayerActionState = EPlayerActionState::E_RobotHacking;
			if (!IsHacking)
			{
				IsHacking = true;
				PlayerCharacter->GetAbilitySystemComponent()->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Player.RobotHacking")));
			}
			HackingStart(Caller);
		}
	}
}

void ARobotCharacter::StartFocus_Implementation()
{
	if(ActionState == ERobotActionState::E_Idle)
		InteractionWidgetComponent->SetVisibility(true);
}

void ARobotCharacter::EndFocus_Implementation()
{
	if (ActionState == ERobotActionState::E_Idle)
	{
		if (HackingPercent > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(HackingAbortTimer, this, &ARobotCharacter::HackingAbort, 1.0f, true);
		}
		InteractionWidgetComponent->SetVisibility(false);
	}
}
