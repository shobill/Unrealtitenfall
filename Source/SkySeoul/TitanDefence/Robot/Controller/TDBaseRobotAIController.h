#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AITypes.h"
#include "TitanDefence/Robot/Core/TDRobotMovementState.h"
#include "TDBaseRobotAIController.generated.h"

class ATDBaseRobotCharacter;
class ANavigationData;
class UNavigationSystemV1;
struct FPathFollowingResult;

/** Owns the single active NavMesh movement command for a Titan Defence robot. */
UCLASS()
class SKYSEOUL_API ATDBaseRobotAIController : public AAIController
{
	GENERATED_BODY()

public:
	ATDBaseRobotAIController();
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	/** Schedules the automatic initial move for the first tick after robot BeginPlay. */
	void NotifyRobotBeginPlay(ATDBaseRobotCharacter* Robot);

	/** Replaces the active command and moves the possessed robot to TargetActor. */
	bool RequestMoveToActor(AActor* TargetActor, float AcceptanceRadius);

	/** Cancels the active command and returns the possessed robot to Idle. */
	void CancelCurrentMove();

	/** Clears an absent target without treating it as a failure or cancellation. */
	bool ClearMoveTarget();

	/** Stops the active request and enters a future-facing external interruption state. */
	void InterruptCurrentMove(
		ETDRobotMovementState InterruptionState,
		ETDRobotMovementEndReason EndReason);

private:
	void StartDeferredInitialMove();
	bool StartMoveNow(AActor* TargetActor, float AcceptanceRadius);
	bool QueueMoveUntilNavigationIsReady(AActor* TargetActor, float AcceptanceRadius);
	void RetryQueuedMove();
	void StartQueuedMoveIfReady(const TCHAR* AvailabilitySource);
	void CancelQueuedMove(const TCHAR* Reason, bool bUpdateRobotState);
	void ResetQueuedMove();
	void BindNavigationGenerationEvent(UNavigationSystemV1* NavigationSystem);
	float GetNavigationWaitElapsed() const;
	FString BuildNavigationLogContext(AActor* TargetActor, float WaitTime, int32 RetryCount) const;
	ETDRobotMovementEndReason GetFailureReason(const FPathFollowingResult& Result) const;
	void InvalidateActiveMove();
	void UnbindActiveTarget();

	UFUNCTION()
	void HandleActiveTargetDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleQueuedTargetDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleNavigationGenerationFinished(ANavigationData* GeneratedNavData);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Titan Defence|Robot|Movement|Navigation",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float NavigationRetryInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Titan Defence|Robot|Movement|Navigation",
		meta = (AllowPrivateAccess = "true", ClampMin = "0.1", UIMin = "0.1", Units = "s"))
	float NavigationMaximumWaitTime = 5.0f;

	TWeakObjectPtr<ATDBaseRobotCharacter> PossessedRobot;
	TWeakObjectPtr<AActor> ActiveTargetActor;
	TWeakObjectPtr<AActor> QueuedTargetActor;
	TWeakObjectPtr<UNavigationSystemV1> BoundNavigationSystem;
	FAIRequestID ActiveMoveRequestId = FAIRequestID::InvalidRequest;
	FTimerHandle DeferredInitialMoveTimerHandle;
	FTimerHandle NavigationRetryTimerHandle;
	float QueuedAcceptanceRadius = 0.0f;
	double NavigationWaitStartTime = -1.0;
	int32 NavigationRetryCount = 0;
	bool bInitialMoveDeferred = false;
};

