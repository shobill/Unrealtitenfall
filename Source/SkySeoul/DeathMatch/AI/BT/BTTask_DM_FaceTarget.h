#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_DM_FaceTarget.generated.h"

/** Rotate pawn yaw toward TargetActor (or TargetLocation). */
UCLASS()
class SKYSEOUL_API UBTTask_DM_FaceTarget : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_DM_FaceTarget();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetLocationKey;

	/** Degrees; if already within this, succeed without turning. */
	UPROPERTY(EditAnywhere, Category = "DeathMatch|Face", meta = (ClampMin = "0.0"))
	float AcceptanceAngleDegrees = 10.0f;

	UPROPERTY(EditAnywhere, Category = "DeathMatch|Face", meta = (ClampMin = "0.0"))
	float RotationSpeedDegrees = 360.0f;

	/** If true, instant snap yaw. If false, one-frame interp toward target (BT may re-run). */
	UPROPERTY(EditAnywhere, Category = "DeathMatch|Face")
	bool bInstantFace = true;
};
