#include "BTTask_DM_SelectRoamLocation.h"
#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/Navigation/DMTacticalAreaSubsystem.h"
#include "DeathMatch/Navigation/DMTacticalAreaVolume.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "AIController.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

UBTTask_DM_SelectRoamLocation::UBTTask_DM_SelectRoamLocation()
{
	NodeName = TEXT("DM Select Roam Location");
	SelectedAreaKey.SelectedKeyName = DMBlackboardKeys::SelectedTacticalArea;
	SelectedAreaKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_SelectRoamLocation, SelectedAreaKey), ADMTacticalAreaVolume::StaticClass());
	MoveGoalKey.SelectedKeyName = DMBlackboardKeys::MoveGoal;
	MoveGoalKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_SelectRoamLocation, MoveGoalKey));
}

EBTNodeResult::Type UBTTask_DM_SelectRoamLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	APawn* Pawn = AI ? AI->GetPawn() : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	ADMTacticalAreaVolume* Area = BB ? Cast<ADMTacticalAreaVolume>(BB->GetValueAsObject(SelectedAreaKey.SelectedKeyName)) : nullptr;
	UNavigationSystemV1* Nav = OwnerComp.GetWorld() ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerComp.GetWorld()) : nullptr;
	if (!Pawn || !BB || !Area || !Area->IsAreaEnabled() || !Nav) return EBTNodeResult::Failed;
	FNavLocation Origin;
	if (!Nav->ProjectPointToNavigation(Area->GetRepresentativeLocation(), Origin, Area->GetWorldBoundsExtent())) return EBTNodeResult::Failed;
	for (int32 Attempt = 0; Attempt < FMath::Max(1, MaxAttempts); ++Attempt)
	{
		FNavLocation Point;
		if (!Nav->GetRandomReachablePointInRadius(Origin.Location, Area->GetSearchRadius(), Point) || !Area->IsPointInsideVolume(Point.Location)) continue;
		UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(OwnerComp.GetWorld(), Pawn->GetActorLocation(), Point.Location, Pawn);
		if (Path && Path->IsValid() && !Path->IsPartial())
		{
			BB->SetValueAsVector(MoveGoalKey.SelectedKeyName, Point.Location);
			return EBTNodeResult::Succeeded;
		}
	}
	if (UDMTacticalAreaSubsystem* Areas = OwnerComp.GetWorld()->GetSubsystem<UDMTacticalAreaSubsystem>())
	{
		Areas->RecordRoamAreaFailure(Pawn, Area, OwnerComp.GetWorld()->GetTimeSeconds(), FailureCooldown);
		Areas->ReleaseIncomingAI(Pawn);
	}
	BB->ClearValue(MoveGoalKey.SelectedKeyName);
	return EBTNodeResult::Failed;
}
