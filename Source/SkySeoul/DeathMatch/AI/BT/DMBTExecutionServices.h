#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "DeathMatch/AI/Execution/DMActionIntentTypes.h"
#include "DMBTExecutionServices.generated.h"

class UDMCombatFireControlComponent;

UENUM(BlueprintType)
enum class EDMFocusTargetSource : uint8
{
	CombatTarget,
	LastKnownTargetLocation,
	MoveGoal
};

/** Applies one high-level action intent for exactly the lifetime of its BT branch. */
UCLASS()
class SKYSEOUL_API UBTService_DM_ActionIntent : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_DM_ActionIntent();
	virtual uint16 GetInstanceMemorySize() const override;
	static FDMActionIntentExecutionSettings ChooseExecutionSettings(bool bHasFixedOverride,
		const FDMActionIntentExecutionSettings& FixedSettings, bool bUseBlackboardSettings,
		bool bBlackboardADS, bool bBlackboardMovingFire, bool& bOutHasOverride);

	UPROPERTY(EditAnywhere, Category="Action Intent")
	EDMActionIntent ActionIntent = EDMActionIntent::None;

	UPROPERTY(EditAnywhere, Category="Action Intent|Execution")
	bool bOverrideExecutionSettings = false;

	UPROPERTY(EditAnywhere, Category="Action Intent|Execution", meta=(EditCondition="bOverrideExecutionSettings", ToolTip="Allow ADS while the pawn is moving. ADS-required intents still aim after stopping when this is false."))
	bool bUseADSWhileMoving = false;

	UPROPERTY(EditAnywhere, Category="Action Intent|Execution", meta=(EditCondition="bOverrideExecutionSettings"))
	bool bAllowFireWhileMoving = false;

	UPROPERTY(EditAnywhere, Category="Action Intent|Execution|Blackboard", meta=(ToolTip="Used only when no explicit Service execution override is enabled."))
	bool bUseBlackboardExecutionSettings = false;

	UPROPERTY(EditAnywhere, Category="Action Intent|Execution|Blackboard", meta=(EditCondition="bUseBlackboardExecutionSettings"))
	FBlackboardKeySelector ShouldADSKey;

	UPROPERTY(EditAnywhere, Category="Action Intent|Execution|Blackboard", meta=(EditCondition="bUseBlackboardExecutionSettings"))
	FBlackboardKeySelector FireWhileMovingKey;

protected:
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

/** Maintains one owned focus request and releases only that request on branch abort/end. */
UCLASS()
class SKYSEOUL_API UBTService_DM_MaintainFocus : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_DM_MaintainFocus();
	virtual uint16 GetInstanceMemorySize() const override;

	UPROPERTY(EditAnywhere, Category="Focus")
	EDMFocusTargetSource FocusTarget = EDMFocusTargetSource::CombatTarget;

protected:
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

/** Keeps a fire request alive while the branch is active and guarantees release on abort/end. */
UCLASS()
class SKYSEOUL_API UBTService_DM_MaintainFire : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_DM_MaintainFire();
	virtual uint16 GetInstanceMemorySize() const override;
	/** Idempotent helpers shared by BT callbacks and lifecycle automation tests. */
	static void BeginOrRefreshOwnedRequest(UDMCombatFireControlComponent* Fire, uint64& RequestId);
	static void EndOwnedRequest(UDMCombatFireControlComponent* Fire, uint64& RequestId);

protected:
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};
