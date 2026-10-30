#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SkySeoul/DeathMatch/GameplayMessageStructure/DeathMatchMessageStructure.h"
#include "DeathMatch/GameplayMessageStructure/DeathMatchEventSubsystem.h"
#include "GS_DeathMatch.generated.h"

/** DeathMatch round state */
UENUM(BlueprintType)
enum class EDeathMatchRoundState : uint8
{
	WaitingToStart UMETA(DisplayName = "WaitingToStart"),
	InProgress     UMETA(DisplayName = "InProgress"),
	RoundOver      UMETA(DisplayName = "RoundOver"),
	MatchOver      UMETA(DisplayName = "MatchOver"),
};

USTRUCT(BlueprintType)
struct FTeamScoreData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	uint8 TeamId;

	UPROPERTY(BlueprintReadWrite)
	int32 Score;

	UPROPERTY(BlueprintReadWrite)
	int32 RoundScore;

	FTeamScoreData()
		: TeamId(255)
		, Score(0)
		, RoundScore(0)
	{
	}

	FTeamScoreData(uint8 InTeamId, int32 InScore, int32 InRoundScore)
		: TeamId(InTeamId)
		, Score(InScore)
		, RoundScore(InRoundScore)
	{
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundStateChanged, EDeathMatchRoundState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamScoreChanged, const FTeamScoreData&, NewData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemainingTimeChanged, float, NewRemainingTime);

/**
 * Replicated match-wide DeathMatch state.
 * Score mutation must go through AddScoreForTeam only.
 */
UCLASS()
class SKYSEOUL_API AGS_DeathMatch : public AGameStateBase
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 TargetRound;

	UPROPERTY()
	int32 TargetKillNum;

	UPROPERTY()
	float RoundDuration;

	FDelegateHandle DeathListenerHandle;

	UPROPERTY()
	FTimerHandle RoundClockTimerHandle;

	UFUNCTION()
	void OnDeathMessageReceived(FGameplayTag InTag, const FDeathMessagePayload& Payload);

	void InitializeTeamScores(const TArray<uint8>& InTeamIds);
	int32 FindTeamScoreIndex(uint8 TeamId) const;
	void AwardKillScoreForVictimTeam(uint8 VictimTeamId);
	void HandleRoundStateEntered(EDeathMatchRoundState NewState);

	void StartRoundClock();
	void StopRoundClock();
	void TickRoundClock();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	AGS_DeathMatch();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	float GetRemainingTime() const;
	EDeathMatchRoundState GetRoundState() const;
	int32 GetTeamScore(uint8 TeamId) const;
	int32 GetTeamRoundScore(uint8 TeamId) const;
	int32 GetTotalScore() const;
	bool IsRoundInProgress() const;
	const TArray<FTeamScoreData>& GetTeamScores() const { return TeamScores; }

	/** Highest RoundScore wins. Returns 255 if none / tie unresolved. */
	uint8 GetLeadingTeamIdByRoundScore() const;

	void SetRemainingTime(float NewRemainingTime);
	void SetRoundState(EDeathMatchRoundState NewState);

	void AddScoreForTeam(uint8 TeamId, int32 Delta);
	void ResetRoundKillScores();

public:
	UPROPERTY(BlueprintAssignable, Category = "DeathMatch")
	FOnRoundStateChanged OnRoundStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "DeathMatch")
	FOnTeamScoreChanged OnTeamScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "DeathMatch")
	FOnRemainingTimeChanged OnRemainingTimeChanged;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_RemainingTime, BlueprintReadOnly, Category = "DeathMatch")
	float RemainingTime;

	UPROPERTY(ReplicatedUsing = OnRep_RoundState, BlueprintReadOnly, Category = "DeathMatch")
	EDeathMatchRoundState RoundState;

	UPROPERTY(ReplicatedUsing = OnRep_TeamScores, BlueprintReadOnly, Category = "DeathMatch")
	TArray<FTeamScoreData> TeamScores;

	UFUNCTION()
	void OnRep_RemainingTime();

	UFUNCTION()
	void OnRep_RoundState();

	UFUNCTION()
	void OnRep_TeamScores();
};
