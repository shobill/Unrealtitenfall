#pragma once

#include "CoreMinimal.h"
#include "DeathMatchWidgetBase.h"
#include "DeathMatchMatchHUDWidget.generated.h"

class UTextBlock;
class UPanelWidget;

/**
 * In-match always-on HUD: timer + team scores.
 * BP: place widgets named Text_RemainingTime, Text_Team0Score, Text_Team1Score,
 * Text_Team0RoundScore, Text_Team1RoundScore (all optional).
 */
UCLASS(Abstract)
class SKYSEOUL_API UDeathMatchMatchHUDWidget : public UDeathMatchWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void HandleRemainingTimeChanged(float NewRemainingTime) override;
	virtual void HandleTeamScoreChanged(const FTeamScoreData& NewData) override;
	virtual void HandleRoundStateChanged(EDeathMatchRoundState NewState) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_RemainingTime;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Team0Score;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Team1Score;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Team0RoundScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Team1RoundScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_RoundState;

	/** Optional root for combat HUD hide on MatchOver. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UPanelWidget> Panel_Combat;
};
