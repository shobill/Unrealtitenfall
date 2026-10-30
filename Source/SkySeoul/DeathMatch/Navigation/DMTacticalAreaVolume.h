#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "DeathMatch/Navigation/DMTacticalAreaTypes.h"
#include "GameFramework/Actor.h"
#include "DMTacticalAreaVolume.generated.h"

class UDMTacticalAreaSubsystem;

/**
 * Designer-placed coarse Roam destination. The volume itself is the target unit; it has no
 * internal cells, grid, visibility analysis, or combat-position policy.
 */
UCLASS(Blueprintable, meta=(DisplayName="DeathMatch Tactical Area Volume"))
class SKYSEOUL_API ADMTacticalAreaVolume : public AActor
{
	GENERATED_BODY()

public:
	ADMTacticalAreaVolume();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	UBoxComponent* GetAreaBounds() const { return AreaBounds; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	FName GetAreaId() const { return AreaId.IsNone() ? GetFName() : AreaId; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	FString GetAreaDisplayName() const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	bool IsAreaEnabled() const { return bEnabled; }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	bool IsPointInsideVolume(const FVector& Point) const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	FVector GetRepresentativeLocation() const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	float GetSearchRadius() const;

	FVector GetScaledAreaExtent() const;
	FVector GetWorldBoundsExtent() const;

	/** Persistent danger derived from hostile team spawn ownership. */
	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area|Danger")
	float GetTeamBaseDanger(uint8 TeamId) const;

	/** Runtime event danger (seen, damage, combat, death), clamped to the shared 0..10 scale. */
	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area|Danger")
	float GetTeamDynamicDanger(uint8 TeamId) const;

	/** Compatibility accessor: Clamp(Max(BaseDanger, DynamicDanger), 0, 10). */
	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area|Danger")
	float GetTeamDanger(uint8 TeamId) const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area|Spawn")
	bool HasSpawnTeam(uint8 TeamId) const { return TeamId != 255 && SpawnTeams.Contains(TeamId); }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	int32 GetFriendlyCount(uint8 TeamId) const { return TeamOccupancy.FindRef(TeamId); }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	int32 GetEnemyCount(uint8 TeamId) const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	int32 GetIncomingAICount(uint8 TeamId) const { return TeamIncomingAI.FindRef(TeamId); }

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	int32 GetTotalIncomingAICount() const;

	UFUNCTION(BlueprintPure, Category="DeathMatch|Tactical Area")
	int32 GetCombatParticipantCount() const { return CurrentCombatParticipantCount; }

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="DeathMatch|Tactical Area",
		meta=(ToolTip="Stable map-unique identifier. If None, the actor name is used."))
	FName AreaId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Area",
		meta=(ToolTip="Disabled areas are neither Roam candidates nor valid active targets."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Area",
		meta=(ToolTip="Reserved for future tactical-area policies. The current Roam score does not use Area Type."))
	EDMTacticalAreaType AreaType = EDMTacticalAreaType::General;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Area|Debug",
		meta=(ToolTip="Optional short label used by Roam diagnostics."))
	FText DebugDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DeathMatch|Tactical Area|Debug")
	FLinearColor DebugColor = FLinearColor(0.1f, 0.75f, 1.0f, 1.0f);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="DeathMatch|Tactical Area|Runtime|Spawn",
		meta=(ToolTip="Teams whose authored or confirmed DeathMatch spawn lies inside this area. NoTeam (255) is never stored."))
	TSet<uint8> SpawnTeams;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="DeathMatch|Tactical Area|Runtime|Danger",
		meta=(ToolTip="Event-driven danger per observing team. Values are always clamped to the shared 0..10 danger range."))
	TMap<uint8, float> TeamDynamicDanger;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="DeathMatch|Tactical Area|Runtime")
	TMap<uint8, int32> TeamOccupancy;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="DeathMatch|Tactical Area|Runtime")
	TMap<uint8, int32> TeamIncomingAI;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="DeathMatch|Tactical Area|Runtime")
	int32 CurrentCombatParticipantCount = 0;

private:
	friend class UDMTacticalAreaSubsystem;
	friend class FDMTacticalAreaOverlapTest;
	friend class FDMTacticalAreaDangerTest;

	UFUNCTION()
	void HandleAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	bool IsTrackedPawn(const AActor* Actor) const;

	void RegisterSpawnTeam(uint8 TeamId);
	void AddDanger(uint8 TeamId, float Amount);
	void AdjustOccupancy(uint8 TeamId, int32 Delta);
	void AdjustIncoming(uint8 TeamId, int32 Delta);
	void AdjustCombatParticipants(int32 Delta);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="DeathMatch|Tactical Area",
		meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> AreaBounds;

	TMap<TWeakObjectPtr<AActor>, TSet<TWeakObjectPtr<UPrimitiveComponent>>> OverlappingComponentsByActor;
};
