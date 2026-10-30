// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BTTask_RobotAttack.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AbilitySystemComponent.h"

#include "Controller/RobotAIController.h"
#include "Character/CharacterBase.h"

UBTTask_RobotAttack::UBTTask_RobotAttack()
{
	NodeName = TEXT("RobotAttack");
	bNotifyTick = true;
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_RobotAttack, BlackboardKey), AActor::StaticClass());
}

void UBTTask_RobotAttack::PostInitProperties()
{
	Super::PostInitProperties();

	PrecisionDot = FMath::Cos(FMath::DegreesToRadians(Precision));
}

void UBTTask_RobotAttack::PostLoad()
{
	Super::PostLoad();

	PrecisionDot = FMath::Cos(FMath::DegreesToRadians(Precision));
}

EBTNodeResult::Type UBTTask_RobotAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ARobotAIController* AIController = Cast<ARobotAIController>(OwnerComp.GetAIOwner());
	if (AIController == NULL || AIController->GetPawn() == NULL)
	{
		return EBTNodeResult::Failed;
	}
	EBTNodeResult::Type Result = EBTNodeResult::InProgress;
	return Result;
}
namespace
{
	FORCEINLINE_DEBUGGABLE FVector::FReal CalculateAngleDifferenceDot(const FVector& VectorA, const FVector& VectorB)
	{
		return (VectorA.IsNearlyZero() || VectorB.IsNearlyZero())
			? 1.f
			: VectorA.CosineAngle2D(VectorB);
	}
}

void UBTTask_RobotAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	ARobotAIController* AIController = Cast<ARobotAIController>(OwnerComp.GetAIOwner());
	if (AIController == NULL || AIController->GetPawn() == NULL)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
	}
	else
	{
		EBTNodeResult::Type Result = EBTNodeResult::Failed;
		APawn* Pawn = AIController->GetPawn();
		const FVector PawnLocation = Pawn->GetActorLocation();
		const UBlackboardComponent* MyBlackboard = OwnerComp.GetBlackboardComponent();
		FRotator TargetRotator;
		if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
		{
			UObject* KeyValue = MyBlackboard->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID());
			AActor* ActorValue = Cast<AActor>(KeyValue);

			if (ActorValue != NULL)
			{
				TargetRotator = (ActorValue->GetActorLocation() - PawnLocation).Rotation();
				TargetRotator.Pitch = 0.f;
				TargetRotator.Roll = 0.f;
				TargetRotator.Normalize();
				if (TargetRotator.Yaw < 0.0f)
					TargetRotator.Yaw += 360.0f;
				Pawn->SetActorRotation(FMath::RInterpConstantTo(Pawn->GetActorRotation(), TargetRotator, DeltaSeconds, 300.f));
			}
			else
			{
				FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			}
		}
		const FVector PawnDirection = Pawn->GetActorForwardVector();
		const FVector FocalPoint = TargetRotator.Vector();
		if (CalculateAngleDifferenceDot(PawnDirection, FocalPoint) >= PrecisionDot)
		{
			if (Pawn != nullptr)
			{
				UObject* KeyValue = MyBlackboard->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID());
				AActor* ActorValue = Cast<AActor>(KeyValue);
				if (ActorValue != NULL)
				{
					UAbilitySystemComponent* ASC = Cast<ACharacterBase>(Pawn)->GetAbilitySystemComponent();
					if (ASC)
					{
						FGameplayTagContainer TempGameplayTagContainer{ FGameplayTag::RequestGameplayTag(FName("Ability.Robot.Attack")) };
						ASC->TryActivateAbilitiesByTag(TempGameplayTagContainer);
						FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
					}
				}
			}
		}
		FinishLatentTask(OwnerComp, EBTNodeResult::InProgress);
	}

}
