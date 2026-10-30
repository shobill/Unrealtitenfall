#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTDecorator.h"
#include "DMBTDataNodes.generated.h"

UCLASS()
class SKYSEOUL_API UBTService_DM_UpdateAIState : public UBTService
{
	GENERATED_BODY()
public:
	UBTService_DM_UpdateAIState();
protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

UCLASS()
class SKYSEOUL_API UBTService_DM_UpdateCombatBlackboard : public UBTService
{
	GENERATED_BODY()
public:
	UBTService_DM_UpdateCombatBlackboard();
protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

UCLASS()
class SKYSEOUL_API UBTService_DM_UpdateHealthRisk : public UBTService
{
	GENERATED_BODY()
public:
	UBTService_DM_UpdateHealthRisk();
protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

UCLASS()
class SKYSEOUL_API UBTService_DM_UpdateFireState : public UBTService
{
	GENERATED_BODY()
public:
	UBTService_DM_UpdateFireState();
protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

/** Periodically re-evaluates a Root-state condition and actively aborts affected branches. */
UCLASS(Abstract)
class SKYSEOUL_API UBTDecorator_DM_ReactiveStateCondition : public UBTDecorator
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_ReactiveStateCondition();

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Condition", meta=(ClampMin="0.02", Units="s"))
	float ReevaluationInterval = 0.1f;

protected:
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

UCLASS()
class SKYSEOUL_API UBTDecorator_DM_HasVisibleTarget : public UBTDecorator_DM_ReactiveStateCondition
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_HasVisibleTarget();

	/** Prevents a single missed perception refresh from bouncing Combat to LostTarget. */
	UPROPERTY(EditAnywhere, Category="Condition", meta=(ClampMin="0.0", Units="s"))
	float ExitGracePeriod = 0.2f;
protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

private:
	mutable double LastVisibleTime = -BIG_NUMBER;
};

UCLASS()
class SKYSEOUL_API UBTDecorator_DM_HasRememberedTarget : public UBTDecorator_DM_ReactiveStateCondition
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_HasRememberedTarget();
protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};

UCLASS()
class SKYSEOUL_API UBTDecorator_DM_HasValidCombatTarget : public UBTDecorator_DM_ReactiveStateCondition
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_HasValidCombatTarget();
protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};

UCLASS()
class SKYSEOUL_API UBTDecorator_DM_IsDead : public UBTDecorator_DM_ReactiveStateCondition
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_IsDead();
protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};

UCLASS()
class SKYSEOUL_API UBTDecorator_DM_IsEmergency : public UBTDecorator_DM_ReactiveStateCondition
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_IsEmergency();
protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};

UCLASS()
class SKYSEOUL_API UBTDecorator_DM_ShouldRetreat : public UBTDecorator_DM_ReactiveStateCondition
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_ShouldRetreat();
protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};

UCLASS()
class SKYSEOUL_API UBTDecorator_DM_NeedsCombatCover : public UBTDecorator_DM_ReactiveStateCondition
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_NeedsCombatCover();
protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};

UCLASS()
class SKYSEOUL_API UBTDecorator_DM_CanFire : public UBTDecorator_DM_ReactiveStateCondition
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_CanFire();
protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};

/** Stable Attack/Approach branch condition backed exclusively by Fire Control's clear-shot contract. */
UCLASS(meta=(DisplayName="DM Has Clear Shot"))
class SKYSEOUL_API UBTDecorator_DM_HasClearShot : public UBTDecorator
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_HasClearShot();

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Condition", meta=(ClampMin="0.02", Units="s"))
	float ReevaluationInterval = 0.1f;

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

UCLASS()
class SKYSEOUL_API UBTDecorator_DM_ShouldInvestigate : public UBTDecorator_DM_ReactiveStateCondition
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_ShouldInvestigate();
protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};

UCLASS()
class SKYSEOUL_API UBTDecorator_DM_ShouldDisengage : public UBTDecorator_DM_ReactiveStateCondition
{
	GENERATED_BODY()
public:
	UBTDecorator_DM_ShouldDisengage();
protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
};
