#pragma once

#include "CoreMinimal.h"
#include "DeathMatch/Presentation/HealthBars/DMHealthBarsRuntimeComponent.h"
#include "DMDebugAllHealthBarsComponent.generated.h"

USTRUCT()
struct FDMDebugHealthCandidate
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Target;

	UPROPERTY()
	FDMHealthSnapshot Snapshot;

	float NextHealthUpdateTime = 0.0f;
};

/** Developer-only X-ray health display. Must not be used for player-facing UI. */
UCLASS(BlueprintType, ClassGroup = (DeathMatchDebug), meta = (BlueprintSpawnableComponent, ToolTip = "Developer-only X-ray health display. Must not be used for player-facing UI."))
class SKYSEOUL_API UDMDebugAllHealthBarsComponent : public UDMHealthBarsRuntimeComponent
{
	GENERATED_BODY()

public:
	UDMDebugAllHealthBarsComponent();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Debug Health Bars")
	void SetDebugHealthBarsEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Debug Health Bars")
	bool AreDebugHealthBarsEnabled() const;

	UFUNCTION(BlueprintPure, Category = "DeathMatch|Debug Health Bars")
	static bool IsDebugFeatureAvailable();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Health Bars")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Health Bars", meta = (ClampMin = "0.05", Units = "s"))
	float ScanInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Health Bars", meta = (ClampMin = "0.02", Units = "s"))
	float HealthUpdateInterval = 0.1f;

	/** Zero means unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Health Bars", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumDisplayDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Health Bars")
	bool bShowSelf = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Debug Health Bars")
	bool bShowDead = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Debug Health Bars|Diagnostics")
	int32 TrackedTargetCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Debug Health Bars|Diagnostics")
	int32 InvalidHealthCount = 0;

private:
	void ScanCharacters(float Now);
	bool ShouldIncludeTarget(const AActor* Target, const APlayerController* Viewer) const;

	UPROPERTY(Transient)
	TArray<FDMDebugHealthCandidate> Candidates;

	float NextScanTime = 0.0f;
	int32 InvalidFromLastScan = 0;
};

