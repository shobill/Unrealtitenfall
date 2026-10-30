#include "GM_DeathMatch.h"
#include "DeathMatch/Core/DeathMatchLog.h"
#include "GS_DeathMatch.h"

#include "DeathMatch/Core/DeathMatchTeamUtils.h"
#include "DeathMatch/Core/DeathMatchSpawnUtils.h"
#include "DeathMatch/GameInstance/DeathMatchGameInstane.h"
#include "DeathMatch/PlayerState/DeathMatchPlayerState.h"
#include "DeathMatch/Controller/DeathMatchAIController.h"
#include "DeathMatch/Controller/DeathMatchPlayerController.h"
#include "DeathMatch/Character/DeathMatchPlayerCharacter.h"
#include "DeathMatch/Presentation/HUD/DeathMatchHUD.h"

#include "AIController.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

AGM_DeathMatch::AGM_DeathMatch()
{
	NumAIPerTeam = 3;
	RoundDuration = 180.0f;
	RoundRestartDelay = 3.0f;
	DeathRespawnDelay = 1.5f;
	TeamIds = { 0, 1 };
	HUDClass = ADeathMatchHUD::StaticClass();

	// Approach A: SkySeoul player (no Parts). BP_GM can still override with BP children.
	DefaultPawnClass = ADeathMatchPlayerCharacter::StaticClass();
	PlayerControllerClass = ADeathMatchPlayerController::StaticClass();
}

void AGM_DeathMatch::OnDeathMessageReceived(FGameplayTag InTag, const FDeathMessagePayload& Payload)
{
	if (!CanProcessDeathRespawn())
	{
		return;
	}

	if (Payload.Victim)
	{
		DeathMatchTeamUtils::SetTeamIdToController(Payload.Victim, FGenericTeamId(Payload.VictimTeamId));
		ScheduleDeathRespawn(Payload.Victim);
	}
}

void AGM_DeathMatch::ScheduleDeathRespawn(AController* Victim)
{
	if (!Victim)
	{
		return;
	}

	if (DeathRespawnDelay <= 0.0f)
	{
		RestartPlayer(Victim);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		RestartPlayer(Victim);
		return;
	}

	TWeakObjectPtr<AController> WeakVictim(Victim);
	FTimerHandle RespawnTimerHandle;
	World->GetTimerManager().SetTimer(
		RespawnTimerHandle,
		FTimerDelegate::CreateUObject(this, &AGM_DeathMatch::ExecuteDeathRespawn, WeakVictim),
		DeathRespawnDelay,
		false
	);
}

void AGM_DeathMatch::ExecuteDeathRespawn(TWeakObjectPtr<AController> Victim)
{
	if (!CanProcessDeathRespawn())
	{
		return;
	}

	if (AController* Controller = Victim.Get())
	{
		RestartPlayer(Controller);
	}
}

bool AGM_DeathMatch::CanProcessDeathRespawn() const
{
	const AGS_DeathMatch* GS = GetGameState<AGS_DeathMatch>();
	if (!GS)
	{
		return true;
	}
	return GS->IsRoundInProgress();
}

void AGM_DeathMatch::NotifyRoundOver()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ClearRoundRestartTimer();

	if (RoundRestartDelay <= 0.0f)
	{
		ExecuteRoundRestart();
		return;
	}

	World->GetTimerManager().SetTimer(
		RoundRestartTimerHandle,
		this,
		&AGM_DeathMatch::ExecuteRoundRestart,
		RoundRestartDelay,
		false
	);
}

void AGM_DeathMatch::NotifyMatchOver()
{
	if (!HasAuthority())
	{
		return;
	}

	ClearRoundRestartTimer();
}

void AGM_DeathMatch::ExecuteRoundRestart()
{
	if (!HasAuthority())
	{
		return;
	}

	const AGS_DeathMatch* GS = GetGameState<AGS_DeathMatch>();
	if (GS && GS->GetRoundState() == EDeathMatchRoundState::MatchOver)
	{
		return;
	}

	if (AGS_DeathMatch* MutableGS = GetGameState<AGS_DeathMatch>())
	{
		MutableGS->ResetRoundKillScores();
		RespawnAllControllers();
		MutableGS->SetRoundState(EDeathMatchRoundState::InProgress);
	}
	else
	{
		RespawnAllControllers();
	}
}

void AGM_DeathMatch::RespawnAllControllers()
{
	TArray<AController*> Controllers;
	DeathMatchSpawnUtils::GatherControllers(GetWorld(), Controllers);

	for (AController* Controller : Controllers)
	{
		RestartPlayer(Controller);
	}
}

void AGM_DeathMatch::ClearRoundRestartTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundRestartTimerHandle);
	}
}

