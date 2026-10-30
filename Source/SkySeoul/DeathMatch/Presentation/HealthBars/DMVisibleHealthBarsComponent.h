#pragma once

#include "CoreMinimal.h"
#include "DeathMatch/Presentation/HealthBars/DMHealthBarsRuntimeComponent.h"
#include "DMVisibleHealthBarsComponent.generated.h"

USTRUCT()
struct FDMVisibleHealthCandidate
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Target;
};

/** Player-facing health display. Shows only screen-visible and policy-authorized targets. */
UCLASS(BlueprintType, ClassGroup = (DeathMatch), meta = (BlueprintSpawnableComponent, ToolTip = "Player-facing health display. Shows only screen-visible and policy-authorized targets."))
class SKYSEOUL_API UDMVisibleHealthBarsComponent : public UDMHealthBarsRuntimeComponent
{
	GENERATED_BODY()

public:
	UDMVisibleHealthBarsComponent();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Visible Health Bars")
	void SetPublicHealthProvider(UObject* Provider);

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Visible Health Bars")
	void SetAuthorizationProvider(UObject* Provider);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars", meta = (ClampMin = "0.05", Units = "s"))
	float ScanInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars", meta = (ClampMin = "0.02", Units = "s"))
	float HealthUpdateInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars", meta = (ClampMin = "100.0", Units = "cm"))
	float MaximumDisplayDistance = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Visibility")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Visibility", meta = (ClampMin = "0.02", Units = "s"))
	float VisibilityCheckInterval = 0.1f;

	/** LOS-only flicker grace. Distance, behind-camera, off-screen, and policy failure hide immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Visibility", meta = (ClampMin = "0.0", ClampMax = "0.5", Units = "s"))
	float VisibilityGraceSeconds = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Policy")
	bool bShowSelf = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Policy")
	bool bShowFriendlies = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Policy")
	bool bShowHostiles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Policy")
	bool bShowNeutral = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Policy")
	bool bShowDead = false;

	/** If true, candidates are denied when no object implementing the authorization interface is available. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Policy")
	bool bRequireAuthorizationProvider = false;

	/** Development fallback only: raw Resolver access is limited in code to Standalone/authority, never a remote network client enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Public Health")
	bool bAllowStandaloneOrAuthorityResolverFallback = true;

	/** A network client may read its own pawn when no public provider exists. This never authorizes another pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Public Health")
	bool bAllowRawSelfHealthFallback = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Public Health", meta = (MustImplement = "/Script/SkySeoul.DMPublicHealthProvider"))
	TObjectPtr<UObject> PublicHealthProvider;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "DeathMatch|Visible Health Bars|Policy", meta = (MustImplement = "/Script/SkySeoul.DMHealthBarAuthorizationProvider"))
	TObjectPtr<UObject> AuthorizationProvider;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Visible Health Bars|Diagnostics")
	int32 CandidateCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Visible Health Bars|Diagnostics")
	int32 OnScreenCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Visible Health Bars|Diagnostics")
	int32 LOSVisibleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DeathMatch|Visible Health Bars|Diagnostics")
	int32 AuthorizedHealthCount = 0;

private:
	void ScanCandidates(const APlayerController* Viewer);
	void EvaluateCandidates(APlayerController* Viewer, float Now);
	bool CanDisplayCandidate(const APlayerController* Viewer, const AActor* Target) const;
	bool CanRevealCandidate(const APlayerController* Viewer, const AActor* Target) const;
	bool IsOnScreen(APlayerController* Viewer, const AActor* Target, const FVector& CameraLocation, const FVector& CameraForward) const;
	bool HasLineOfSight(const APlayerController* Viewer, const AActor* Target, const FVector& CameraLocation) const;
	bool ResolveAuthorizedSnapshot(const APlayerController* Viewer, const AActor* Target, FDMHealthSnapshot& OutSnapshot) const;
	UObject* ResolvePublicHealthProvider(const APlayerController* Viewer) const;
	UObject* ResolveAuthorizationProvider(const APlayerController* Viewer) const;
	static UObject* FindInterfaceProvider(const APlayerController* Viewer, const AActor* Owner, UClass* InterfaceClass);

	UPROPERTY(Transient)
	TArray<FDMVisibleHealthCandidate> Candidates;

	float NextScanTime = 0.0f;
	float NextVisibilityCheckTime = 0.0f;
};

