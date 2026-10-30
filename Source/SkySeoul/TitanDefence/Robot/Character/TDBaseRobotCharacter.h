#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TitanDefence/Robot/Core/TDRobotMovementState.h"
#include "TDBaseRobotCharacter.generated.h"

class ATDBaseRobotAIController;
class UAbilitySystemComponent;
class UTDRobotAbilitySystemComponent;
class UTDRobotAttributeSet;
class UTDRobotStateComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FTDRobotMovementStateChanged,
	ETDRobotMovementState, PreviousState,
	ETDRobotMovementState, NewState,
	ETDRobotMovementEndReason, EndReason,
	AActor*, CurrentTargetActor);

/**
 * Common Blueprint parent for Titan Defence robots.
 *
 * This character owns movement configuration and observable state only. The
 * possessed ATDBaseRobotAIController owns pathfinding and movement execution.
 */
UCLASS(Blueprintable)
class SKYSEOUL_API ATDBaseRobotCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ATDBaseRobotCharacter();

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|State")
	UTDRobotStateComponent* GetRobotStateComponent() const { return RobotStateComponent; }

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|State")
	UTDRobotAbilitySystemComponent* GetRobotAbilitySystemComponent() const
	{
		return RobotAbilitySystemComponent;
	}

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	/** Starts a move using InitialMoveTargetActor. */
	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Movement")
	bool StartMovingToInitialTarget();

	/** Replaces any active move with a move to NewTargetActor. */
	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Movement")
	bool StartMovingToActor(AActor* NewTargetActor);

	/** Cancels the active move. Cancellation returns the robot to Idle. */
	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Movement")
	void CancelCurrentMovement();

	/**
	 * Extension point for future obstruction, stun, EMP, and damage systems.
	 * Only Blocked, Interrupted, and Disabled are accepted here.
	 */
	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|Movement")
	bool RequestMovementInterruption(
		ETDRobotMovementState InterruptionState,
		ETDRobotMovementEndReason EndReason = ETDRobotMovementEndReason::Aborted);

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|Movement")
	AActor* GetCurrentMoveTargetActor() const { return CurrentMoveTargetActor; }

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|Movement")
	ETDRobotMovementState GetMovementState() const { return MovementState; }

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|Movement")
	ETDRobotMovementEndReason GetLastMovementEndReason() const { return LastMovementEndReason; }

	AActor* GetInitialMoveTargetActor() const { return InitialMoveTargetActor; }
	float GetTargetAcceptanceRadius() const { return TargetAcceptanceRadius; }
	bool ShouldAutoMoveOnBeginPlay() const { return bAutoMoveOnBeginPlay; }

	/** Emitted only when MovementState actually changes. */
	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|Movement")
	FTDRobotMovementStateChanged OnMovementStateChanged;

protected:
	/** Blueprint owns all optional damage, separation, presentation, and audio wiring. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Titan Defence|Robot|State",
		meta = (DisplayName = "On Robot State Ready"))
	void BP_OnRobotStateReady(
		UTDRobotStateComponent* InRobotState,
		UAbilitySystemComponent* InAbilitySystemComponent);

	virtual void BeginPlay() override;

	/** Actor to approach when automatic movement or StartMovingToInitialTarget is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan Defence|Robot|Movement")
	TObjectPtr<AActor> InitialMoveTargetActor = nullptr;

	/** Additional acceptable distance from the goal actor, in Unreal units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan Defence|Robot|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float TargetAcceptanceRadius = 100.0f;

	/** If enabled, the controller starts moving to InitialMoveTargetActor when it possesses this robot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Titan Defence|Robot|Movement")
	bool bAutoMoveOnBeginPlay = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Titan Defence|Robot|Movement")
	TObjectPtr<AActor> CurrentMoveTargetActor = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Titan Defence|Robot|Movement")
	ETDRobotMovementState MovementState = ETDRobotMovementState::Idle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Titan Defence|Robot|Movement")
	ETDRobotMovementEndReason LastMovementEndReason = ETDRobotMovementEndReason::None;

private:
	friend class ATDBaseRobotAIController;

	bool InitializeRobotStateSystem();

	ATDBaseRobotAIController* GetTDRobotController() const;
	void SetCurrentMoveTargetActor(AActor* NewTargetActor);
	void TransitionMovementState(
		ETDRobotMovementState NewState,
		ETDRobotMovementEndReason EndReason = ETDRobotMovementEndReason::None);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Titan Defence|Robot|State",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTDRobotAbilitySystemComponent> RobotAbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Titan Defence|Robot|State",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTDRobotAttributeSet> RobotAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Titan Defence|Robot|State",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTDRobotStateComponent> RobotStateComponent;

	bool bRobotStateReadyNotified = false;
};

