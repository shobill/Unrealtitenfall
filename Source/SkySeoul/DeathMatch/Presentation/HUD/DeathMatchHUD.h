#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DeathMatch/GameMode/GS_DeathMatch.h"
#include "DeathMatchHUD.generated.h"

class UDeathMatchMatchHUDWidget;
class UDeathMatchScoreboardWidget;
class UDeathMatchRoundBannerWidget;
class UDeathMatchMatchResultWidget;

/**
 * DeathMatch UI orchestrator.
 * Assign BP widget subclasses on BP_DeathMatchHUD; BP only designs those widgets.
 */
UCLASS()
class SKYSEOUL_API ADeathMatchHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADeathMatchHUD();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|UI")
	void ToggleScoreboard();

	UFUNCTION(BlueprintCallable, Category = "DeathMatch|UI")
	void SetScoreboardVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "DeathMatch|UI")
	bool IsScoreboardVisible() const { return bScoreboardVisible; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawHUD() override;
	void DrawAICombatDebug();

	/** C++ crosshair (works without BP). Hide when using a widget crosshair. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI|Crosshair")
	bool bDrawCrosshair = true;

	/** Only draw while DeathMatchPlayer AimZoom IsAiming (RMB hold). */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI|Crosshair", meta = (EditCondition = "bDrawCrosshair"))
	bool bCrosshairOnlyWhileAiming = true;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI|Crosshair", meta = (EditCondition = "bDrawCrosshair", ClampMin = "1.0"))
	float CrosshairSize = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI|Crosshair", meta = (EditCondition = "bDrawCrosshair", ClampMin = "0.0"))
	float CrosshairGap = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI|Crosshair", meta = (EditCondition = "bDrawCrosshair", ClampMin = "1.0"))
	float CrosshairThickness = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI|Crosshair", meta = (EditCondition = "bDrawCrosshair"))
	FLinearColor CrosshairColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.85f);

	/** Scale gap while ADS (AimZoom alpha). */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI|Crosshair", meta = (EditCondition = "bDrawCrosshair", ClampMin = "0.0", ClampMax = "1.0"))
	float CrosshairAimGapScale = 0.35f;

	/**
	 * C++ canvas overlay: remaining time + team kill scores (works without MatchHUD BP).
	 * Turn off when a BP MatchHUD widget is fully wired.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI|CombatHUD")
	bool bDrawCombatHUD = true;

	/** If true, skip canvas combat HUD when MatchHUDWidget exists. Default false so empty BP class slots still show C++ HUD. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI|CombatHUD", meta = (EditCondition = "bDrawCombatHUD"))
	bool bHideCombatHUDWhenMatchWidgetPresent = false;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI|CombatHUD", meta = (EditCondition = "bDrawCombatHUD"))
	FLinearColor CombatHUDTextColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI")
	TSubclassOf<UDeathMatchMatchHUDWidget> MatchHUDClass;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI")
	TSubclassOf<UDeathMatchScoreboardWidget> ScoreboardClass;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI")
	TSubclassOf<UDeathMatchRoundBannerWidget> RoundBannerClass;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|UI")
	TSubclassOf<UDeathMatchMatchResultWidget> MatchResultClass;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|UI")
	TObjectPtr<UDeathMatchMatchHUDWidget> MatchHUDWidget;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|UI")
	TObjectPtr<UDeathMatchScoreboardWidget> ScoreboardWidget;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|UI")
	TObjectPtr<UDeathMatchRoundBannerWidget> RoundBannerWidget;

	UPROPERTY(BlueprintReadOnly, Category = "DeathMatch|UI")
	TObjectPtr<UDeathMatchMatchResultWidget> MatchResultWidget;

private:
	void CreateWidgets();
	void TryBindGameState();
	void UnbindGameState();
	void ApplyRoundState(EDeathMatchRoundState NewState);
	void DrawCombatHUDOverlay();
	void DrawCrosshairHUD();
	AGS_DeathMatch* ResolveGameState() const;

	UFUNCTION()
	void OnRoundStateChanged(EDeathMatchRoundState NewState);

	UPROPERTY()
	TWeakObjectPtr<AGS_DeathMatch> CachedGameState;

	UPROPERTY()
	FTimerHandle GameStateBindRetryHandle;

	bool bBoundToGameState = false;
	bool bScoreboardVisible = false;
};
