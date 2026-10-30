#pragma once

#include "CoreMinimal.h"
#include "AIModuleBase.h"
#include "DeathMatch/AIModules/DMPerceptionTypes.h"
#include "AIModule_Sense.generated.h"

class UAIPerceptionComponent;

/**
 * Perception gather (Phase 1):
 * AIPerception -> filter hostile/alive -> Seen/Known/Focus snapshot + query API.
 * Blackboard bridge is Phase 2.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UAIModule_Sense : public UAIModuleBase
{
    friend class FDMAdaptiveSafetyTest;
    friend class FDMArchitectureOwnershipTest;
	GENERATED_BODY()

public:
	UAIModule_Sense();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Controller lifecycle reset: a new pawn must not inherit the previous pawn's targets. */
    void ResetAwareness();

	UFUNCTION(BlueprintPure, Category = "AI|Sense")
	AActor* GetFocusTarget() const { return FocusTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "AI|Sense")
	FVector GetFocusLocation() const { return FocusLocation; }

	UFUNCTION(BlueprintPure, Category = "AI|Sense")
	bool HasHostile() const;

	UFUNCTION(BlueprintPure, Category = "AI|Sense")
	AActor* FindBestHostile(bool bPreferSeenOnly = true) const;

	UFUNCTION(BlueprintCallable, Category = "AI|Sense")
	void GetSeenHostiles(TArray<AActor*>& OutHostiles) const;

	UFUNCTION(BlueprintCallable, Category = "AI|Sense")
	void GetKnownHostiles(TArray<AActor*>& OutHostiles) const;

	UFUNCTION(BlueprintPure, Category = "AI|Sense")
	bool GetAwareTarget(AActor* Target, FDMAwareTarget& OutData) const;

	const TSet<TWeakObjectPtr<AActor>>& GetSeenTargets() const { return SeenTargets; }
	const TSet<TWeakObjectPtr<AActor>>& GetKnownTargets() const { return KnownTargets; }
    float GetHostileMemoryLifetime() const;

private:
    friend class FDMCoverFallbackTest;
	UFUNCTION()
	void OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors);

	void CleanupKnownTargets();
	void UpdateFocus(bool bForce);
	void UpsertAwareness(AActor* Target, bool bSensedNow, const FVector& SenseLocation);
	void RemoveAwareness(AActor* Target);
	void RefreshDistanceCaches();

	bool IsValidHostileTarget(AActor* Target) const;
	APawn* ResolveOwnerPawn() const;
	UAIPerceptionComponent* ResolvePerceptionComponent() const;
	float GetWorldTimeSeconds() const;

	UPROPERTY()
	TObjectPtr<UAIPerceptionComponent> CachedPerception;

	UPROPERTY(VisibleAnywhere, Category = "AI|Sense")
	TSet<TWeakObjectPtr<AActor>> SeenTargets;

	UPROPERTY(VisibleAnywhere, Category = "AI|Sense")
	TSet<TWeakObjectPtr<AActor>> KnownTargets;

	/** Runtime snapshot (not replicated). */
	TMap<TWeakObjectPtr<AActor>, FDMAwareTarget> AwarenessMap;

	UPROPERTY()
	TWeakObjectPtr<AActor> FocusTarget;

	UPROPERTY()
	FVector FocusLocation = FVector::ZeroVector;

	FTimerHandle CleanupTimer;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense")
	float CleanupInterval = 0.25f;

	/** Current focus distance is multiplied by this (<1 keeps focus stickier). */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Sense|Focus", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float FocusHoldDistanceScale = 0.85f;

};
