#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeathMatch/Core/DMHealthTypes.h"
#include "DMHealthBarsRuntimeComponent.generated.h"

class AActor;
class APlayerController;
class UDMHealthBarWidget;
class UWidgetComponent;

USTRUCT()
struct FDMHealthBarTargetEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> WidgetComponent = nullptr;

	UPROPERTY()
	FDMHealthSnapshot LastHealthSnapshot;

	UPROPERTY()
	float LastVisibleTime = -BIG_NUMBER;

	UPROPERTY()
	bool bCurrentlyDisplayed = false;

	UPROPERTY()
	float NextHealthUpdateTime = 0.0f;

	uint32 LastDisplayPass = 0;
};

namespace DMHealthBars
{
	SKYSEOUL_API FVector ResolveAnchorLocation(const AActor* Target, FName HealthBarSocketName, float HeightOffset);
	SKYSEOUL_API FVector ResolveLineOfSightLocation(const AActor* Target);
}

/** Neutral local-only widget pooling and target-entry lifetime. It owns no search or visibility policy. */
UCLASS(Abstract, BlueprintType, ClassGroup = (DeathMatch), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UDMHealthBarsRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDMHealthBarsRuntimeComponent();

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Health Bars")
	int32 GetActiveWidgetCount() const { return ActiveWidgetCount; }

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Health Bars")
	int32 GetPooledWidgetCount() const { return PooledWidgetCount; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void BeginDisplayPass();
	void ShowTarget(AActor* Target, const FDMHealthSnapshot& Snapshot, float Now, bool bRefreshLastVisibleTime = true);
	void ShowTargetUsingCachedSnapshot(AActor* Target, float Now, bool bRefreshLastVisibleTime = false);
	void EndDisplayPass(float Now, float EntryRetentionSeconds);
	void HideAllTargets();
	void RemoveInvalidEntries();

	FDMHealthBarTargetEntry* FindTargetEntry(const AActor* Target);
	const FDMHealthBarTargetEntry* FindTargetEntry(const AActor* Target) const;
	bool IsHealthRefreshDue(const AActor* Target, float Now) const;
	void SetNextHealthRefresh(AActor* Target, float NextUpdateTime);
	APlayerController* ResolveLocalViewerController() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Health Bars|Appearance")
	TSubclassOf<UDMHealthBarWidget> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Health Bars|Appearance")
	FIntPoint DrawSize = FIntPoint(140, 18);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Health Bars|Appearance")
	FVector2D Pivot = FVector2D(0.5f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Health Bars|Placement")
	FName HealthBarSocketName = TEXT("HealthBar");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Health Bars|Placement", meta = (Units = "cm"))
	float HeightOffset = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Health Bars|Appearance")
	FDMHealthBarDisplayOptions DisplayOptions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Health Bars|Diagnostics")
	int32 ActiveWidgetCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Health Bars|Diagnostics")
	int32 PooledWidgetCount = 0;

private:
	UWidgetComponent* AcquireWidgetComponent();
	void ReleaseWidgetComponent(FDMHealthBarTargetEntry& Entry);
	void RefreshWidget(FDMHealthBarTargetEntry& Entry, AActor* Target, const FDMHealthSnapshot& Snapshot);
	void UpdateWidgetLocation(FDMHealthBarTargetEntry& Entry) const;
	void UpdateDiagnosticCounts();

	UPROPERTY(Transient)
	TArray<FDMHealthBarTargetEntry> TargetEntries;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetComponent>> WidgetPool;

	uint32 DisplayPassSerial = 0;
};

