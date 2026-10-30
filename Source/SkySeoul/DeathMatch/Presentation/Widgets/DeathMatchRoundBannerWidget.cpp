#include "DeathMatchRoundBannerWidget.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"

void UDeathMatchRoundBannerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetBannerVisible(false);
}

void UDeathMatchRoundBannerWidget::SetBannerVisible(bool bVisible)
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

	BP_OnBannerVisibilityAnim(bVisible);
}

void UDeathMatchRoundBannerWidget::HandleRoundStateChanged(EDeathMatchRoundState NewState)
{
	if (Text_Banner)
	{
		switch (NewState)
		{
		case EDeathMatchRoundState::WaitingToStart:
			Text_Banner->SetText(FText::FromString(TEXT("Waiting To Start")));
			break;
		case EDeathMatchRoundState::RoundOver:
			Text_Banner->SetText(FText::FromString(TEXT("Round Over")));
			break;
		case EDeathMatchRoundState::InProgress:
			Text_Banner->SetText(FText::FromString(TEXT("Fight!")));
			break;
		case EDeathMatchRoundState::MatchOver:
			Text_Banner->SetText(FText::FromString(TEXT("Match Over")));
			break;
		default:
			break;
		}
	}

	Super::HandleRoundStateChanged(NewState);
}
