#pragma once

#include "CoreMinimal.h"
#include "DeathMatch/Controller/DeathMatchAIController.h"
#include "PartsCharacter/PartsTypes.h"
#include "DeathMatch/AIModules/DMPerceptionTypes.h"
#include "GenericTeamAgentInterface.h"
#include "Navigation/PathFollowingComponent.h"
#include "DeathMatchRobotAIController.generated.h"

class ADeathMatchRobotCharacter;
class UPartsComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAIModule_Sense;
class UBehaviorTree;
class ADMMovePoint;
class UDMCombatMoveDirector;
class UDMCombatKnowledgeComponent;
class UDMCombatFireControlComponent;
class UDMMovementSpeedPolicyComponent;
class UDMActionIntentComponent;
class UDMCombatFocusComponent;
class UDMCombatStatusComponent;
class UDMCombatTargetSelectionPolicy;
class ADMWeaponActor;
class UNavigationPath;


/**
 * DeathMatch robot AI: parts gates + Phase 1 perception gather (Sense).
 */
UCLASS()
class SKYSEOUL_API ADeathMatchRobotAIController : public ADeathMatchAIController
{
	GENERATED_BODY()

public:
	ADeathMatchRobotAIController();

	virtual TSubclassOf<APawn> GetAIPawnClass() const override;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Robot|AI")
	void RunDeathMatchBehaviorTree();

	// AAIController already implements IGenericTeamAgentInterface — override only.
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	virtual FGenericTeamId GetGenericTeamId() const override;

