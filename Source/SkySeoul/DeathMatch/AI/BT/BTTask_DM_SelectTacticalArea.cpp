#include "BTTask_DM_SelectTacticalArea.h"

#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"
#include "DeathMatch/Navigation/DMTacticalAreaSubsystem.h"
#include "DeathMatch/Navigation/DMTacticalAreaVolume.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

UBTTask_DM_SelectTacticalArea::UBTTask_DM_SelectTacticalArea()
{
	NodeName = TEXT("DM Select Tactical Area");
	bCreateNodeInstance = true;
	SelectedAreaKey.SelectedKeyName = DMBlackboardKeys::SelectedTacticalArea;
	SelectedAreaKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_SelectTacticalArea, SelectedAreaKey), ADMTacticalAreaVolume::StaticClass());
}

float UBTTask_DM_SelectTacticalArea::CalculateDangerFit(float Danger, float Preferred, float Tolerance)
{
	return FMath::Clamp(1.f - FMath::Abs(FMath::Clamp(Danger, 0.f, 10.f) - FMath::Clamp(Preferred, 0.f, 10.f))
		/ FMath::Max(Tolerance, UE_SMALL_NUMBER), 0.f, 1.f);
}

float UBTTask_DM_SelectTacticalArea::CalculateSelectionWeight(float Danger, float Preferred, float Tolerance,
	int32 FriendlyCount, int32 IncomingCount, float DistanceCm, bool bRecent, bool bCurrent,
	float BaseWeight, float DangerWeight, float FriendlyWeight, float IncomingWeight, float DistancePenalty,
	float RecentPenalty, float CurrentPenalty, float MinimumWeight)
{
	const float Raw = FMath::Max(0.f, BaseWeight) + CalculateDangerFit(Danger, Preferred, Tolerance) * FMath::Max(0.f, DangerWeight)
		- FMath::Max(0, FriendlyCount) * FMath::Max(0.f, FriendlyWeight)
		- FMath::Max(0, IncomingCount) * FMath::Max(0.f, IncomingWeight)
		- FMath::Max(0.f, DistanceCm) / 1000.f * FMath::Max(0.f, DistancePenalty)
		- (bRecent ? FMath::Max(0.f, RecentPenalty) : 0.f)
		- (bCurrent ? FMath::Max(0.f, CurrentPenalty) : 0.f);
	return FMath::Max(FMath::Max(UE_SMALL_NUMBER, MinimumWeight), Raw);
}

int32 UBTTask_DM_SelectTacticalArea::SelectWeightedIndex(const TArray<float>& Weights, FRandomStream& Stream)
{
	float Total = 0.f;
	for (float Weight : Weights) Total += FMath::IsFinite(Weight) ? FMath::Max(0.f, Weight) : 0.f;
	if (Total <= UE_SMALL_NUMBER) return INDEX_NONE;
	float Roll = Stream.FRand() * Total;
	int32 LastPositive = INDEX_NONE;
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		const float Weight = FMath::IsFinite(Weights[Index]) ? FMath::Max(0.f, Weights[Index]) : 0.f;
		if (Weight <= 0.f) continue;
		LastPositive = Index;
		Roll -= Weight;
		if (Roll < 0.f) return Index;
	}
	return LastPositive;
}

float UBTTask_DM_SelectTacticalArea::CalculatePersonalDangerBias(int32 StableSeed, float BiasRange)
{
	const float Range = FMath::Clamp(FMath::Abs(BiasRange), 0.f, 10.f);
	FRandomStream Stream(StableSeed ^ 0x5F3759DF);
	return Range > 0.f ? Stream.FRandRange(-Range, Range) : 0.f;
}

EBTNodeResult::Type UBTTask_DM_SelectTacticalArea::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	AAIController* AI = OwnerComp.GetAIOwner();
	APawn* Pawn = AI ? AI->GetPawn() : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	UWorld* World = OwnerComp.GetWorld();
	UDMTacticalAreaSubsystem* Areas = World ? World->GetSubsystem<UDMTacticalAreaSubsystem>() : nullptr;
	if (!AI || !Pawn || !BB || !Areas) return EBTNodeResult::Failed;
	const uint8 Team = DeathMatchTeamUtils::GetTeamIdFromController(AI).GetId();
	if (Team == 255) return EBTNodeResult::Failed;
	TArray<ADMTacticalAreaVolume*> Volumes;
	Areas->GetRegisteredVolumes(Volumes, true);
	ADMTacticalAreaVolume* Current = Areas->FindVolumeAtLocation(Pawn->GetActorLocation(), true);
	ADMTacticalAreaVolume* Recent = Areas->GetRecentRoamArea(Pawn);
	const float EffectivePreferredDanger = FMath::Clamp(PreferredDanger
		+ CalculatePersonalDangerBias(AI->GetUniqueID(), PersonalDangerBiasRange), 0.f, 10.f);
	TArray<float> Weights;
	float Total = 0.f;
	for (ADMTacticalAreaVolume* Volume : Volumes)
	{
		const float Weight = Volume && !Areas->IsRoamAreaOnCooldown(Pawn, Volume, World->GetTimeSeconds())
			? CalculateSelectionWeight(Volume->GetTeamDanger(Team), EffectivePreferredDanger, DangerTolerance,
			Volume->GetFriendlyCount(Team), Volume->GetIncomingAICount(Team), FVector::Dist2D(Pawn->GetActorLocation(), Volume->GetRepresentativeLocation()),
			Volume == Recent, Volume == Current, BaseExplorationWeight, DangerPreferenceWeight,
			FriendlyCrowdingWeight, IncomingAIWeight, DistanceWeight, RecentVolumePenalty, CurrentVolumePenalty, MinimumSelectionWeight) : 0.f;
		Weights.Add(Weight);
		Total += Weight;
	}
	if (Total <= UE_SMALL_NUMBER) return EBTNodeResult::Failed;
	float Roll = FMath::FRandRange(0.f, Total);
	ADMTacticalAreaVolume* Selected = nullptr;
	for (int32 Index = 0; Index < Volumes.Num(); ++Index)
	{
		Roll -= Weights[Index];
		if (Roll <= 0.f) { Selected = Volumes[Index]; break; }
	}
	if (!Selected) Selected = Volumes.Last();
	if (!Selected || !Areas->ReserveIncomingAI(Pawn, Team, Selected)) return EBTNodeResult::Failed;
	BB->SetValueAsObject(SelectedAreaKey.SelectedKeyName, Selected);
	return EBTNodeResult::Succeeded;
}
