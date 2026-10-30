#include "BTTask_DM_FindCombatPosition.h"

#include "DeathMatch/AI/BT/DMBlackboardKeys.h"
#include "DeathMatch/AI/Combat/DMCombatDebug.h"
#include "DeathMatch/AI/Combat/DMCombatPositionRegistry.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "DeathMatch/Navigation/DMTacticalAreaSubsystem.h"
#include "DeathMatch/Navigation/DMTacticalAreaVolume.h"
#include "DeathMatch/Weapons/DMWeaponActor.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

namespace
{
const TCHAR* PurposeName(EDMCombatPositionPurpose Purpose)
{
	switch (Purpose)
	{
	case EDMCombatPositionPurpose::Approach: return TEXT("Approach");
	case EDMCombatPositionPurpose::Hide: return TEXT("Hide");
	default: return TEXT("Attack");
	}
}

struct FDMCandidateWork
{
	FVector Location = FVector::ZeroVector;
	FVector FireOrigin = FVector::ZeroVector;
	float RangeFitness = 0.f;
	float NearbyFriendlies = 0.f;
	float PathLength = 0.f;
	float AreaDanger = 0.f;
	float AreaDangerIncrease = 0.f;
	TWeakObjectPtr<ADMTacticalAreaVolume> Area;
	float CheapScore = 0.f;
	bool bFocusLineClear = false;
	bool bRepresentativeConcealed = false;
};

bool IsExposedToThreat(UWorld* World, const APawn* Pawn, const FDMRememberedThreat* Threat,
	const FVector& CandidateOrigin, ECollisionChannel TraceChannel, float FallbackEyeHeight)
{
	if (!World || !Pawn || !Threat) return false;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DMCombatPositionExposure), false, Pawn);
	if (IsValid(Threat->Actor)) Params.AddIgnoredActor(Threat->Actor.Get());
	FVector ThreatOrigin = Threat->LastKnownLocation + FVector::UpVector * FMath::Max(0.f, FallbackEyeHeight);
	if (const APawn* ThreatPawn = Cast<APawn>(Threat->Actor.Get()))
	{
		ThreatOrigin = UDMCombatFireControlComponent::ProjectFireOriginOffset(
			ThreatPawn->GetActorLocation(), ThreatPawn->GetPawnViewLocation(), Threat->LastKnownLocation);
	}
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, ThreatOrigin, CandidateOrigin, TraceChannel, Params);
	return !bBlocked;
}
}

UBTTask_DM_FindCombatPosition::UBTTask_DM_FindCombatPosition()
{
	NodeName = TEXT("DM Find Combat Position");
	bCreateNodeInstance = true;
	MoveGoalKey.SelectedKeyName = DMBlackboardKeys::MoveGoal;
	MoveGoalKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_FindCombatPosition, MoveGoalKey));
	ShouldADSKey.SelectedKeyName = DMBlackboardKeys::ShouldADS;
	ShouldADSKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_FindCombatPosition, ShouldADSKey));
	FireWhileMovingKey.SelectedKeyName = DMBlackboardKeys::FireWhileMoving;
	FireWhileMovingKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_DM_FindCombatPosition, FireWhileMovingKey));
}

const UDMCombatPositionProfile* UBTTask_DM_FindCombatPosition::GetProfileForPurpose(EDMCombatPositionPurpose Purpose) const
{
	switch (Purpose)
	{
	case EDMCombatPositionPurpose::Approach: return ApproachPositionProfile;
	case EDMCombatPositionPurpose::Hide: return HidePositionProfile;
	default: return AttackPositionProfile;
	}
}

bool UBTTask_DM_FindCombatPosition::ShouldStartSearch(double Now, double LastTime, float Cooldown)
{
	return Now - LastTime >= FMath::Max(0.f, Cooldown);
}

EBTNodeResult::Type UBTTask_DM_FindCombatPosition::FailWithoutFallback(UBlackboardComponent* Blackboard, FName ResultKey)
{
	if (Blackboard) Blackboard->ClearValue(ResultKey);
	return EBTNodeResult::Failed;
}

