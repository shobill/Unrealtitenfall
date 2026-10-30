#include "BTTask_DM_SelectMoveLocation.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"

#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/Navigation/DMMovePoint.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"

UBTTask_DM_SelectMoveLocation::UBTTask_DM_SelectMoveLocation()
{
	NodeName = TEXT("DM Select Move Location");

	MoveGoalLocationKey.SelectedKeyName = DMBlackboardKeys::MoveGoalLocation;
	HasMoveGoalKey.SelectedKeyName = DMBlackboardKeys::HasMoveGoal;

	MoveGoalLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_SelectMoveLocation, MoveGoalLocationKey));
	HasMoveGoalKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_SelectMoveLocation, HasMoveGoalKey));
}

void UBTTask_DM_SelectMoveLocation::GatherMovePoints(UWorld* World, TArray<ADMMovePoint*>& OutPoints) const
{
	OutPoints.Reset();
	if (!World)
	{
		return;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, ADMMovePoint::StaticClass(), Found);

	for (AActor* Actor : Found)
	{
		ADMMovePoint* Point = Cast<ADMMovePoint>(Actor);
		if (!Point)
		{
			continue;
		}

		if (bOnlyEnabledPoints && !Point->bEnabled)
		{
			continue;
		}

		if (RequiredZoneId != 255 && Point->ZoneId != RequiredZoneId && Point->ZoneId != 255)
		{
			continue;
		}

		OutPoints.Add(Point);
	}
}

int32 UBTTask_DM_SelectMoveLocation::CountPawnsNearLocation(
	UWorld* World,
	const FVector& Location,
	float Radius,
	APawn* IgnorePawn) const
{
	if (!World || Radius <= 0.0f)
	{
		return 0;
	}

	const float RadiusSq = FMath::Square(Radius);
	int32 Count = 0;

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (!DeathMatchTeamUtils::IsCombatTargetCandidate(Pawn) || Pawn == IgnorePawn)
		{
			continue;
		}

		if (FVector::DistSquared(Pawn->GetActorLocation(), Location) <= RadiusSq)
		{
			++Count;
		}
	}

	return Count;
}

void UBTTask_DM_SelectMoveLocation::BuildZoneOccupancy(
	UWorld* World,
	const TArray<ADMMovePoint*>& AllPoints,
	TMap<uint8, int32>& OutOccupancy) const
{
	OutOccupancy.Reset();
	if (!World || AllPoints.Num() == 0)
	{
		return;
	}

	const float AttrRadiusSq = FMath::Square(ZoneAttributionRadius);

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Pawn = *It;
		if (!DeathMatchTeamUtils::IsCombatTargetCandidate(Pawn))
		{
			continue;
		}

		const FVector PawnLoc = Pawn->GetActorLocation();
		ADMMovePoint* Nearest = nullptr;
		float BestDistSq = AttrRadiusSq;

		for (ADMMovePoint* Point : AllPoints)
		{
			if (!Point || !Point->bEnabled || Point->ZoneId == 255)
			{
				continue;
			}

			const float DistSq = FVector::DistSquared(PawnLoc, Point->GetActorLocation());
			if (DistSq <= BestDistSq)
			{
				BestDistSq = DistSq;
				Nearest = Point;
			}
		}

		if (Nearest)
		{
			OutOccupancy.FindOrAdd(Nearest->ZoneId) += 1;
		}
	}
}

float UBTTask_DM_SelectMoveLocation::ComputePointWeight(
	UWorld* World,
	APawn* SelfPawn,
	ADMMovePoint* Point,
	const TMap<uint8, int32>& ZoneOccupancy) const
{
	if (!Point)
	{
		return 0.0f;
	}

	float Weight = FMath::Max(Point->BaseWeight, 0.01f);

	const int32 Nearby = CountPawnsNearLocation(
		World,
		Point->GetActorLocation(),
		NearbyPawnRadius,
		SelfPawn);
	Weight /= (1.0f + static_cast<float>(Nearby) * NearbyPawnPenaltyScale);

	if (Point->ZoneId != 255)
	{
		const int32 ZoneCount = ZoneOccupancy.FindRef(Point->ZoneId);
		Weight /= (1.0f + static_cast<float>(ZoneCount) * ZoneOccupancyPenaltyScale);
	}

	return FMath::Max(Weight, 0.01f);
}

