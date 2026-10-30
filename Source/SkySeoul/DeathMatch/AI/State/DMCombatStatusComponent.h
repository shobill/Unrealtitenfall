#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMCombatStatusTypes.h"
#include "DMCombatStatusComponent.generated.h"

/** Authoritative cached combat/health facts consumed read-only by Behavior Tree nodes. */
UCLASS(ClassGroup=(DeathMatch), meta=(BlueprintSpawnableComponent))
class SKYSEOUL_API UDMCombatStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDMCombatStatusComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void RefreshStatus();
	void ResetStatus();

	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Status") bool IsDead() const { return bDead; }
	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Status") float GetHealthRatio() const { return HealthRatio; }
	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Status") EDMHealthRiskTier GetRiskTier() const { return RiskTier; }
	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Status") EDMCombatAIState GetAIState() const { return AIState; }
	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Status") bool IsEmergency() const { return bEmergency; }
	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Status") bool ShouldRetreat() const { return bShouldRetreat; }
	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Status") bool NeedsCombatCover() const { return bNeedsCombatCover; }
	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Status") bool ShouldInvestigate() const { return bShouldInvestigate; }
	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Status") bool ShouldDisengage() const { return bShouldDisengage; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DeathMatch|Combat Status", meta=(ClampMin="0", ClampMax="1"))
	float EmergencyHealthRatio = .3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DeathMatch|Combat Status", meta=(ClampMin="0", ClampMax="1"))
	float RetreatHealthRatio = .5f;

private:
	UPROPERTY(Transient) float HealthRatio = 1.f;
	UPROPERTY(Transient) bool bDead = false;
	UPROPERTY(Transient) EDMHealthRiskTier RiskTier = EDMHealthRiskTier::Safe;
	UPROPERTY(Transient) EDMCombatAIState AIState = EDMCombatAIState::Idle;
	UPROPERTY(Transient) bool bEmergency = false;
	UPROPERTY(Transient) bool bShouldRetreat = false;
	UPROPERTY(Transient) bool bNeedsCombatCover = false;
	UPROPERTY(Transient) bool bShouldInvestigate = false;
	UPROPERTY(Transient) bool bShouldDisengage = true;
};
