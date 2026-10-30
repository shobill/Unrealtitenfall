#pragma once

#include "CoreMinimal.h"
#include "DeathMatchWidgetBase.h"
#include "DeathMatchRoundBannerWidget.generated.h"

class UTextBlock;
class UPanelWidget;

/**
 * Short RoundOver / Waiting banner. HUD owns show/hide by RoundState.
 */
UCLASS(Abstract)
class SKYSEOUL_API UDeathMatchRoundBannerWidget : public UDeathMatchWidgetBase
{
	GENERATED_BODY()

public:
	void SetBannerVisible(bool bVisible);

protected:
	virtual void NativeConstruct() override;
	virtual void HandleRoundStateChanged(EDeathMatchRoundState NewState) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UPanelWidget> Panel_Root;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "DeathMatch|UI")
	TObjectPtr<UTextBlock> Text_Banner;

	UFUNCTION(BlueprintImplementableEvent, Category = "DeathMatch|UI", meta = (DisplayName = "On Banner Visibility Anim"))
	void BP_OnBannerVisibilityAnim(bool bVisible);
};
