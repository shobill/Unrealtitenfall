#include "GS_DeathMatch.h"
#include "DeathMatch/Core/DeathMatchLog.h"
#include "Net/UnrealNetwork.h"
#include "GM_DeathMatch.h"
#include "TimerManager.h"

void AGS_DeathMatch::OnDeathMessageReceived(FGameplayTag InTag, const FDeathMessagePayload& Payload)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!Payload.Victim)
	{
		return;
	}

	AwardKillScoreForVictimTeam(Payload.VictimTeamId);
}

void AGS_DeathMatch::AwardKillScoreForVictimTeam(uint8 VictimTeamId)
{
	// NoTeam / unset — don't award (would score every team incorrectly).
	if (VictimTeamId == 255)
	{
		return;
	}

	for (const FTeamScoreData& TSD : TeamScores)
	{
		if (TSD.TeamId != VictimTeamId)
		{
			AddScoreForTeam(TSD.TeamId, 1);
			return;
		}
	}
}

void AGS_DeathMatch::InitializeTeamScores(const TArray<uint8>& InTeamIds)
{
	if (!HasAuthority())
	{
		return;
	}

	TeamScores.Reset();
	TeamScores.Reserve(InTeamIds.Num());

	for (const uint8 TeamId : InTeamIds)
	{
		TeamScores.Add(FTeamScoreData(TeamId, 0, 0));
	}
}

void AGS_DeathMatch::ResetRoundKillScores()
{
	if (!HasAuthority())
	{
		return;
	}

	for (FTeamScoreData& Pair : TeamScores)
	{
		Pair.Score = 0;
		OnTeamScoreChanged.Broadcast(Pair);
	}
}

void AGS_DeathMatch::StartRoundClock()
{
	if (!HasAuthority())
	{
		return;
	}

	StopRoundClock();

	if (RoundDuration <= 0.0f)
	{
		SetRemainingTime(0.0f);
		return;
	}

	SetRemainingTime(RoundDuration);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RoundClockTimerHandle,
			this,
			&AGS_DeathMatch::TickRoundClock,
			1.0f,
			true
		);
	}
}

void AGS_DeathMatch::StopRoundClock()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundClockTimerHandle);
	}
}

void AGS_DeathMatch::TickRoundClock()
{
	if (!HasAuthority())
	{
		return;
	}

	if (RoundState != EDeathMatchRoundState::InProgress)
	{
		StopRoundClock();
		return;
	}

	SetRemainingTime(RemainingTime - 1.0f);

	if (RemainingTime <= 0.0f)
	{
		SetRemainingTime(0.0f);
		StopRoundClock();
		SetRoundState(EDeathMatchRoundState::RoundOver);
	}
}

void AGS_DeathMatch::HandleRoundStateEntered(EDeathMatchRoundState NewState)
{
	AGM_DeathMatch* DMGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AGM_DeathMatch>() : nullptr;

	switch (NewState)
	{
	case EDeathMatchRoundState::InProgress:
		StartRoundClock();
		break;

	case EDeathMatchRoundState::RoundOver:
		StopRoundClock();
		if (DMGameMode)
		{
			DMGameMode->NotifyRoundOver();
		}
		break;

	case EDeathMatchRoundState::MatchOver:
		StopRoundClock();
		if (DMGameMode)
		{
			DMGameMode->NotifyMatchOver();
		}
		break;

	default:
		StopRoundClock();
		break;
	}
}

int32 AGS_DeathMatch::FindTeamScoreIndex(uint8 TeamId) const
{
	for (int32 Index = 0; Index < TeamScores.Num(); ++Index)
	{
		if (TeamScores[Index].TeamId == TeamId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void AGS_DeathMatch::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	const FGameplayTag DeathEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Death")), false);
	if (DeathEventTag.IsValid())
	{
		DeathListenerHandle = GetWorld()->GetSubsystem<UDeathMatchEventSubsystem>()->OnDeath
			.AddUObject(
				this,
				&AGS_DeathMatch::OnDeathMessageReceived
			);
	}
	else
	{
		UE_LOG(LogDM, Error, TEXT("GS_DeathMatch: Event.Death tag missing — death listener not registered"));
	}

	AGM_DeathMatch* DMGameMode = GetWorld()->GetAuthGameMode<AGM_DeathMatch>();
	if (!DMGameMode)
	{
		return;
	}

	TargetRound = DMGameMode->GetTargetRound();
	TargetKillNum = static_cast<int32>(DMGameMode->GetTargetKillNum());
	RoundDuration = DMGameMode->GetRoundDuration();

	InitializeTeamScores(DMGameMode->GetTeamIds());
	SetRoundState(EDeathMatchRoundState::InProgress);
}

void AGS_DeathMatch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDeathMatchEventSubsystem* Events = GetWorld()->GetSubsystem<UDeathMatchEventSubsystem>())
	{
		Events->OnDeath.Remove(DeathListenerHandle);
	}
	StopRoundClock();
	Super::EndPlay(EndPlayReason);
}

