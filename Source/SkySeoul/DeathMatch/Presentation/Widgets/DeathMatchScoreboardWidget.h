#pragma once

#include "CoreMinimal.h"
#include "DeathMatchWidgetBase.h"
#include "DeathMatchScoreboardWidget.generated.h"

class UTextBlock;
class UPanelWidget;

/**
 * Scoreboard overlay (Tab etc.). C++ fills team rows; BP designs layout.
 * Optional: Text_Team0Score, Text_Team1Score, Text_Team0RoundScore, Text_Team1RoundScore.
 */
UCLASS(Abstract)
class SKYSEOUL_API UDeathMatchScoreboardWidget : public UDeathMatchWidgetBase
{
	GENERATED_BODY()

public:
	void SetScoreboardVisible(bool bVisible);

protected:
	virtual void NativeConstruct() override;
	virtual void HandleTeamScoreChanged(const FTeamScoreData& NewData) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UPanelWidget> Panel_Root;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Team0Score;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Team1Score;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Team0RoundScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Team1RoundScore;

	UFUNCTION(BlueprintImplementableEvent, Category = "DeathMatch|UI", meta = (DisplayName = "On Scoreboard Visibility Anim"))
	void BP_OnScoreboardVisibilityAnim(bool bVisible);
};