void UBTTask_DM_FindCombatPosition::InitializeSearchOutputs(UBlackboardComponent* Blackboard,
	FName MoveGoalKeyName, FName ShouldADSKeyName, FName FireWhileMovingKeyName)
{
	if (!Blackboard) return;
	Blackboard->ClearValue(MoveGoalKeyName);
	Blackboard->SetValueAsBool(ShouldADSKeyName, false);
	Blackboard->SetValueAsBool(FireWhileMovingKeyName, false);
}

EBTNodeResult::Type UBTTask_DM_FindCombatPosition::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	InitializeSearchOutputs(Blackboard, MoveGoalKey.SelectedKeyName,
		ShouldADSKey.SelectedKeyName, FireWhileMovingKey.SelectedKeyName);

	ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(OwnerComp.GetAIOwner());
	APawn* Pawn = AI ? AI->GetPawn() : nullptr;
	UDMCombatKnowledgeComponent* Knowledge = AI ? AI->GetCombatKnowledge() : nullptr;
	const UDMCombatPositionProfile* Profile = GetProfileForPurpose(PositionPurpose);
	UWorld* World = OwnerComp.GetWorld();
	if (!Blackboard || !AI || !Pawn || !Knowledge || !Profile || Profile->Purpose != PositionPurpose
		|| !CommonCandidateQuery || !World)
	{
		LogResult(TEXT("InvalidConfiguration"), 0, 0, nullptr);
		return EBTNodeResult::Failed;
	}

	AActor* Target = Knowledge->GetCombatTarget();
	if (PositionPurpose != EDMCombatPositionPurpose::Hide
		&& (!IsValid(Target) || !Knowledge->IsCombatTargetVisible()))
	{
		LogResult(TEXT("MissingVisibleFocusTarget"), 0, 0, nullptr);
		return EBTNodeResult::Failed;
	}
	TArray<const FDMRememberedThreat*> Threats;
	DMCombatPositionEvaluation::SelectValidThreats(Knowledge->GetThreatRecords(), Target,
		World->GetTimeSeconds(), Profile->Search.ThreatMemoryMaxAge, Profile->Search.MaxThreatCount, Threats);
	if (PositionPurpose == EDMCombatPositionPurpose::Hide && Threats.IsEmpty())
	{
		LogResult(TEXT("NoRememberedThreat"), 0, 0, nullptr);
		return EBTNodeResult::Failed;
	}
	const ADMWeaponActor* Weapon = AI->GetActiveRangeWeapon();
	const float EffectiveRange = Weapon ? Weapon->GetEffectiveRange() : 0.f;
	const float MaximumRange = Weapon ? Weapon->GetMaxFireRange() : 0.f;
	const uint32 ThreatSnapshotHash = DMCombatPositionEvaluation::ComputeThreatSnapshotHash(Threats);
	const uint8 TeamId = AI->GetGenericTeamId().GetId();
	const UDMTacticalAreaSubsystem* Areas = World->GetSubsystem<UDMTacticalAreaSubsystem>();
	ADMTacticalAreaVolume* CurrentArea = Areas
		? Areas->FindVolumeAtLocation(Pawn->GetActorLocation(), true) : nullptr;
	ADMTacticalAreaVolume* CandidateArea = Areas
		? Areas->FindVolumeAtLocation(CachedLocation, true) : nullptr;
	const float CurrentAreaDanger = DMCombatPositionEvaluation::ResolveAreaDanger(
		CurrentArea, TeamId, Profile->UnassignedAreaDanger);
	const float CandidateAreaDanger = DMCombatPositionEvaluation::ResolveAreaDanger(
		CandidateArea, TeamId, Profile->UnassignedAreaDanger);
	const bool bAreaDangerCacheValid = CachedTeamId == TeamId
		&& CachedCurrentArea.Get() == CurrentArea
		&& CachedCandidateArea.Get() == CandidateArea
		&& !DMCombatPositionEvaluation::HasMeaningfulDangerChange(
			CachedCurrentAreaDanger, CurrentAreaDanger, Profile->Search.AreaDangerChangeTolerance)
		&& !DMCombatPositionEvaluation::HasMeaningfulDangerChange(
			CachedCandidateAreaDanger, CandidateAreaDanger, Profile->Search.AreaDangerChangeTolerance);

	const double Now = World->GetTimeSeconds();
	if (bHasCachedLocation && CachedPurpose == PositionPurpose && CachedTarget == Target
		&& CachedProfile.Get() == Profile && CachedWeapon.Get() == Weapon
		&& FMath::IsNearlyEqual(CachedEffectiveRange, EffectiveRange)
		&& FMath::IsNearlyEqual(CachedMaximumRange, MaximumRange)
		&& CachedThreatSnapshotHash == ThreatSnapshotHash
		&& bAreaDangerCacheValid
		&& bCachedUseADS == Profile->bUseADSWhileMoving
		&& bCachedAllowMovingFire == Profile->bAllowFireWhileMoving
		&& Now - CachedAt <= Profile->Search.ResultReuseTime)
	{
		Blackboard->SetValueAsVector(MoveGoalKey.SelectedKeyName, CachedLocation);
		Blackboard->SetValueAsBool(ShouldADSKey.SelectedKeyName, Profile->bUseADSWhileMoving);
		Blackboard->SetValueAsBool(FireWhileMovingKey.SelectedKeyName, Profile->bAllowFireWhileMoving);
		return EBTNodeResult::Succeeded;
	}
	if (!ShouldStartSearch(Now, LastSearchTime, Profile->Search.SearchCooldown))
	{
		LogResult(TEXT("SearchCooldown"), 0, 0, nullptr);
		return EBTNodeResult::Failed;
	}

	LastSearchTime = Now;
	ActiveOwner = &OwnerComp;
	ActiveTarget = Target;
	FEnvQueryRequest Request(CommonCandidateQuery, AI);
	ActiveQueryId = Request.Execute(EEnvQueryRunMode::AllMatching, this, &UBTTask_DM_FindCombatPosition::OnQueryFinished);
	if (ActiveQueryId == INDEX_NONE)
	{
		ActiveOwner.Reset();
		LogResult(TEXT("QueryDidNotStart"), 0, 0, nullptr);
		return EBTNodeResult::Failed;
	}
	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_DM_FindCombatPosition::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8*)
{
	if (ActiveQueryId != INDEX_NONE)
	{
		if (UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(OwnerComp.GetWorld())) Manager->AbortQuery(ActiveQueryId);
	}
	ActiveQueryId = INDEX_NONE;
	ActiveOwner.Reset();
	return EBTNodeResult::Aborted;
}