AGS_DeathMatch::AGS_DeathMatch()
{
	RemainingTime = 0.0f;
	RoundState = EDeathMatchRoundState::WaitingToStart;
	TargetRound = 0;
	TargetKillNum = 0;
	RoundDuration = 0.0f;

	bReplicates = true;
}

void AGS_DeathMatch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGS_DeathMatch, RemainingTime);
	DOREPLIFETIME(AGS_DeathMatch, RoundState);
	DOREPLIFETIME(AGS_DeathMatch, TeamScores);
}

float AGS_DeathMatch::GetRemainingTime() const
{
	return RemainingTime;
}

EDeathMatchRoundState AGS_DeathMatch::GetRoundState() const
{
	return RoundState;
}

bool AGS_DeathMatch::IsRoundInProgress() const
{
	return RoundState == EDeathMatchRoundState::InProgress;
}

int32 AGS_DeathMatch::GetTeamScore(uint8 TeamId) const
{
	const int32 Index = FindTeamScoreIndex(TeamId);
	if (Index == INDEX_NONE)
	{
		return 0;
	}
	return TeamScores[Index].Score;
}

int32 AGS_DeathMatch::GetTeamRoundScore(uint8 TeamId) const
{
	const int32 Index = FindTeamScoreIndex(TeamId);
	if (Index == INDEX_NONE)
	{
		return 0;
	}
	return TeamScores[Index].RoundScore;
}

int32 AGS_DeathMatch::GetTotalScore() const
{
	int32 Total = 0;
	for (const FTeamScoreData& Pair : TeamScores)
	{
		Total += Pair.Score;
	}
	return Total;
}

uint8 AGS_DeathMatch::GetLeadingTeamIdByRoundScore() const
{
	if (TeamScores.Num() == 0)
	{
		return 255;
	}

	uint8 LeadingTeamId = TeamScores[0].TeamId;
	int32 BestRoundScore = TeamScores[0].RoundScore;
	bool bTie = false;

	for (int32 Index = 1; Index < TeamScores.Num(); ++Index)
	{
		const FTeamScoreData& Pair = TeamScores[Index];
		if (Pair.RoundScore > BestRoundScore)
		{
			BestRoundScore = Pair.RoundScore;
			LeadingTeamId = Pair.TeamId;
			bTie = false;
		}
		else if (Pair.RoundScore == BestRoundScore)
		{
			bTie = true;
		}
	}

	return bTie ? 255 : LeadingTeamId;
}

void AGS_DeathMatch::SetRemainingTime(float NewRemainingTime)
{
	if (!HasAuthority())
	{
		return;
	}

	RemainingTime = NewRemainingTime;
	OnRemainingTimeChanged.Broadcast(RemainingTime);
}

void AGS_DeathMatch::SetRoundState(EDeathMatchRoundState NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	if (RoundState == NewState)
	{
		return;
	}

	RoundState = NewState;
	OnRoundStateChanged.Broadcast(RoundState);
	HandleRoundStateEntered(RoundState);
}

void AGS_DeathMatch::AddScoreForTeam(uint8 TeamId, int32 Delta)
{
	if (!HasAuthority())
	{
		return;
	}

	if (RoundState != EDeathMatchRoundState::InProgress)
	{
		return;
	}

	int32 Index = FindTeamScoreIndex(TeamId);
	if (Index == INDEX_NONE)
	{
		Index = TeamScores.Add(FTeamScoreData(TeamId, 0, 0));
	}

	FTeamScoreData& Pair = TeamScores[Index];
	Pair.Score += Delta;

	OnTeamScoreChanged.Broadcast(Pair);

	if (TargetKillNum > 0 && Pair.Score >= TargetKillNum)
	{
		Pair.RoundScore += 1;
		OnTeamScoreChanged.Broadcast(Pair);

		if (TargetRound > 0 && Pair.RoundScore >= TargetRound)
		{
			SetRoundState(EDeathMatchRoundState::MatchOver);
		}
		else
		{
			SetRoundState(EDeathMatchRoundState::RoundOver);
		}
	}
}

void AGS_DeathMatch::OnRep_RemainingTime()
{
	OnRemainingTimeChanged.Broadcast(RemainingTime);
}

void AGS_DeathMatch::OnRep_RoundState()
{
	OnRoundStateChanged.Broadcast(RoundState);
}

void AGS_DeathMatch::OnRep_TeamScores()
{
	for (const FTeamScoreData& Pair : TeamScores)
	{
		OnTeamScoreChanged.Broadcast(Pair);
	}
}
