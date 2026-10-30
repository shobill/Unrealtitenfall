#include "BTTask_DM_FaceTarget.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"

#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

UBTTask_DM_FaceTarget::UBTTask_DM_FaceTarget()
{
	NodeName = TEXT("DM Face Target");
	BlackboardKey.SelectedKeyName = DMBlackboardKeys::TargetActor;
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_FaceTarget, BlackboardKey), AActor::StaticClass());

	TargetLocationKey.SelectedKeyName = DMBlackboardKeys::TargetLocation;
	TargetLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_FaceTarget, TargetLocationKey));
}

EBTNodeResult::Type UBTTask_DM_FaceTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!AI || !BB)
	{
		return EBTNodeResult::Failed;
	}

	APawn* Pawn = AI->GetPawn();
	if (!Pawn)
	{
		return EBTNodeResult::Failed;
	}

	FVector TargetLoc = FVector::ZeroVector;
	bool bHaveTarget = false;

	AActor* KnowledgeTarget = nullptr;
	if (const UDMCombatKnowledgeComponent* Knowledge = AI->FindComponentByClass<UDMCombatKnowledgeComponent>())
		KnowledgeTarget = Knowledge->GetCombatTarget();
	if (AActor* TargetActor = KnowledgeTarget ? KnowledgeTarget : Cast<AActor>(BB->GetValueAsObject(BlackboardKey.SelectedKeyName)))
	{
		TargetLoc = TargetActor->GetActorLocation();
		bHaveTarget = true;
	}
	else if (BB->IsVectorValueSet(TargetLocationKey.SelectedKeyName))
	{
		TargetLoc = BB->GetValueAsVector(TargetLocationKey.SelectedKeyName);
		bHaveTarget = true;
	}

	if (!bHaveTarget)
	{
		return EBTNodeResult::Failed;
	}

	const FVector PawnLoc = Pawn->GetActorLocation();
	FVector ToTarget = TargetLoc - PawnLoc;
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return EBTNodeResult::Succeeded;
	}

	const FRotator DesiredRot = ToTarget.Rotation();
	const FRotator CurrentRot = Pawn->GetActorRotation();
	const float YawDelta = FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, DesiredRot.Yaw);

	if (FMath::Abs(YawDelta) <= AcceptanceAngleDegrees)
	{
		return EBTNodeResult::Succeeded;
	}

	FRotator NewRot = CurrentRot;
	if (bInstantFace)
	{
		NewRot.Yaw = DesiredRot.Yaw;
	}
	else
	{
		UWorld* World = Pawn->GetWorld();
		const float DeltaSeconds = World ? World->GetDeltaSeconds() : 0.016f;
		const float Step = RotationSpeedDegrees * DeltaSeconds;
		NewRot.Yaw = FMath::FixedTurn(CurrentRot.Yaw, DesiredRot.Yaw, Step);
	}

	Pawn->SetActorRotation(NewRot);
	AI->SetControlRotation(NewRot);

	return EBTNodeResult::Succeeded;
}