void UBTTask_DM_FindCombatPosition::OnQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	UBehaviorTreeComponent* OwnerComp = ActiveOwner.Get();
	ActiveQueryId = INDEX_NONE;
	if (!OwnerComp) return;
	if (!Result.IsValid() || !Result->IsSuccessful())
	{
		FailActiveTask(TEXT("EQSFailed"));
		return;
	}
	TArray<FVector> Locations;
	Result->GetAllAsLocations(Locations);
	FDMCombatPositionScore Best;
	int32 FilterPassedCount = 0;
	FName FailureReason = NAME_None;
	if (!EvaluateCandidates(Locations, Best, FilterPassedCount, FailureReason))
	{
		LogResult(FailureReason, Locations.Num(), FilterPassedCount, nullptr);
		FailActiveTask(FailureReason);
		return;
	}
	if (UBlackboardComponent* Blackboard = OwnerComp->GetBlackboardComponent())
	{
		ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(OwnerComp->GetAIOwner());
		const UDMCombatPositionProfile* Profile = GetProfileForPurpose(PositionPurpose);
		Blackboard->SetValueAsVector(MoveGoalKey.SelectedKeyName, Best.Location);
		Blackboard->SetValueAsBool(ShouldADSKey.SelectedKeyName, Profile->bUseADSWhileMoving);
		Blackboard->SetValueAsBool(FireWhileMovingKey.SelectedKeyName, Profile->bAllowFireWhileMoving);
		CachedLocation = Best.Location;
		CachedAt = OwnerComp->GetWorld()->GetTimeSeconds();
		CachedPurpose = PositionPurpose;
		CachedTarget = ActiveTarget;
		CachedProfile = const_cast<UDMCombatPositionProfile*>(Profile);
		CachedWeapon = AI ? AI->GetActiveRangeWeapon() : nullptr;
		CachedEffectiveRange = CachedWeapon.IsValid() ? CachedWeapon->GetEffectiveRange() : 0.f;
		CachedMaximumRange = CachedWeapon.IsValid() ? CachedWeapon->GetMaxFireRange() : 0.f;
		CachedTeamId = AI ? AI->GetGenericTeamId().GetId() : 255;
		CachedCandidateArea = Best.Area;
		CachedCurrentArea = Best.CurrentArea;
		CachedCandidateAreaDanger = Best.AreaDanger;
		CachedCurrentAreaDanger = Best.CurrentAreaDanger;
		TArray<const FDMRememberedThreat*> CurrentThreats;
		if (AI && AI->GetCombatKnowledge())
		{
			DMCombatPositionEvaluation::SelectValidThreats(AI->GetCombatKnowledge()->GetThreatRecords(),
				ActiveTarget.Get(), OwnerComp->GetWorld()->GetTimeSeconds(), Profile->Search.ThreatMemoryMaxAge,
				Profile->Search.MaxThreatCount, CurrentThreats);
		}
		CachedThreatSnapshotHash = DMCombatPositionEvaluation::ComputeThreatSnapshotHash(CurrentThreats);
		bCachedUseADS = Profile->bUseADSWhileMoving;
		bCachedAllowMovingFire = Profile->bAllowFireWhileMoving;
		bHasCachedLocation = true;
		LogResult(NAME_None, Locations.Num(), FilterPassedCount, &Best);
		ActiveOwner.Reset();
		FinishLatentTask(*OwnerComp, EBTNodeResult::Succeeded);
		return;
	}
	FailActiveTask(TEXT("MissingBlackboard"));
}

