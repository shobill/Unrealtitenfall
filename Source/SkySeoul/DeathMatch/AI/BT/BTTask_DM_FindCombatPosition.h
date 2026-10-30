#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "DeathMatch/AI/Combat/DMCombatPositionProfile.h"
#include "BTTask_DM_FindCombatPosition.generated.h"

class UEnvQuery;
class UBlackboardComponent;
class ADMWeaponActor;
class ADMTacticalAreaVolume;
struct FEnvQueryResult;

/** Runs one shared candidate EQS and evaluates its points with the selected purpose profile. */
UCLASS(meta=(DisplayName="DM Find Combat Position"))
class SKYSEOUL_API UBTTask_DM_FindCombatPosition : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_DM_FindCombatPosition();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	const UDMCombatPositionProfile* GetProfileForPurpose(EDMCombatPositionPurpose Purpose) const;
	static bool ShouldStartSearch(double Now, double LastSearchTime, float Cooldown);
	static EBTNodeResult::Type FailWithoutFallback(UBlackboardComponent* Blackboard, FName ResultKey);
	static void InitializeSearchOutputs(UBlackboardComponent* Blackboard, FName MoveGoalKeyName,
		FName ShouldADSKeyName, FName FireWhileMovingKeyName);

	UPROPERTY(EditAnywhere, Category="Position") EDMCombatPositionPurpose PositionPurpose = EDMCombatPositionPurpose::Attack;
	UPROPERTY(EditAnywhere, Category="Position") TObjectPtr<UEnvQuery> CommonCandidateQuery;
	UPROPERTY(EditAnywhere, Category="Position|Profiles") TObjectPtr<UDMCombatPositionProfile> AttackPositionProfile;
	UPROPERTY(EditAnywhere, Category="Position|Profiles") TObjectPtr<UDMCombatPositionProfile> ApproachPositionProfile;
	UPROPERTY(EditAnywhere, Category="Position|Profiles") TObjectPtr<UDMCombatPositionProfile> HidePositionProfile;

	UPROPERTY(EditAnywhere, Category="Blackboard") FBlackboardKeySelector MoveGoalKey;
	UPROPERTY(EditAnywhere, Category="Blackboard") FBlackboardKeySelector ShouldADSKey;
	UPROPERTY(EditAnywhere, Category="Blackboard") FBlackboardKeySelector FireWhileMovingKey;

private:
	void OnQueryFinished(TSharedPtr<FEnvQueryResult> Result);
	bool EvaluateCandidates(const TArray<FVector>& QueryLocations, FDMCombatPositionScore& OutBest,
		int32& OutFilterPassedCount, FName& OutFailureReason) const;
	void FailActiveTask(FName Reason);
	void LogResult(FName FailureReason, int32 CandidateCount, int32 FilteredCount,
		const FDMCombatPositionScore* Selected) const;

	TWeakObjectPtr<UBehaviorTreeComponent> ActiveOwner;
	TWeakObjectPtr<AActor> ActiveTarget;
	int32 ActiveQueryId = INDEX_NONE;
	double LastSearchTime = -BIG_NUMBER;
	double CachedAt = -BIG_NUMBER;
	EDMCombatPositionPurpose CachedPurpose = EDMCombatPositionPurpose::Attack;
	TWeakObjectPtr<AActor> CachedTarget;
	TWeakObjectPtr<UDMCombatPositionProfile> CachedProfile;
	TWeakObjectPtr<ADMWeaponActor> CachedWeapon;
	FVector CachedLocation = FVector::ZeroVector;
	float CachedEffectiveRange = 0.f;
	float CachedMaximumRange = 0.f;
	float CachedCandidateAreaDanger = 0.f;
	float CachedCurrentAreaDanger = 0.f;
	uint8 CachedTeamId = 255;
	TWeakObjectPtr<ADMTacticalAreaVolume> CachedCandidateArea;
	TWeakObjectPtr<ADMTacticalAreaVolume> CachedCurrentArea;
	uint32 CachedThreatSnapshotHash = 0;
	bool bCachedUseADS = false;
	bool bCachedAllowMovingFire = false;
	bool bHasCachedLocation = false;
	mutable int32 LastPrefilterCandidateCount = 0;
	mutable int32 LastFocusLinePassedCount = 0;
	mutable int32 LastExpensiveCandidateCount = 0;
	mutable FVector LastExpectedFireOrigin = FVector::ZeroVector;
	mutable FName LastRepresentativeThreat = NAME_None;
};
