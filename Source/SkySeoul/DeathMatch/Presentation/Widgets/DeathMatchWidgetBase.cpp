#include "DeathMatchWidgetBase.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UDeathMatchWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	TryBindGameState();
}

void UDeathMatchWidgetBase::NativeDestruct()
{
	UnbindGameState();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GameStateBindRetryHandle);
	}

	Super::NativeDestruct();
}

AGS_DeathMatch* UDeathMatchWidgetBase::GetDeathMatchGameState() const
{
	if (CachedGameState.IsValid())
	{
		return CachedGameState.Get();
	}

	if (const UWorld* World = GetWorld())
	{
		return World->GetGameState<AGS_DeathMatch>();
	}

	return nullptr;
}

void UDeathMatchWidgetBase::TryBindGameState()
{
	if (bBoundToGameState)
	{
		return;
	}

	AGS_DeathMatch* GS = GetDeathMatchGameState();
	if (!GS)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				GameStateBindRetryHandle,
				this,
				&UDeathMatchWidgetBase::TryBindGameState,
				0.1f,
				false
			);
		}
		return;
	}

	CachedGameState = GS;
	GS->OnRemainingTimeChanged.AddDynamic(this, &UDeathMatchWidgetBase::OnRemainingTimeChanged);
	GS->OnRoundStateChanged.AddDynamic(this, &UDeathMatchWidgetBase::OnRoundStateChanged);
	GS->OnTeamScoreChanged.AddDynamic(this, &UDeathMatchWidgetBase::OnTeamScoreChanged);
	bBoundToGameState = true;

	RefreshFromGameState();
}

void UDeathMatchWidgetBase::UnbindGameState()
{
	if (!bBoundToGameState)
	{
		return;
	}

	if (AGS_DeathMatch* GS = CachedGameState.Get())
	{
		GS->OnRemainingTimeChanged.RemoveDynamic(this, &UDeathMatchWidgetBase::OnRemainingTimeChanged);
		GS->OnRoundStateChanged.RemoveDynamic(this, &UDeathMatchWidgetBase::OnRoundStateChanged);
		GS->OnTeamScoreChanged.RemoveDynamic(this, &UDeathMatchWidgetBase::OnTeamScoreChanged);
	}

	bBoundToGameState = false;
	CachedGameState.Reset();
}

void UDeathMatchWidgetBase::RefreshFromGameState()
{
	AGS_DeathMatch* GS = GetDeathMatchGameState();
	if (!GS)
	{
		return;
	}

	HandleRemainingTimeChanged(GS->GetRemainingTime());
	HandleRoundStateChanged(GS->GetRoundState());

	for (const FTeamScoreData& Pair : GS->GetTeamScores())
	{
		HandleTeamScoreChanged(Pair);
	}
}

void UDeathMatchWidgetBase::HandleRemainingTimeChanged(float NewRemainingTime)
{
	BP_OnRemainingTimeAnim(NewRemainingTime);
}

void UDeathMatchWidgetBase::HandleRoundStateChanged(EDeathMatchRoundState NewState)
{
	BP_OnRoundStateAnim(NewState);
}

void UDeathMatchWidgetBase::HandleTeamScoreChanged(const FTeamScoreData& NewData)
{
	BP_OnTeamScoreAnim(NewData);
}

void UDeathMatchWidgetBase::OnRemainingTimeChanged(float NewRemainingTime)
{
	HandleRemainingTimeChanged(NewRemainingTime);
}

void UDeathMatchWidgetBase::OnRoundStateChanged(EDeathMatchRoundState NewState)
{
	HandleRoundStateChanged(NewState);
}

void UDeathMatchWidgetBase::OnTeamScoreChanged(const FTeamScoreData& NewData)
{
	HandleTeamScoreChanged(NewData);
}
