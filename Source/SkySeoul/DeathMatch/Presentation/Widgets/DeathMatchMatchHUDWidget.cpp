#include "DeathMatchMatchHUDWidget.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"

namespace DeathMatchMatchHUDPrivate
{
	static void SetTextInt(UTextBlock* TextBlock, int32 Value)
	{
		if (TextBlock)
		{
			TextBlock->SetText(FText::AsNumber(Value));
		}
	}

	static FText FormatRemainingTime(float RemainingSeconds)
	{
		const int32 TotalSeconds = FMath::Max(0, FMath::CeilToInt(RemainingSeconds));
		const int32 Minutes = TotalSeconds / 60;
		const int32 Seconds = TotalSeconds % 60;
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), Minutes, Seconds));
	}

	static FText RoundStateToText(EDeathMatchRoundState State)
	{
		switch (State)
		{
		case EDeathMatchRoundState::WaitingToStart: return FText::FromString(TEXT("Waiting"));
		case EDeathMatchRoundState::InProgress:     return FText::FromString(TEXT("In Progress"));
		case EDeathMatchRoundState::RoundOver:      return FText::FromString(TEXT("Round Over"));
		case EDeathMatchRoundState::MatchOver:      return FText::FromString(TEXT("Match Over"));
		default:                                    return FText::GetEmpty();
		}
	}
}

void UDeathMatchMatchHUDWidget::HandleRemainingTimeChanged(float NewRemainingTime)
{
	if (Text_RemainingTime)
	{
		Text_RemainingTime->SetText(DeathMatchMatchHUDPrivate::FormatRemainingTime(NewRemainingTime));
	}

	Super::HandleRemainingTimeChanged(NewRemainingTime);
}

void UDeathMatchMatchHUDWidget::HandleTeamScoreChanged(const FTeamScoreData& NewData)
{
	if (NewData.TeamId == 0)
	{
		DeathMatchMatchHUDPrivate::SetTextInt(Text_Team0Score, NewData.Score);
		DeathMatchMatchHUDPrivate::SetTextInt(Text_Team0RoundScore, NewData.RoundScore);
	}
	else if (NewData.TeamId == 1)
	{
		DeathMatchMatchHUDPrivate::SetTextInt(Text_Team1Score, NewData.Score);
		DeathMatchMatchHUDPrivate::SetTextInt(Text_Team1RoundScore, NewData.RoundScore);
	}

	Super::HandleTeamScoreChanged(NewData);
}

void UDeathMatchMatchHUDWidget::HandleRoundStateChanged(EDeathMatchRoundState NewState)
{
	if (Text_RoundState)
	{
		Text_RoundState->SetText(DeathMatchMatchHUDPrivate::RoundStateToText(NewState));
	}

	if (Panel_Combat)
	{
		const bool bShowCombat = (NewState != EDeathMatchRoundState::MatchOver);
		Panel_Combat->SetVisibility(bShowCombat ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	Super::HandleRoundStateChanged(NewState);
}
