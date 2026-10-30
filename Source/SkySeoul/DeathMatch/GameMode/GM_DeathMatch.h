#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GenericTeamAgentInterface.h"
#include "DeathMatch/GameplayMessageStructure/DeathMatchEventSubsystem.h"
#include "SkySeoul/DeathMatch/GameplayMessageStructure/DeathMatchMessageStructure.h"
#include "GM_DeathMatch.generated.h"

class ADeathMatchAIController;

UCLASS()
class SKYSEOUL_API AGM_DeathMatch : public AGameModeBase
{
	GENERATED_BODY()

	FDelegateHandle DeathListenerHandle;

	UPROPERTY()
	FTimerHandle RoundRestartTimerHandle;

	UFUNCTION()
	void OnDeathMessageReceived(FGameplayTag InTag, const FDeathMessagePayload& Payload);

public:
	AGM_DeathMatch();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	uint8 GetTargetRound() const { return TargetRound; }
	uint32 GetTargetKillNum() const { return TargetKillNum; }
	float GetRoundDuration() const { return RoundDuration; }
	const TArray<uint8>& GetTeamIds() const { return TeamIds; }

	void NotifyRoundOver();
	void NotifyMatchOver();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|RoundState")
	uint8 TargetRound = 5;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|RoundState")
	uint32 TargetKillNum = 50;

	/** Per-round combat duration in seconds. 0 = no time limit */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|RoundState", meta = (ClampMin = "0.0"))
	float RoundDuration = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|RoundState", meta = (ClampMin = "0.0"))
	float RoundRestartDelay = 3.0f;

	/** Delay after death before RestartPlayer so parts explosion FX can play. 0 = immediate. */
	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Respawn", meta = (ClampMin = "0.0"))
	float DeathRespawnDelay = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Team")
	TArray<uint8> TeamIds;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|Team")
	int32 NumAIPerTeam;

	UPROPERTY(EditDefaultsOnly, Category = "DeathMatch|AI")
	TSubclassOf<class AAIController> AIControllerClass;

protected:
	void SpawnAllAIsForTeams();
	void SpawnSingleAI(uint8 TeamId);

	bool CanProcessDeathRespawn() const;
	void RespawnAllControllers();
	void ExecuteRoundRestart();
	void ClearRoundRestartTimer();
	void ScheduleDeathRespawn(AController* Victim);
	void ExecuteDeathRespawn(TWeakObjectPtr<AController> Victim);
};