ADMMovePoint* UBTTask_DM_SelectMoveLocation::PickWeightedRandom(
	const TArray<ADMMovePoint*>& Points,
	const TArray<float>& Weights)
{
	if (Points.Num() == 0 || Points.Num() != Weights.Num())
	{
		return nullptr;
	}

	float Total = 0.0f;
	for (const float W : Weights)
	{
		Total += W;
	}

	if (Total <= 0.0f)
	{
		return Points[FMath::RandRange(0, Points.Num() - 1)];
	}

	float Roll = FMath::FRandRange(0.0f, Total);
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		Roll -= Weights[Index];
		if (Roll <= 0.0f)
		{
			return Points[Index];
		}
	}

	return Points.Last();
}

ADMMovePoint* UBTTask_DM_SelectMoveLocation::SelectMovePoint(
	UBehaviorTreeComponent& OwnerComp,
	const TArray<ADMMovePoint*>& Candidates) const
{
	AAIController* AI = OwnerComp.GetAIOwner();
	APawn* SelfPawn = AI ? AI->GetPawn() : nullptr;
	UWorld* World = OwnerComp.GetWorld();
	if (!World || Candidates.Num() == 0)
	{
		return nullptr;
	}

	ADeathMatchRobotAIController* RobotAI = Cast<ADeathMatchRobotAIController>(AI);
	ADMMovePoint* LastPoint = RobotAI ? RobotAI->GetLastMovePoint() : nullptr;

	TMap<uint8, int32> ZoneOccupancy;
	BuildZoneOccupancy(World, Candidates, ZoneOccupancy);

	TArray<ADMMovePoint*> Filtered;
	TArray<float> Weights;
	Filtered.Reserve(Candidates.Num());
	Weights.Reserve(Candidates.Num());

	const FVector SelfLoc = SelfPawn ? SelfPawn->GetActorLocation() : FVector::ZeroVector;
	const float MinDistSq = FMath::Square(MinDistanceFromSelf);

	for (ADMMovePoint* Point : Candidates)
	{
		if (!Point)
		{
			continue;
		}

		if (bExcludeLastMovePoint && LastPoint && Point == LastPoint)
		{
			continue;
		}

		if (SelfPawn && MinDistanceFromSelf > 0.0f)
		{
			if (FVector::DistSquared(SelfLoc, Point->GetActorLocation()) < MinDistSq)
			{
				continue;
			}
		}

		Filtered.Add(Point);
		Weights.Add(ComputePointWeight(World, SelfPawn, Point, ZoneOccupancy));
	}

	// If filters removed everything, relax last/min-distance and weight all candidates.
	if (Filtered.Num() == 0)
	{
		for (ADMMovePoint* Point : Candidates)
		{
			if (!Point)
			{
				continue;
			}
			Filtered.Add(Point);
			Weights.Add(ComputePointWeight(World, SelfPawn, Point, ZoneOccupancy));
		}
	}

	return PickWeightedRandom(Filtered, Weights);
}

EBTNodeResult::Type UBTTask_DM_SelectMoveLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB)
	{
		return EBTNodeResult::Failed;
	}

	TArray<ADMMovePoint*> Candidates;
	GatherMovePoints(OwnerComp.GetWorld(), Candidates);

	ADMMovePoint* Chosen = SelectMovePoint(OwnerComp, Candidates);
	if (!Chosen)
	{
		BB->SetValueAsBool(HasMoveGoalKey.SelectedKeyName, false);
		BB->ClearValue(MoveGoalLocationKey.SelectedKeyName);
		return EBTNodeResult::Failed;
	}

	if (ADeathMatchRobotAIController* RobotAI = Cast<ADeathMatchRobotAIController>(OwnerComp.GetAIOwner()))
	{
		RobotAI->SetLastMovePoint(Chosen);
	}

	BB->SetValueAsVector(MoveGoalLocationKey.SelectedKeyName, Chosen->GetMoveLocation());
	BB->SetValueAsBool(HasMoveGoalKey.SelectedKeyName, true);
	return EBTNodeResult::Succeeded;
}
