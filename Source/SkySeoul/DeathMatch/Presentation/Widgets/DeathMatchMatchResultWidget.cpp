#include "DeathMatchMatchResultWidget.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"

void UDeathMatchMatchResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetResultVisible(false);
}

void UDeathMatchMatchResultWidget::SetResultVisible(bool bVisible)
{
	const ESlateVisibility NewVisibility = bVisible
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;

	if (Panel_Root)
	{
		Panel_Root->SetVisibility(NewVisibility);
	}
	else
	{
		SetVisibility(NewVisibility);
	}

	if (bVisible)
	{
		RefreshResultTexts();
	}

	BP_OnResultVisibilityAnim(bVisible);
}

void UDeathMatchMatchResultWidget::HandleRoundStateChanged(EDeathMatchRoundState NewState)
{
	if (NewState == EDeathMatchRoundState::MatchOver)
	{
		RefreshResultTexts();
	}

	Super::HandleRoundStateChanged(NewState);
}

void UDeathMatchMatchResultWidget::HandleTeamScoreChanged(const FTeamScoreData& NewData)
{
	auto SetTextInt = [](UTextBlock* TextBlock, int32 Value)
	{
		if (TextBlock)
		{
			TextBlock->SetText(FText::AsNumber(Value));
		}
	};

	if (NewData.TeamId == 0)
	{
		SetTextInt(Text_Team0RoundScore, NewData.RoundScore);
	}
	else if (NewData.TeamId == 1)
	{
		SetTextInt(Text_Team1RoundScore, NewData.RoundScore);
	}

	Super::HandleTeamScoreChanged(NewData);
}

void UDeathMatchMatchResultWidget::RefreshResultTexts()
{
	AGS_DeathMatch* GS = GetDeathMatchGameState();
	if (!GS)
	{
		return;
	}

	auto SetTextInt = [](UTextBlock* TextBlock, int32 Value)
	{
		if (TextBlock)
		{
			TextBlock->SetText(FText::AsNumber(Value));
		}
	};

	for (const FTeamScoreData& Pair : GS->GetTeamScores())
	{
		if (Pair.TeamId == 0)
		{
			SetTextInt(Text_Team0RoundScore, Pair.RoundScore);
		}
		else if (Pair.TeamId == 1)
		{
			SetTextInt(Text_Team1RoundScore, Pair.RoundScore);
		}
	}

	if (!Text_Winner)
	{
		return;
	}

	const uint8 LeadingTeamId = GS->GetLeadingTeamIdByRoundScore();
	if (LeadingTeamId == 255)
	{
		Text_Winner->SetText(FText::FromString(TEXT("Draw")));
	}
	else
	{
		Text_Winner->SetText(FText::FromString(FString::Printf(TEXT("Team %d Wins"), LeadingTeamId)));
	}
}