	/**
	 * Used by AIPerception DetectionByAffiliation (Enemy/Neutral/Friendly).
	 * Must resolve Other through Pawn→PlayerState/Controller — default AIC Cast fails on Pawns.
	 */
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot")
	ADeathMatchRobotCharacter* GetRobotCharacter() const { return CachedRobot.Get(); }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot")
	UPartsComponent* GetPartsComponent() const { return CachedParts.Get(); }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Parts")
	bool CanShoot() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Parts")
	EPartsArmState GetLeftArmState() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Parts")
	EPartsArmState GetRightArmState() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Parts")
	bool IsHeadIntact() const;

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Robot|Parts")
	void NotifyPossessedRobotDied();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Robot|Parts")
	void NotifyPossessedRobotRespawnReset();

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Perception")
	UAIModule_Sense* GetSenseModule() const { return SenseModule; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Perception")
	AActor* GetFocusTarget() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Perception")
	FVector GetFocusLocation() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Perception")
	bool HasHostile() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Perception")
	AActor* FindBestHostile(bool bPreferSeenOnly = true) const;

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Robot|Perception")
	void GetSeenHostiles(TArray<AActor*>& OutHostiles) const;

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Robot|Perception")
	void GetKnownHostiles(TArray<AActor*>& OutHostiles) const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Perception")
	bool GetAwareTarget(AActor* Target, FDMAwareTarget& OutData) const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Move")
	ADMMovePoint* GetLastMovePoint() const { return LastMovePoint.Get(); }

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Robot|Move")
	void SetLastMovePoint(ADMMovePoint* Point);

	/** Applies the DeathMatch-only RVO policy. Crowd following is intentionally not combined with it. */
	void SetNavigationMovementMode(bool bCombat);
	bool ValidateMoveGoal(const FVector& Goal, bool bCombat, FVector& OutProjectedGoal, FName& OutFailureReason);
	bool IsMoveGoalTemporarilyBlocked(const FVector& Goal) const;
	void RecordMoveFailure(const FVector& Goal, FName Reason, bool bLogWarning = true);
	void SetMoveDebugOwner(FName OwnerName, const FVector& Goal);
	void SetStuckDebug(FName Reason, int32 RepathCount);
	FString GetNavigationDebugSummary() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Move")
	UDMCombatMoveDirector* GetCombatMoveDirector() const { return CombatMoveDirector; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Combat")
	UDMCombatKnowledgeComponent* GetCombatKnowledge() const { return CombatKnowledge; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Combat")
	UDMCombatFireControlComponent* GetCombatFireControl() const { return CombatFireControl; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Move")
	UDMMovementSpeedPolicyComponent* GetMovementSpeedPolicy() const { return MovementSpeedPolicy; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Combat")
	UDMActionIntentComponent* GetActionIntentComponent() const { return ActionIntentComponent; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Combat")
	UDMCombatFocusComponent* GetCombatFocusComponent() const { return CombatFocusComponent; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Combat")
	UDMCombatStatusComponent* GetCombatStatus() const { return CombatStatus; }

	UDMCombatTargetSelectionPolicy* GetTargetSelectionPolicy() const { return TargetSelectionPolicy; }

	/** Read current weapon data directly so swaps and runtime tuning apply immediately. */
	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Weapon")
	float GetActiveWeaponEffectiveRange() const;
	UFUNCTION(BlueprintPure, Category = "DeathMatch|Robot|Weapon")
	float GetActiveWeaponMaxFireRange() const;
	ADMWeaponActor* GetActiveRangeWeapon() const;

	/**
	 * Pawn spawned for this AI (DeathMatchRobotCharacter BP).
	 * Set on BP_AIC_DM_Robot Class Defaults — not on GameMode.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot")
	TSubclassOf<ADeathMatchRobotCharacter> AICharacterClass;

	/**
	 * Single root Behavior Tree. Branch priority and state transitions live in this asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|AI", meta = (DisplayName = "Root Behavior Tree"))
	TObjectPtr<UBehaviorTree> RootBehaviorTree;

protected:
	void RefreshRobotCache(APawn* InPawn);
	void ClearRobotCache();
	void SetupPerception();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Robot|Perception")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Robot|Perception")
	TObjectPtr<UAIModule_Sense> SenseModule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Robot|Move")
	TObjectPtr<UDMCombatMoveDirector> CombatMoveDirector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Robot|Combat")
	TObjectPtr<UDMCombatKnowledgeComponent> CombatKnowledge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Robot|Combat")
	TObjectPtr<UDMCombatFireControlComponent> CombatFireControl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Robot|Move")
	TObjectPtr<UDMMovementSpeedPolicyComponent> MovementSpeedPolicy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Robot|Combat")
	TObjectPtr<UDMActionIntentComponent> ActionIntentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Robot|Combat")
	TObjectPtr<UDMCombatFocusComponent> CombatFocusComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Robot|Combat")
	TObjectPtr<UDMCombatStatusComponent> CombatStatus;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "DeathMatch|Robot|Combat")
	TObjectPtr<UDMCombatTargetSelectionPolicy> TargetSelectionPolicy;

	UPROPERTY(EditAnywhere, Category = "DeathMatch|Robot|Perception")
	float SightRadius = 3000.0f;

	UPROPERTY(EditAnywhere, Category = "DeathMatch|Robot|Perception")
	float LoseSightRadiusOffset = 500.0f;

	UPROPERTY(EditAnywhere, Category = "DeathMatch|Robot|Perception")
	float PeripheralVisionAngleDegrees = 90.0f;

	UPROPERTY(EditAnywhere, Category = "DeathMatch|Robot|Perception")
	float SightMaxAge = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Quality",
		meta = (ClampMin = "1.0", ToolTip = "Quality-only wall clearance scale for Roam. It never rejects an otherwise valid move."))
	float RoamWallClearanceScale = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Quality",
		meta = (ClampMin = "1.0", ToolTip = "Quality-only wall clearance scale for Combat. It never rejects an otherwise valid move."))
	float CombatWallClearanceScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Passability",
		meta = (ClampMin = "0.5", ClampMax = "1.0", UIMin = "0.5", UIMax = "1.0", ToolTip = "Radius scale for the loose preflight probe, not the character's physical capsule size."))
	float PathPassabilityRadiusScale = 0.70f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Passability",
		meta = (ClampMin = "0.5", ClampMax = "1.0", UIMin = "0.5", UIMax = "1.0", ToolTip = "Half-height scale for the loose preflight probe. The result is clamped to at least its radius."))
	float PathPassabilityHalfHeightScale = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Passability",
		meta = (ClampMin = "0.5", ClampMax = "1.0", UIMin = "0.5", UIMax = "1.0", ToolTip = "Slightly reduced capsule used to verify that the final NavMesh destination can hold the character."))
	float DestinationCapsuleScale = 0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Passability",
		meta = (ToolTip = "Raises loose probes on stair or slope segments to avoid treating walkable step faces as walls."))
	bool bRaisePassabilityProbeOnVerticalSegments = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Passability",
		meta = (ClampMin = "0.0", Units = "cm", ToolTip = "Minimum segment height change that activates the stair/slope raised probe."))
	float VerticalSegmentThreshold = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Passability",
		meta = (ClampMin = "0.0", ToolTip = "Multiplier applied to Character Movement Max Step Height for raised stair/slope probes."))
	float StepHeightProbeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Passability",
		meta = (ClampMin = "1.0", Units = "cm", ToolTip = "Maximum spacing of sweeps inside each real Nav Path segment; sweeps never bridge path corners."))
	float PathClearanceSampleSpacing = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Avoidance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RoamAvoidanceWeight = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Avoidance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CombatAvoidanceWeight = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Avoidance", meta = (ClampMin = "0.0", Units = "cm"))
	float AvoidanceConsiderationRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Robot|Navigation|Recovery", meta = (ClampMin = "0.1", Units = "s"))
	float BlockedPathCooldown = 1.5f;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Robot")
	TWeakObjectPtr<ADeathMatchRobotCharacter> CachedRobot;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Robot")
	TWeakObjectPtr<UPartsComponent> CachedParts;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|Robot|Move")
	TWeakObjectPtr<ADMMovePoint> LastMovePoint;

	struct FBlockedMoveGoal
	{
		FVector Goal = FVector::ZeroVector;
		double ExpiresAt = 0.0;
		FName Reason = NAME_None;
	};
	TArray<FBlockedMoveGoal> BlockedMoveGoals;
	FName CurrentMoveOwner = NAME_None;
	FName LastNavigationState = NAME_None;
	FName LastClearanceFailure = NAME_None;
	FName LastPathQuality = NAME_None;
	FName LastStuckReason = NAME_None;
	FVector CurrentMoveDestination = FVector::ZeroVector;
	bool bLastDynamicObstacleDetected = false;
	bool bLastRaisedStepProbeUsed = false;
	int32 DebugRepathCount = 0;

	FGenericTeamId CachedTeamId;
	mutable TWeakObjectPtr<ADMWeaponActor> CachedRangeWeapon;
};