void AGM_DeathMatch::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Always use DM PlayerState / GameState.
	PlayerStateClass = ADeathMatchPlayerState::StaticClass();
	GameStateClass = AGS_DeathMatch::StaticClass();

	// Lobby → match sets GI payload. Direct PIE has empty payload — keep BP_GM defaults
	// (otherwise PlayerControllerClass becomes null → "Couldn't spawn player controller").
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UDeathMatchGameInstane* Subsys = GI ? GI->GetSubsystem<UDeathMatchGameInstane>() : nullptr;
	if (!Subsys)
	{
		return;
	}

	if (const TSubclassOf<ACharacter> PayloadPawn = Subsys->GetPayloadCharacter())
	{
		DefaultPawnClass = PayloadPawn;
	}

	if (const TSubclassOf<AController> PayloadPC = Subsys->GetPayloadController())
	{
		if (PayloadPC->IsChildOf(APlayerController::StaticClass()))
		{
			PlayerControllerClass = PayloadPC;
		}
	}
}

void AGM_DeathMatch::BeginPlay()
{
	Super::BeginPlay();

	SpawnAllAIsForTeams();

	if (HasAuthority())
	{
		const FGameplayTag DeathEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Death")), false);
		if (DeathEventTag.IsValid())
		{
			DeathListenerHandle = GetWorld()->GetSubsystem<UDeathMatchEventSubsystem>()->OnDeath
				.AddUObject(
					this,
					&AGM_DeathMatch::OnDeathMessageReceived
				);
		}
		else
		{
			UE_LOG(LogDM, Error, TEXT("GM_DeathMatch: Event.Death tag missing — death listener not registered"));
		}
	}
}

void AGM_DeathMatch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDeathMatchEventSubsystem* Events = GetWorld()->GetSubsystem<UDeathMatchEventSubsystem>())
	{
		Events->OnDeath.Remove(DeathListenerHandle);
	}
	ClearRoundRestartTimer();
	Super::EndPlay(EndPlayReason);
}

void AGM_DeathMatch::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	DeathMatchTeamUtils::AssignTeamIfNone(GetWorld(), NewPlayer, TeamIds);

	if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
	{
		Super::RestartPlayer(NewPlayer);

		FInputModeGameOnly IMGO;
		PC->SetInputMode(IMGO);
	}
	else if (ADeathMatchAIController* AIC = Cast<ADeathMatchAIController>(NewPlayer))
	{
		DeathMatchSpawnUtils::RestartDeathMatchAI(GetWorld(), AIC);
	}
}

AActor* AGM_DeathMatch::ChoosePlayerStart_Implementation(AController* Player)
{
	// Team must be set before start search — otherwise Find returns null and Super stacks everyone on spawn #1.
	DeathMatchTeamUtils::AssignTeamIfNone(GetWorld(), Player, TeamIds);

	if (AActor* TeamStart = DeathMatchSpawnUtils::FindPlayerStartByTeam(GetWorld(), Player))
	{
		return TeamStart;
	}

	UE_LOG(LogDM, Warning,
		TEXT("GM_DeathMatch::ChoosePlayerStart — no team start for %s; Super may use first PlayerStart."),
		*GetNameSafe(Player));
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AGM_DeathMatch::SpawnAllAIsForTeams()
{
	if (TeamIds.Num() == 0)
	{
		UE_LOG(LogDM, Warning, TEXT("GM_DeathMatch: TeamIds empty — AI spawn skipped."));
		return;
	}
	if (NumAIPerTeam <= 0)
	{
		UE_LOG(LogDM, Warning, TEXT("GM_DeathMatch: NumAIPerTeam=%d — AI spawn skipped."), NumAIPerTeam);
		return;
	}
	if (!AIControllerClass)
	{
		UE_LOG(LogDM, Warning, TEXT("GM_DeathMatch: AIControllerClass not set on GameMode — AI spawn skipped. Assign BP_AIC_DM_Robot (DeathMatchRobotAIController)."));
		return;
	}

	for (uint8 TeamIdValue : TeamIds)
	{
		for (int32 i = 0; i < NumAIPerTeam; ++i)
		{
			SpawnSingleAI(TeamIdValue);
		}
	}
}

void AGM_DeathMatch::SpawnSingleAI(uint8 TeamId)
{
	AAIController* NewAIController = DeathMatchSpawnUtils::SpawnAIController(GetWorld(), AIControllerClass, TeamId);
	if (!NewAIController)
	{
		UE_LOG(LogDM, Warning, TEXT("GM_DeathMatch: Failed to spawn AIController class %s"), *GetNameSafe(AIControllerClass));
		return;
	}

	if (!Cast<ADeathMatchAIController>(NewAIController))
	{
		UE_LOG(LogDM, Warning, TEXT("GM_DeathMatch: %s is not a DeathMatchAIController — pawn will not spawn. Use DeathMatchRobotAIController BP."), *NewAIController->GetClass()->GetName());
	}

	RestartPlayer(NewAIController);
}