bool UBTTask_DM_FindCombatPosition::EvaluateCandidates(const TArray<FVector>& QueryLocations,
	FDMCombatPositionScore& OutBest, int32& OutFilterPassedCount, FName& OutFailureReason) const
{
	OutFilterPassedCount = 0;
	LastPrefilterCandidateCount = 0;
	LastFocusLinePassedCount = 0;
	LastExpensiveCandidateCount = 0;
	LastExpectedFireOrigin = FVector::ZeroVector;
	LastRepresentativeThreat = NAME_None;
	UBehaviorTreeComponent* OwnerComp = ActiveOwner.Get();
	ADeathMatchRobotAIController* AI = OwnerComp ? Cast<ADeathMatchRobotAIController>(OwnerComp->GetAIOwner()) : nullptr;
	APawn* Pawn = AI ? AI->GetPawn() : nullptr;
	UWorld* World = OwnerComp ? OwnerComp->GetWorld() : nullptr;
	const UDMCombatPositionProfile* Profile = GetProfileForPurpose(PositionPurpose);
	const UDMCombatKnowledgeComponent* Knowledge = AI ? AI->GetCombatKnowledge() : nullptr;
	AActor* Target = Knowledge ? Knowledge->GetCombatTarget() : nullptr;
	const ADMWeaponActor* Weapon = AI ? AI->GetActiveRangeWeapon() : nullptr;
	if (!AI || !Pawn || !World || !Profile || !Knowledge || (PositionPurpose != EDMCombatPositionPurpose::Hide && (!Target || !Weapon)))
	{
		OutFailureReason = TEXT("InvalidEvaluationContext");
		return false;
	}

	TArray<const FDMRememberedThreat*> Threats;
	DMCombatPositionEvaluation::SelectValidThreats(Knowledge->GetThreatRecords(), Target,
		World->GetTimeSeconds(), Profile->Search.ThreatMemoryMaxAge, Profile->Search.MaxThreatCount, Threats);
	if (PositionPurpose == EDMCombatPositionPurpose::Hide && Threats.IsEmpty())
	{
		OutFailureReason = TEXT("NoRememberedThreat");
		return false;
	}
	const FDMRememberedThreat* RepresentativeThreat = PositionPurpose == EDMCombatPositionPurpose::Hide
		&& !Threats.IsEmpty() ? Threats[0] : nullptr;
	if (RepresentativeThreat && IsValid(RepresentativeThreat->Actor))
		LastRepresentativeThreat = RepresentativeThreat->Actor->GetFName();
	TArray<FVector> FriendlyLocations;
	if (UDMCombatPositionRegistry* Registry = World->GetSubsystem<UDMCombatPositionRegistry>())
	{
		TArray<FDMPositionNeighbor> Neighbors;
		Registry->GatherFriendlyOccupants(AI, Neighbors);
		for (const FDMPositionNeighbor& Neighbor : Neighbors) FriendlyLocations.Add(Neighbor.Location);
	}

	TArray<FVector> LimitedLocations;
	DMCombatPositionEvaluation::SelectSpatiallyDiverseLocations(QueryLocations,
		FMath::Max(1, Profile->Search.MaxCandidateCount), LimitedLocations);
	TArray<FDMCandidateWork> Candidates;
	const uint8 TeamId = AI->GetGenericTeamId().GetId();
	const UDMTacticalAreaSubsystem* Areas = World->GetSubsystem<UDMTacticalAreaSubsystem>();
	ADMTacticalAreaVolume* CurrentArea = Areas
		? Areas->FindVolumeAtLocation(Pawn->GetActorLocation(), true) : nullptr;
	const float CurrentAreaDanger = DMCombatPositionEvaluation::ResolveAreaDanger(
		CurrentArea, TeamId, Profile->UnassignedAreaDanger);
	TMap<const ADMTacticalAreaVolume*, float> DangerByArea;
	DangerByArea.Add(CurrentArea, CurrentAreaDanger);
	auto ResolveCachedAreaDanger = [&DangerByArea, TeamId, Profile](const ADMTacticalAreaVolume* Area)
	{
		if (const float* CachedDanger = DangerByArea.Find(Area)) return *CachedDanger;
		const float Danger = DMCombatPositionEvaluation::ResolveAreaDanger(
			Area, TeamId, Profile->UnassignedAreaDanger);
		DangerByArea.Add(Area, Danger);
		return Danger;
	};
	const ECollisionChannel TraceChannel = UDMCombatFireControlComponent::ResolveWeaponTraceChannel(AI);
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ANavigationData* NavigationData = NavigationSystem
		? NavigationSystem->GetNavDataForProps(Pawn->GetNavAgentPropertiesRef(), Pawn->GetNavAgentLocation()) : nullptr;
	if (!NavigationSystem || !NavigationData)
	{
		OutFailureReason = TEXT("MissingNavigationData");
		return false;
	}
	for (const FVector& Candidate : LimitedLocations)
	{
		const FPathFindingQuery CompletenessQuery(Pawn, *NavigationData,
			Pawn->GetNavAgentLocation(), Candidate);
		if (!NavigationSystem->TestPathSync(CompletenessQuery)) continue;

		FDMCandidateWork& Work = Candidates.AddDefaulted_GetRef();
		Work.Location = Candidate;
		Work.Area = Areas ? Areas->FindVolumeAtLocation(Candidate, true) : nullptr;
		Work.AreaDanger = ResolveCachedAreaDanger(Work.Area.Get());
		Work.AreaDangerIncrease = DMCombatPositionEvaluation::ComputeAreaDangerIncrease(
			Work.AreaDanger, CurrentAreaDanger);
		Work.FireOrigin = UDMCombatFireControlComponent::ResolveCandidateFireOrigin(
			AI, Candidate, Profile->Search.CandidateEyeHeight);
		Work.NearbyFriendlies = static_cast<float>(DMCombatPositionEvaluation::CountNearbyLocations(
			Candidate, FriendlyLocations, Profile->NearbyFriendlyCount.CheckRadius));
		if (PositionPurpose != EDMCombatPositionPurpose::Hide && Target && Weapon)
		{
			const FDMFireRangeEvaluation Range = UDMCombatFireControlComponent::ResolveFireRange(
				Pawn, Target, Work.FireOrigin, Weapon->GetMaxFireRange());
			if (!Range.bAllowed || !DMCombatPositionEvaluation::IsWithinMaximumRange(
				Range.Distance, Range.MaximumRange, Profile->MaximumRangeSafetyMargin))
			{
				Candidates.Pop(EAllowShrinking::No);
				continue;
			}
			Work.RangeFitness = DMCombatPositionEvaluation::ComputeRangeFitness(Range.Distance,
				Weapon->GetEffectiveRange(), Weapon->GetMaxFireRange());
		}
		if (!DMCombatPositionEvaluation::IsCriterionAllowed(Work.RangeFitness, Profile->RangeFitness)
			|| !DMCombatPositionEvaluation::IsCriterionAllowed(Work.NearbyFriendlies, Profile->NearbyFriendlyCount)
			|| !DMCombatPositionEvaluation::IsCriterionAllowed(Work.AreaDanger, Profile->AreaDanger)
			|| !DMCombatPositionEvaluation::IsCriterionAllowed(
				Work.AreaDangerIncrease, Profile->AreaDangerIncrease))
		{
			Candidates.Pop(EAllowShrinking::No);
			continue;
		}
		Work.CheapScore = Profile->RangeFitness.bEnabled
			? DMCombatPositionEvaluation::NormalizeCriterion(Work.RangeFitness, Profile->RangeFitness) * Profile->RangeFitness.Weight : 0.f;
		if (Profile->NearbyFriendlyCount.bEnabled)
			Work.CheapScore += DMCombatPositionEvaluation::NormalizeCriterion(Work.NearbyFriendlies,
				Profile->NearbyFriendlyCount) * Profile->NearbyFriendlyCount.Weight;
		Work.CheapScore += DMCombatPositionEvaluation::ComputeAreaDangerScore(
			Work.AreaDanger, CurrentAreaDanger, Profile->AreaDanger, Profile->AreaDangerIncrease);

		if (PositionPurpose != EDMCombatPositionPurpose::Hide)
		{
			const EDMFireAttemptFailure Failure = UDMCombatFireControlComponent::EvaluateLineOfFire(World,
				Pawn, AI->GetGenericTeamId(), Work.FireOrigin, Target, Weapon->GetMaxFireRange(), TraceChannel);
			Work.bFocusLineClear = Failure == EDMFireAttemptFailure::None;
			if (Work.bFocusLineClear) ++LastFocusLinePassedCount;
		}
		else if (Profile->Search.bUseHideRepresentativeThreatPrefilter && RepresentativeThreat)
		{
			Work.bRepresentativeConcealed = !IsExposedToThreat(World, Pawn, RepresentativeThreat,
				Work.FireOrigin, TraceChannel, Profile->Search.CandidateEyeHeight);
		}
		else
		{
			Work.bRepresentativeConcealed = true;
		}
	}
	LastPrefilterCandidateCount = Candidates.Num();
	if (Candidates.IsEmpty())
	{
		OutFailureReason = TEXT("NoCompleteNavCandidate");
		return false;
	}
	TArray<FDMCombatPositionPrefilterCandidate> PrefilterCandidates;
	for (const FDMCandidateWork& Work : Candidates)
	{
		FDMCombatPositionPrefilterCandidate& Candidate = PrefilterCandidates.AddDefaulted_GetRef();
		Candidate.Location = Work.Location;
		Candidate.CheapScore = Work.CheapScore;
		Candidate.bFocusLineClear = Work.bFocusLineClear;
		Candidate.bRepresentativeConcealed = Work.bRepresentativeConcealed;
	}
	const int32 ExpensiveLimit = PositionPurpose == EDMCombatPositionPurpose::Hide
		&& Profile->Search.bUseHideRepresentativeThreatPrefilter
		? Profile->Search.HidePrefilterCandidateCount : Profile->Search.ExpensiveCheckCandidateCount;
	TArray<int32> Shortlist;
	DMCombatPositionEvaluation::SelectPurposeShortlist(PrefilterCandidates, PositionPurpose,
		FMath::Max(1, ExpensiveLimit), Profile->Search.HideExposedCandidateFraction, Shortlist);
	LastExpensiveCandidateCount = Shortlist.Num();
	if (PositionPurpose != EDMCombatPositionPurpose::Hide && Shortlist.IsEmpty())
	{
		OutFailureReason = TEXT("NoFocusLineCandidate");
		return false;
	}

	bool bFound = false;
	for (const int32 CandidateIndex : Shortlist)
	{
		FDMCandidateWork& Work = Candidates[CandidateIndex];
		UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World,
			Pawn->GetActorLocation(), Work.Location, Pawn);
		if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
		Work.PathLength = Path->GetPathLength();
		FDMCombatPositionScore Score;
		Score.Location = Work.Location;
		Score.RangeFitness = Work.RangeFitness;
		Score.NearbyFriendlyCount = Work.NearbyFriendlies;
		Score.PathCost = Work.PathLength;
		Score.AreaDanger = Work.AreaDanger;
		Score.CurrentAreaDanger = CurrentAreaDanger;
		Score.AreaDangerIncrease = Work.AreaDangerIncrease;
		Score.Area = Work.Area;
		Score.CurrentArea = CurrentArea;
		if (PositionPurpose != EDMCombatPositionPurpose::Hide)
		{
			Score.FireFeasibility = Work.bFocusLineClear ? 1.f : 0.f;
			if (!Work.bFocusLineClear) continue;
		}

		TArray<bool> Exposures;
		TArray<bool> FocusFlags;
		for (const FDMRememberedThreat* Threat : Threats)
		{
			Exposures.Add(IsExposedToThreat(World, Pawn, Threat, Work.FireOrigin,
				TraceChannel, Profile->Search.CandidateEyeHeight));
			FocusFlags.Add(Threat->Actor == Target);
		}
		Score.ThreatExposure = DMCombatPositionEvaluation::ComputeWeightedExposure(Exposures, FocusFlags,
			Profile->FocusingTargetExposureWeight, Profile->OtherThreatExposureWeight, Score.ExposedThreats);
		Score.EvaluatedThreats = Threats.Num();
		const float ConcealedFraction = Threats.IsEmpty() ? 0.f
			: 1.f - static_cast<float>(Score.ExposedThreats) / Threats.Num();
		if (PositionPurpose == EDMCombatPositionPurpose::Hide
			&& ConcealedFraction < FMath::Clamp(Profile->MinimumConcealedFraction, 0.f, 1.f)) continue;
		if ((PositionPurpose != EDMCombatPositionPurpose::Hide
				&& !DMCombatPositionEvaluation::IsCriterionAllowed(Score.FireFeasibility, Profile->FireFeasibility))
			|| !DMCombatPositionEvaluation::IsCriterionAllowed(Score.RangeFitness, Profile->RangeFitness)
			|| !DMCombatPositionEvaluation::IsCriterionAllowed(Score.ThreatExposure, Profile->ThreatExposure)
			|| !DMCombatPositionEvaluation::IsCriterionAllowed(Score.NearbyFriendlyCount, Profile->NearbyFriendlyCount)
			|| !DMCombatPositionEvaluation::IsCriterionAllowed(Score.PathCost, Profile->PathCost)
			|| !DMCombatPositionEvaluation::IsCriterionAllowed(Score.AreaDanger, Profile->AreaDanger)
			|| !DMCombatPositionEvaluation::IsCriterionAllowed(
				Score.AreaDangerIncrease, Profile->AreaDangerIncrease)) continue;
		++OutFilterPassedCount;

		Score.Total = 0.f;
		if (Profile->FireFeasibility.bEnabled) Score.Total += Score.FireFeasibility * Profile->FireFeasibility.Weight;
		if (Profile->RangeFitness.bEnabled) Score.Total += DMCombatPositionEvaluation::NormalizeCriterion(
			Score.RangeFitness, Profile->RangeFitness) * Profile->RangeFitness.Weight;
		if (Profile->ThreatExposure.bEnabled) Score.Total += DMCombatPositionEvaluation::NormalizeCriterion(
			Score.ThreatExposure, Profile->ThreatExposure) * Profile->ThreatExposure.Weight;
		if (Profile->NearbyFriendlyCount.bEnabled) Score.Total += DMCombatPositionEvaluation::NormalizeCriterion(
			Score.NearbyFriendlyCount, Profile->NearbyFriendlyCount) * Profile->NearbyFriendlyCount.Weight;
		if (Profile->PathCost.bEnabled) Score.Total += DMCombatPositionEvaluation::NormalizeCriterion(
			Score.PathCost, Profile->PathCost) * Profile->PathCost.Weight;
		Score.Total += DMCombatPositionEvaluation::ComputeAreaDangerScore(
			Score.AreaDanger, CurrentAreaDanger, Profile->AreaDanger, Profile->AreaDangerIncrease);
		if (!bFound || Score.Total > OutBest.Total)
		{
			OutBest = Score;
			LastExpectedFireOrigin = Work.FireOrigin;
			bFound = true;
		}
	}
	OutFailureReason = bFound ? NAME_None : FName(TEXT("NoCandidatePassedPurposeFilters"));
	return bFound;
}

