#include "DeathMatchScoreboardWidget.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"

void UDeathMatchScoreboardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetScoreboardVisible(false);
}

void UDeathMatchScoreboardWidget::SetScoreboardVisible(bool bVisible)
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

	BP_OnScoreboardVisibilityAnim(bVisible);
}

void UDeathMatchScoreboardWidget::HandleTeamScoreChanged(const FTeamScoreData& NewData)
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
		SetTextInt(Text_Team0Score, NewData.Score);
		SetTextInt(Text_Team0RoundScore, NewData.RoundScore);
	}
	else if (NewData.TeamId == 1)
	{
		SetTextInt(Text_Team1Score, NewData.Score);
		SetTextInt(Text_Team1RoundScore, NewData.RoundScore);
	}

	Super::HandleTeamScoreChanged(NewData);
}
