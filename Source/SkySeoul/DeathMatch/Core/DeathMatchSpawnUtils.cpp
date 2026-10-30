#include "DeathMatchSpawnUtils.h"
#include "DeathMatch/Core/DeathMatchLog.h"

#include "DeathMatch/Core/DeathMatchTeamUtils.h"
#include "DeathMatch/PlayerStart/PlayerStartByTeam.h"
#include "DeathMatch/Controller/DeathMatchAIController.h"

#include "AIController.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

namespace DeathMatchSpawnUtils
{
	void EnsurePlayerStateCreated(AController* Controller)
	{
		if (!Controller)
		{
			return;
		}

		if (Controller->GetPlayerState<APlayerState>())
		{
			return;
		}

		Controller->InitPlayerState();
	}

	AActor* FindPlayerStartByTeam(UWorld* World, AController* Player)
	{
		if (!World || !Player)
		{
			return nullptr;
		}

		const FGenericTeamId TeamId = DeathMatchTeamUtils::GetTeamIdFromController(Player);
		if (TeamId == FGenericTeamId::NoTeam)
		{
			return nullptr;
		}

		TArray<AActor*> FoundStarts;
		UGameplayStatics::GetAllActorsOfClass(World, APlayerStartByTeam::StaticClass(), FoundStarts);

		TArray<APlayerStartByTeam*> Matching;
		TArray<APlayerStartByTeam*> Unoccupied;
		Matching.Reserve(FoundStarts.Num());
		Unoccupied.Reserve(FoundStarts.Num());

		// PlayerStart capsule overlap is unreliable — treat a start as occupied if a pawn is nearby.
		constexpr float OccupiedRadiusSq = 120.0f * 120.0f;
		TArray<AActor*> Pawns;
		UGameplayStatics::GetAllActorsOfClass(World, APawn::StaticClass(), Pawns);

		for (AActor* Actor : FoundStarts)
		{
			APlayerStartByTeam* Start = Cast<APlayerStartByTeam>(Actor);
			if (!Start || Start->TeamId != TeamId.GetId())
			{
				continue;
			}

			Matching.Add(Start);

			const FVector StartLoc = Start->GetActorLocation();
			bool bBlocked = false;
			for (AActor* PawnActor : Pawns)
			{
				if (DeathMatchTeamUtils::IsCombatTargetCandidate(PawnActor)
					&& FVector::DistSquared(PawnActor->GetActorLocation(), StartLoc) <= OccupiedRadiusSq)
				{
					bBlocked = true;
					break;
				}
			}
			if (!bBlocked)
			{
				Unoccupied.Add(Start);
			}
		}

		if (Matching.Num() == 0)
		{
			UE_LOG(LogDM, Warning,
				TEXT("DeathMatchSpawn: no PlayerStartByTeam for team %d (found %d starts total). Check TeamId on starts."),
				TeamId.GetId(), FoundStarts.Num());
			for (AActor* Actor : FoundStarts)
			{
				if (const APlayerStartByTeam* Start = Cast<APlayerStartByTeam>(Actor))
				{
					UE_LOG(LogDM, Warning, TEXT("  start=%s TeamId=%d"), *GetNameSafe(Start), Start->TeamId);
				}
			}
			return nullptr;
		}

		if (Matching.Num() == 1)
		{
			UE_LOG(LogDM, Warning,
				TEXT("DeathMatchSpawn: team %d has only 1 matching PlayerStartByTeam (%s). All spawns will stack there — add more starts with TeamId=%d."),
				TeamId.GetId(), *GetNameSafe(Matching[0]), TeamId.GetId());
		}

		const TArray<APlayerStartByTeam*>& Pool = Unoccupied.Num() > 0 ? Unoccupied : Matching;

		// Round-robin per team — avoids everyone stacking on start #1 when Rand is cold/unseeded.
		static TMap<uint8, int32> NextIndexByTeam;
		int32& NextIndex = NextIndexByTeam.FindOrAdd(TeamId.GetId());
		const int32 Pick = FMath::Clamp(NextIndex % Pool.Num(), 0, Pool.Num() - 1);
		++NextIndex;

		UE_LOG(LogDM, Verbose,
			TEXT("DeathMatchSpawn: team %d picked start %d/%d (%s) unoccupied=%d matching=%d"),
			TeamId.GetId(), Pick + 1, Pool.Num(), *GetNameSafe(Pool[Pick]), Unoccupied.Num(), Matching.Num());
		return Pool[Pick];
	}

	void GatherControllers(UWorld* World, TArray<AController*>& OutControllers)
	{
		OutControllers.Reset();
		if (!World)
		{
			return;
		}

		for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
		{
			if (AController* Controller = It->Get())
			{
				OutControllers.Add(Controller);
			}
		}
	}

	bool RestartDeathMatchAI(UWorld* World, ADeathMatchAIController* AIController)
	{
		if (!World || !AIController)
		{
			return false;
		}

		if (APawn* OldPawn = AIController->GetPawn())
		{
			AIController->UnPossess();
			OldPawn->Destroy();
		}

		AActor* TeamStart = FindPlayerStartByTeam(World, AIController);
		if (!TeamStart)
		{
			UE_LOG(LogDM, Warning, TEXT("DeathMatchSpawnUtils::RestartDeathMatchAI - no PlayerStartByTeam for team %d. Place starts with TeamId 0/1."),
				DeathMatchTeamUtils::GetTeamIdFromController(AIController).GetId());
			return false;
		}

		TSubclassOf<APawn> PawnClass = AIController->GetAIPawnClass();
		if (!PawnClass)
		{
			UE_LOG(LogDM, Warning, TEXT("DeathMatchSpawnUtils::RestartDeathMatchAI - no AI pawn class assigned. Set AICharacterClass on %s."),
				*AIController->GetClass()->GetName());
			return false;
		}

		APawn* NewPawn = World->SpawnActor<APawn>(
			PawnClass,
			TeamStart->GetActorLocation(),
			TeamStart->GetActorRotation()
		);

		if (!NewPawn)
		{
			UE_LOG(LogDM, Warning, TEXT("DeathMatchSpawnUtils::RestartDeathMatchAI - SpawnActor failed for %s at %s"),
				*GetNameSafe(PawnClass.Get()), *TeamStart->GetActorLocation().ToString());
			return false;
		}

		AIController->Possess(NewPawn);
		return true;
	}

	AAIController* SpawnAIController(UWorld* World, TSubclassOf<AAIController> AIControllerClass, uint8 TeamId)
	{
		if (!World || !AIControllerClass)
		{
			return nullptr;
		}

		AAIController* NewAIController = World->SpawnActor<AAIController>(AIControllerClass);
		if (!NewAIController)
		{
			return nullptr;
		}

		EnsurePlayerStateCreated(NewAIController);
		DeathMatchTeamUtils::SetTeamIdToController(NewAIController, FGenericTeamId(TeamId));
		return NewAIController;
	}
}
