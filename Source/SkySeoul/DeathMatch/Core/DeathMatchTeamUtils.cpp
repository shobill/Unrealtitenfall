#include "DeathMatchTeamUtils.h"
#include "DeathMatch/Core/DeathMatchLog.h"
#include "DeathMatch/Core/DMNonCombatActor.h"

#include "DeathMatch/PlayerState/DeathMatchPlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GenericTeamAgentInterface.h"
#include "Engine/World.h"

namespace DeathMatchTeamUtils
{
	bool IsCombatTargetCandidate(const AActor* Actor)
	{
		return IsValid(Actor)
			&& !Actor->GetClass()->ImplementsInterface(UDMNonCombatActor::StaticClass());
	}

	FGenericTeamId GetTeamIdFromController(const AController* Controller)
	{
		if (!Controller)
		{
			return FGenericTeamId::NoTeam;
		}

		if (const ADeathMatchPlayerState* DMPS = Controller->GetPlayerState<ADeathMatchPlayerState>())
		{
			return DMPS->GetGenericTeamId();
		}

		// AI / edge cases: team may only live on the controller.
		if (const IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(Controller))
		{
			return TeamAgent->GetGenericTeamId();
		}

		return FGenericTeamId::NoTeam;
	}

	FGenericTeamId GetTeamIdFromActor(const AActor* Actor)
	{
		if (!Actor)
		{
			return FGenericTeamId::NoTeam;
		}

		// PlayerState / AIController implementing the interface directly.
		if (const IGenericTeamAgentInterface* Direct = Cast<IGenericTeamAgentInterface>(Actor))
		{
			const FGenericTeamId DirectId = Direct->GetGenericTeamId();
			if (DirectId != FGenericTeamId::NoTeam)
			{
				return DirectId;
			}
		}

		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			if (const ADeathMatchPlayerState* DMPS = Pawn->GetPlayerState<ADeathMatchPlayerState>())
			{
				return DMPS->GetGenericTeamId();
			}

			if (const AController* PawnController = Pawn->GetController())
			{
				return GetTeamIdFromController(PawnController);
			}

			// Pawn may implement interface with NoTeam — already tried Direct above.
			return FGenericTeamId::NoTeam;
		}

		if (const AController* AsController = Cast<AController>(Actor))
		{
			return GetTeamIdFromController(AsController);
		}

		return FGenericTeamId::NoTeam;
	}

	ETeamAttitude::Type GetAttitudeTowards(FGenericTeamId SelfTeam, const AActor& Other)
	{
		if (!IsCombatTargetCandidate(&Other))
		{
			return ETeamAttitude::Neutral;
		}

		const FGenericTeamId OtherTeam = GetTeamIdFromActor(&Other);

		// Unassigned teams must stay Neutral so Perception affiliation does not
		// treat NoTeam(255) vs Team0 as Hostile via default ID inequality.
		if (SelfTeam == FGenericTeamId::NoTeam || OtherTeam == FGenericTeamId::NoTeam)
		{
			return ETeamAttitude::Neutral;
		}

		return FGenericTeamId::GetAttitude(SelfTeam, OtherTeam);
	}

	const TCHAR* AttitudeToString(ETeamAttitude::Type Attitude)
	{
		switch (Attitude)
		{
		case ETeamAttitude::Friendly: return TEXT("Friendly");
		case ETeamAttitude::Hostile:  return TEXT("Hostile");
		case ETeamAttitude::Neutral:  return TEXT("Neutral");
		default:                      return TEXT("Unknown");
		}
	}

	void SetTeamIdToController(AController* Controller, const FGenericTeamId& TeamId)
	{
		if (!Controller)
		{
			UE_LOG(LogDM, Log, TEXT("DeathMatchTeamUtils::SetTeamIdToController - Invalid Controller"));
			return;
		}

		if (ADeathMatchPlayerState* DMPS = Controller->GetPlayerState<ADeathMatchPlayerState>())
		{
			DMPS->SetGenericTeamId(TeamId);
		}
		else
		{
			UE_LOG(LogDM, Log, TEXT("DeathMatchTeamUtils::SetTeamIdToController - Invalid DeathMatchPlayerState"));
		}

		if (IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(Controller))
		{
			TeamAgent->SetGenericTeamId(TeamId);
		}

		UE_LOG(LogDM, Verbose,
			TEXT("DeathMatchTeam: %s assigned TeamId=%u"),
			*GetNameSafe(Controller),
			TeamId.GetId());
	}

	int32 CountControllers(UWorld* World)
	{
		if (!World)
		{
			return 0;
		}

		int32 ControllerCount = 0;
		for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
		{
			if (It->Get())
			{
				++ControllerCount;
			}
		}
		return ControllerCount;
	}

	void AssignTeamIfNone(UWorld* World, AController* NewPlayer, const TArray<uint8>& TeamIds)
	{
		if (!NewPlayer || !World || TeamIds.Num() == 0)
		{
			return;
		}

		if (GetTeamIdFromController(NewPlayer) != FGenericTeamId::NoTeam)
		{
			return;
		}

		// Count members per team (exclude NewPlayer who still has NoTeam).
		TArray<int32> Counts;
		Counts.Init(0, TeamIds.Num());

		for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
		{
			AController* Other = It->Get();
			if (!Other || Other == NewPlayer)
			{
				continue;
			}

			const FGenericTeamId OtherTeam = GetTeamIdFromController(Other);
			if (OtherTeam == FGenericTeamId::NoTeam)
			{
				continue;
			}

			for (int32 i = 0; i < TeamIds.Num(); ++i)
			{
				if (TeamIds[i] == OtherTeam.GetId())
				{
					Counts[i]++;
					break;
				}
			}
		}

		// Human players: prefer TeamIds[0] when that team is not overcrowded vs the other teams.
		// AI / others: join the team with the fewest members (balance).
		int32 ChosenIndex = 0;
		if (Cast<APlayerController>(NewPlayer))
		{
			ChosenIndex = 0;
			for (int32 i = 1; i < TeamIds.Num(); ++i)
			{
				if (Counts[i] < Counts[ChosenIndex])
				{
					ChosenIndex = i;
				}
			}
			// Prefer team 0 unless it already has strictly more players than the smallest team.
			if (Counts[0] <= Counts[ChosenIndex])
			{
				ChosenIndex = 0;
			}
		}
		else
		{
			ChosenIndex = 0;
			for (int32 i = 1; i < TeamIds.Num(); ++i)
			{
				if (Counts[i] < Counts[ChosenIndex])
				{
					ChosenIndex = i;
				}
			}
		}

		SetTeamIdToController(NewPlayer, FGenericTeamId(TeamIds[ChosenIndex]));
	}
}
