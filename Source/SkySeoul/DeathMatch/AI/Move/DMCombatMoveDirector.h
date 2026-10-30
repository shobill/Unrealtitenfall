#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeathMatch/AI/Move/DMMoveTypes.h"
#include "DMCombatMoveDirector.generated.h"

class ADeathMatchRobotAIController;
enum class EDMBehaviorState : uint8;

/** Executes BT-issued path requests. It owns no provider registry or behavior priority policy. */
UCLASS(ClassGroup=(DeathMatch), meta=(BlueprintSpawnableComponent))
class SKYSEOUL_API UDMCombatMoveDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UDMCombatMoveDirector();

	uint64 SubmitMoveRequest(EDMMoveRequestOwner RequestOwner, int32 Priority, const FVector& Goal,
		float AcceptanceRadius, bool bCombatMovement, bool bProjectDestinationToNavigation = true);
	void CancelMoveRequest(uint64 RequestId, bool bStopMovement = true);
	void CancelMoveRequest(EDMMoveRequestOwner RequestOwner, bool bStopMovement = true);
	void CancelAllMoveRequests(bool bStopMovement = true);
	void TransitionToBehaviorState(EDMBehaviorState NewState);
	void NotifyPathFollowingCompleted(bool bSuccess);

	UFUNCTION(BlueprintPure, Category="DeathMatch|Move")
	EDMMoveRequestOwner GetActiveRequestOwner() const { return ActiveRequestOwner; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Move")
	int32 GetActiveRequestPriority() const { return ActiveRequestPriority; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Move")
	FVector GetActiveRequestGoal() const { return ActiveRequestGoal; }
	uint64 GetActiveRequestId() const { return ActiveRequestId; }
	bool IsMoveRequestActive(uint64 RequestId) const { return RequestId != 0 && RequestId == ActiveRequestId; }

	uint32 GetCompletionSerial() const { return CompletionSerial; }
	bool WasLastMoveSuccessful() const { return bLastMoveSuccessful; }
	EDMMoveRequestOwner GetLastCompletedOwner() const { return LastCompletedOwner; }
	uint64 GetLastCompletedRequestId() const { return LastCompletedRequestId; }

	static bool CanReplaceRequest(EDMMoveRequestOwner ActiveOwner, int32 ActivePriority,
		EDMMoveRequestOwner IncomingOwner, int32 IncomingPriority);
	static EDMMoveRequestOwner GetAllowedOwnerForState(EDMBehaviorState State);
	static bool IsOwnerAllowedForState(EDMBehaviorState State, EDMMoveRequestOwner Owner);

private:
	ADeathMatchRobotAIController* GetOwnerRobotAI() const;

	UPROPERTY(Transient, VisibleAnywhere, Category="DeathMatch|Move")
	EDMMoveRequestOwner ActiveRequestOwner = EDMMoveRequestOwner::None;

	UPROPERTY(Transient, VisibleAnywhere, Category="DeathMatch|Move")
	int32 ActiveRequestPriority = TNumericLimits<int32>::Lowest();

	UPROPERTY(Transient, VisibleAnywhere, Category="DeathMatch|Move")
	FVector ActiveRequestGoal = FVector::ZeroVector;
	uint64 ActiveRequestId = 0;
	uint64 NextRequestId = 1;

	EDMMoveRequestOwner LastCompletedOwner = EDMMoveRequestOwner::None;
	uint64 LastCompletedRequestId = 0;
	uint32 CompletionSerial = 0;
	bool bLastMoveSuccessful = false;
};