void UBTTask_DM_FindCombatPosition::FailActiveTask(FName Reason)
{
	if (UBehaviorTreeComponent* OwnerComp = ActiveOwner.Get())
	{
		InitializeSearchOutputs(OwnerComp->GetBlackboardComponent(), MoveGoalKey.SelectedKeyName,
			ShouldADSKey.SelectedKeyName, FireWhileMovingKey.SelectedKeyName);
		ActiveOwner.Reset();
		FinishLatentTask(*OwnerComp, EBTNodeResult::Failed);
	}
}

void UBTTask_DM_FindCombatPosition::LogResult(FName FailureReason, int32 CandidateCount,
	int32 FilteredCount, const FDMCombatPositionScore* Selected) const
{
	const UBehaviorTreeComponent* OwnerComp = ActiveOwner.Get();
	const UWorld* World = OwnerComp ? OwnerComp->GetWorld() : GetWorld();
	if (!DMCombatDebug::IsEnabled(World) || DMCombatDebug::Mode() < 2) return;
	if (Selected)
	{
		UE_LOG(LogTemp, Log, TEXT("DM CombatPosition Purpose=%s Candidates=%d Prefilter=%d FocusFire=%d Expensive=%d Passed=%d Selected=%s FireOrigin=%s Score=%.3f Fire=%.2f Exposure=%.2f Range=%.2f Friendlies=%.0f Path=%.0f Area=%s CurrentArea=%s AreaDanger=%.1f DangerIncrease=%.1f HideThreat=%s"),
			PurposeName(PositionPurpose), CandidateCount, LastPrefilterCandidateCount, LastFocusLinePassedCount,
			LastExpensiveCandidateCount, FilteredCount, *Selected->Location.ToCompactString(),
			*LastExpectedFireOrigin.ToCompactString(),
			Selected->Total, Selected->FireFeasibility, Selected->ThreatExposure, Selected->RangeFitness,
			Selected->NearbyFriendlyCount, Selected->PathCost, *GetNameSafe(Selected->Area.Get()),
			*GetNameSafe(Selected->CurrentArea.Get()), Selected->AreaDanger, Selected->AreaDangerIncrease,
			*LastRepresentativeThreat.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("DM CombatPosition Purpose=%s Candidates=%d Prefilter=%d FocusFire=%d Expensive=%d Passed=%d Failed=%s HideThreat=%s"),
			PurposeName(PositionPurpose), CandidateCount, LastPrefilterCandidateCount, LastFocusLinePassedCount,
			LastExpensiveCandidateCount, FilteredCount, *FailureReason.ToString(), *LastRepresentativeThreat.ToString());
	}
}
