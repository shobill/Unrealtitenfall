#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeathMatch/AI/Combat/DMCombatDamageStimulus.h"
#include "DMCombatKnowledgeTypes.h"
#include "DMCombatKnowledgeComponent.generated.h"

class UAIModule_Sense;

/**
 * Passive combat evidence store. It mirrors perception and damage input, but never chooses
 * a behavior, runs EQS, starts fire, or issues movement.
 */
UCLASS(ClassGroup=(DeathMatch), meta=(BlueprintSpawnableComponent))
class SKYSEOUL_API UDMCombatKnowledgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDMCombatKnowledgeComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Combat Knowledge")
	void RefreshFromPerception();

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Combat Knowledge")
	void SetCombatTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Combat Knowledge")
	void ClearCombatTarget();

	void NotifyDamageReceived(const FDMCombatDamageStimulus& Stimulus);
	void ResetKnowledge();

	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Knowledge")
	AActor* GetCombatTarget() const { return CombatTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Knowledge")
	bool HasVisibleTarget() const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Knowledge")
	bool HasRememberedTarget() const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Knowledge")
	bool IsCombatTargetVisible() const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Knowledge")
	bool IsCombatTargetRemembered() const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Knowledge")
	FVector GetLastKnownThreatLocation() const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Knowledge")
	FVector GetDamageOrigin() const { return LastDamageOrigin; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Knowledge")
	FVector GetDamageDirection() const { return LastDamageDirection; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Combat Knowledge")
	EDMThreatAwareness GetThreatAwareness() const { return ThreatAwareness; }

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Combat Knowledge")
	void GetVisibleThreatActors(TArray<AActor*>& OutActors) const;

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Combat Knowledge")
	void GetRememberedThreatLocations(TArray<FVector>& OutLocations) const;

	const TArray<FDMRememberedThreat>& GetThreatRecords() const { return Threats; }
	float GetLastDamageTime() const { return LastDamageTime; }
	EDMDamageAwarenessType GetLastDamageAwarenessType() const { return LastDamageAwarenessType; }

private:
	UAIModule_Sense* ResolveSense() const;
	const FDMRememberedThreat* FindThreat(const AActor* Actor) const;
	FDMRememberedThreat* FindThreat(AActor* Actor);
	void UpdateThreatAwareness();

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CombatTarget;

	UPROPERTY(Transient)
	TArray<FDMRememberedThreat> Threats;

	UPROPERTY(Transient)
	FVector LastDamageOrigin = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector LastDamageDirection = FVector::ZeroVector;

	UPROPERTY(Transient)
	float LastDamageTime = -BIG_NUMBER;

	UPROPERTY(Transient)
	float TargetLostTime = -BIG_NUMBER;

	EDMDamageAwarenessType LastDamageAwarenessType = EDMDamageAwarenessType::Unknown;

	UPROPERTY(Transient)
	EDMThreatAwareness ThreatAwareness = EDMThreatAwareness::Disengaged;
};
