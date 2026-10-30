#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DMTacticalAreaSubsystem.generated.h"

class ADMTacticalAreaVolume;

/** Event-driven world registry for designer-authored tactical areas and team-relative danger. */
UCLASS(meta=(DisplayName="DeathMatch Tactical Areas"))
class SKYSEOUL_API UDMTacticalAreaSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	void RegisterVolume(ADMTacticalAreaVolume* Volume);
	void UnregisterVolume(ADMTacticalAreaVolume* Volume);
	void NotifyVolumeEntered(ADMTacticalAreaVolume* Volume, AActor* Actor);
	void NotifyVolumeExited(ADMTacticalAreaVolume* Volume, AActor* Actor);

	void GetRegisteredVolumes(TArray<ADMTacticalAreaVolume*>& OutVolumes, bool bActiveOnly = true) const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	ADMTacticalAreaVolume* FindVolumeAtLocation(const FVector& Location, bool bActiveOnly = true) const;

	/** Records a confirmed team-spawn location. NoTeam (255) and locations outside authored areas are ignored. */
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Area|Spawn")
	void RegisterSpawnTeamAtLocation(uint8 TeamId, const FVector& SpawnLocation);

	/** Re-scans authored PlayerStartByTeam actors. Safe to call after streamed volumes are registered. */
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Area|Spawn")
	void RefreshSpawnTeams();

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Area|Events")
	void ReportCharacterSpawn(AActor* Character, uint8 TeamId);

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Area|Events")
	void ReportCharacterDeath(AActor* Character, uint8 TeamId, float DangerAmount = 2.f);

	/** Lifecycle removal without adding death danger (for UnPossess, travel, teardown). */
	void RemoveCharacter(AActor* Character);

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Area|Events")
	void ReportCombatStart(AActor* Character, uint8 TeamId, float DangerAmount = 2.f);

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Area|Events")
	void ReportCombatEnd(AActor* Character);

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Area|Events")
	void ReportEnemySeen(uint8 ObserverTeamId, const FVector& Location, float DangerAmount = 3.f,
		AActor* Source = nullptr);

	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Area|Events")
	void ReportDamageReceived(uint8 VictimTeamId, const FVector& Location, float DangerAmount = 3.f,
		AActor* Victim = nullptr);

	/** API is intentionally inert in the first Roam version. */
	UFUNCTION(BlueprintCallable, Category="DeathMatch|Tactical Area|Events")
	void ReportSoundEvent(uint8 ObserverTeamId, const FVector& Location, float Strength,
		AActor* Source = nullptr);

	/** One reservation per AI. Re-reserving the same area is idempotent. */
	bool ReserveIncomingAI(AActor* AIActor, uint8 TeamId, ADMTacticalAreaVolume* Volume);
	void ReleaseIncomingAI(AActor* AIActor);
	ADMTacticalAreaVolume* GetReservedVolume(AActor* AIActor) const;
	bool IsRoamAreaOnCooldown(AActor* AIActor, ADMTacticalAreaVolume* Volume, float Now) const;
	void RecordRoamAreaFailure(AActor* AIActor, ADMTacticalAreaVolume* Volume, float Now, float CooldownSeconds);
	void ReportRoamAreaReached(AActor* AIActor, ADMTacticalAreaVolume* Volume);
	ADMTacticalAreaVolume* GetRecentRoamArea(AActor* AIActor) const;

private:
	struct FPresenceRecord
	{
		TWeakObjectPtr<ADMTacticalAreaVolume> Volume;
		uint8 TeamId = 255;
		bool bInCombat = false;
	};

	struct FReservationRecord
	{
		TWeakObjectPtr<ADMTacticalAreaVolume> Volume;
		uint8 TeamId = 255;
	};

	void SetPresence(AActor* Actor, uint8 TeamId, ADMTacticalAreaVolume* Volume);
	void ApplyDanger(uint8 TeamId, const FVector& Location, float Amount);
	void TrackActorDestruction(AActor* Actor);
	void StopTrackingActorDestruction(AActor* Actor);

	UFUNCTION()
	void HandleTrackedActorDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ADMTacticalAreaVolume>> RegisteredVolumes;

	TMap<TWeakObjectPtr<AActor>, FPresenceRecord> PresenceByActor;
	TMap<TWeakObjectPtr<AActor>, FReservationRecord> ReservationByActor;
	TMap<TWeakObjectPtr<AActor>, TMap<TWeakObjectPtr<ADMTacticalAreaVolume>, float>> RoamFailureCooldowns;
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<ADMTacticalAreaVolume>> RecentRoamAreaByActor;
};
