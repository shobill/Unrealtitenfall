#pragma once

#include "CoreMinimal.h"
#include "DeathMatch/Navigation/DMTacticalGridTypes.h"
#include "DeathMatch/Navigation/DMTacticalAreaVolume.h"
#include "DMMoveTypes.generated.h"

UENUM(BlueprintType)
enum class EDMMoveIntent : uint8
{
	None      UMETA(DisplayName = "None"),
	Roam      UMETA(DisplayName = "Roam"),
	Approach  UMETA(DisplayName = "Approach"),
	HuntLost  UMETA(DisplayName = "HuntLost"),
	Hold      UMETA(DisplayName = "Hold"),
};

/** The single active path request is owned and issued by Combat Move Director. */
UENUM(BlueprintType)
enum class EDMMoveRequestOwner : uint8
{
	None,
	StrategicRoam,
	Investigate,
	Combat,
	Retreat,
	EmergencyHide
};

namespace DMMovePriority
{
	static constexpr int32 StrategicRoam = 10;
	static constexpr int32 Investigate = 20;
	static constexpr int32 Combat = 30;
	static constexpr int32 Retreat = 40;
	static constexpr int32 EmergencyHide = 50;
}

/** Result of a move-goal provider. Director writes this to Blackboard later. */
USTRUCT(BlueprintType)
struct FDMMoveGoal
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	EDMMoveIntent Intent = EDMMoveIntent::None;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	TWeakObjectPtr<AActor> RelatedTarget;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	bool bUsesTacticalGrid = false;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	bool bUsedMovePointFallback = false;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	FDMTacticalCellId TacticalCell;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	EDMTacticalPurpose TacticalPurpose = EDMTacticalPurpose::None;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	FDMTacticalPathResult TacticalPath;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	FVector ControlledApproachDirection = FVector::ZeroVector;

	/** First-pass coarse-volume Roam metadata. It is intentionally separate from Tactical Grid. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	bool bUsesTacticalArea = false;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	TWeakObjectPtr<ADMTacticalAreaVolume> TargetVolume;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	TWeakObjectPtr<ADMTacticalAreaVolume> CurrentVolume;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	float SelectionWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	float BaseDanger = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	float DynamicDanger = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	float FinalDanger = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	float PreferredDanger = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	float DangerFit = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	float TotalSelectionWeight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	float RandomRoll = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	int32 RandomAttemptCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	float AcceptanceRadius = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Move")
	float StuckTimeout = 0.f;

	static FDMMoveGoal Invalid()
	{
		return FDMMoveGoal();
	}

	static FDMMoveGoal Make(EDMMoveIntent InIntent, const FVector& InLocation, AActor* InRelated = nullptr)
	{
		FDMMoveGoal Goal;
		Goal.bValid = true;
		Goal.Intent = InIntent;
		Goal.Location = InLocation;
		Goal.RelatedTarget = InRelated;
		return Goal;
	}

	static FDMMoveGoal MakeTacticalRoam(const FVector& InLocation, const FDMTacticalCellScore& Score)
	{
		FDMMoveGoal Goal = Make(EDMMoveIntent::Roam, InLocation);
		Goal.bUsesTacticalGrid = true;
		Goal.TacticalCell = Score.CellId;
		Goal.TacticalPurpose = Score.Purpose;
		Goal.TacticalPath = Score.Path;
		Goal.ControlledApproachDirection = Score.ControlledApproachDirection;
		return Goal;
	}

	static FDMMoveGoal MakeTacticalAreaRoam(const FVector& InLocation,
		ADMTacticalAreaVolume* InVolume, ADMTacticalAreaVolume* InCurrentVolume,
		float InSelectionWeight, float InBaseDanger, float InDynamicDanger, float InFinalDanger,
		float InPreferredDanger, float InDangerFit, float InTotalSelectionWeight, float InRandomRoll,
		int32 InRandomAttemptCount,
		float InAcceptanceRadius, float InStuckTimeout)
	{
		FDMMoveGoal Goal = Make(EDMMoveIntent::Roam, InLocation, InVolume);
		Goal.bUsesTacticalArea = true;
		Goal.TargetVolume = InVolume;
		Goal.CurrentVolume = InCurrentVolume;
		Goal.SelectionWeight = InSelectionWeight;
		Goal.BaseDanger = InBaseDanger;
		Goal.DynamicDanger = InDynamicDanger;
		Goal.FinalDanger = InFinalDanger;
		Goal.PreferredDanger = InPreferredDanger;
		Goal.DangerFit = InDangerFit;
		Goal.TotalSelectionWeight = InTotalSelectionWeight;
		Goal.RandomRoll = InRandomRoll;
		Goal.RandomAttemptCount = InRandomAttemptCount;
		Goal.AcceptanceRadius = InAcceptanceRadius;
		Goal.StuckTimeout = InStuckTimeout;
		return Goal;
	}

};
