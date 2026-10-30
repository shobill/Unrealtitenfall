#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeathMatch/GameMode/GS_DeathMatch.h"
#include "DeathMatchWidgetBase.generated.h"

/**
 * DeathMatch UI base: binds to AGS_DeathMatch delegates.
 * Blueprint children only style BindWidget slots and implement anim events.
 */
UCLASS(Abstract)
class SKYSEOUL_API UDeathMatchWidgetBase : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	AGS_DeathMatch* GetDeathMatchGameState() const;

	/** Pull current GS values into the widget. */
	virtual void RefreshFromGameState();

	virtual void HandleRemainingTimeChanged(float NewRemainingTime);
	virtual void HandleRoundStateChanged(EDeathMatchRoundState NewState);
	virtual void HandleTeamScoreChanged(const FTeamScoreData& NewData);

	UFUNCTION(BlueprintImplementableEvent, Category = "DeathMatch|UI", meta = (DisplayName = "On Remaining Time Anim"))
	void BP_OnRemainingTimeAnim(float NewRemainingTime);

	UFUNCTION(BlueprintImplementableEvent, Category = "DeathMatch|UI", meta = (DisplayName = "On Round State Anim"))
	void BP_OnRoundStateAnim(EDeathMatchRoundState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "DeathMatch|UI", meta = (DisplayName = "On Team Score Anim"))
	void BP_OnTeamScoreAnim(const FTeamScoreData& NewData);

private:
	void TryBindGameState();
	void UnbindGameState();

	UFUNCTION()
	void OnRemainingTimeChanged(float NewRemainingTime);

	UFUNCTION()
	void OnRoundStateChanged(EDeathMatchRoundState NewState);

	UFUNCTION()
	void OnTeamScoreChanged(const FTeamScoreData& NewData);

	UPROPERTY()
	TWeakObjectPtr<AGS_DeathMatch> CachedGameState;

	UPROPERTY()
	FTimerHandle GameStateBindRetryHandle;

	bool bBoundToGameState = false;
};
