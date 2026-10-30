#pragma once

#include "CoreMinimal.h"
#include "DeathMatchWidgetBase.h"
#include "DeathMatchMatchResultWidget.generated.h"

class UTextBlock;
class UPanelWidget;

/**
 * MatchOver result screen. HUD shows this when RoundState == MatchOver.
 */
UCLASS(Abstract)
class SKYSEOUL_API UDeathMatchMatchResultWidget : public UDeathMatchWidgetBase
{
	GENERATED_BODY()

public:
	void SetResultVisible(bool bVisible);

protected:
	virtual void NativeConstruct() override;
	virtual void HandleRoundStateChanged(EDeathMatchRoundState NewState) override;
	virtual void HandleTeamScoreChanged(const FTeamScoreData& NewData) override;

	void RefreshResultTexts();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UPanelWidget> Panel_Root;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Winner;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Team0RoundScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Team1RoundScore;

	UFUNCTION(BlueprintImplementableEvent, Category = "DeathMatch|UI", meta = (DisplayName = "On Result Visibility Anim"))
	void BP_OnResultVisibilityAnim(bool bVisible);
};
