#include "DMCombatPositionRegistry.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "DeathMatch/Character/DeathMatchRobotCharacter.h"
#include "Navigation/PathFollowingComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
bool Alive(AActor* Owner)
{
    if (!IsValid(Owner)) return false;
    if (const auto* Robot=Cast<ADeathMatchRobotCharacter>(Owner)) return !Robot->IsDead;
    const auto* AI = Cast<ADeathMatchRobotAIController>(Owner);
    if (!AI) return true;
    const APawn* Pawn = AI->GetPawn();
    const auto* Robot = Cast<ADeathMatchRobotCharacter>(Pawn);
    return IsValid(Pawn) && (!Robot || !Robot->IsDead);
}
}
bool UDMCombatPositionRegistry::Overlaps(const FVector& A, const FVector& B, float Radius)
{
    return FMath::Abs(A.Z-B.Z) < 180 && FVector::DistSquared2D(A,B) <= FMath::Square(FMath::Max(0.f,Radius));
}
void UDMCombatPositionRegistry::Prune(float Now)
{
    Claims.RemoveAll([Now](const FDMPositionNeighbor& N) { return !Alive(N.Owner.Get()) || Now >= N.Expires; });
}
void UDMCombatPositionRegistry::RefreshOccupants()
{
    if (SnapshotFrame == GFrameCounter) return;
    SnapshotFrame = GFrameCounter; Occupants.Reset();
    // One world scan per frame, shared by all modules. Queries are O(pawns + claims).
    // Replace this snapshot with a spatial index when combat populations grow.
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        APawn* Pawn = *It;
		if (!Alive(Pawn) || !DeathMatchTeamUtils::IsCombatTargetCandidate(Pawn)) continue;
        auto* AI = Cast<ADeathMatchRobotAIController>(Pawn->GetController());
        if (AI && !Alive(AI)) continue;
        FDMPositionNeighbor N; N.Owner = AI ? static_cast<AActor*>(AI) : Pawn;
        N.Location = Pawn->GetNavAgentLocation();
        if (AI)
        {
            N.FireLaneEnd=AI->GetFocalPoint();
            N.bHasFireLane=!N.FireLaneEnd.ContainsNaN() && FVector::DistSquared(N.FireLaneEnd,N.Location)<1.e12;
        }
        Occupants.Add(N);
        if (AI && AI->GetMoveStatus() != EPathFollowingStatus::Idle && AI->GetPathFollowingComponent())
        {
            N.Location = AI->GetPathFollowingComponent()->GetPathDestination(); N.bReserved = true; N.bHasFireLane=false; Occupants.Add(N);
        }
    }
}
void UDMCombatPositionRegistry::Gather(AActor* Self, float Now, TArray<FDMPositionNeighbor>& Out)
{
    Prune(Now); RefreshOccupants(); Out.Reset();
    const auto Team=DeathMatchTeamUtils::GetTeamIdFromActor(Self);
    auto IsAlly=[&](const FDMPositionNeighbor& N) { return N.Owner!=Self && Alive(N.Owner.Get())
        && DeathMatchTeamUtils::GetAttitudeTowards(Team,*N.Owner)==ETeamAttitude::Friendly; };
    // Hostile occupancy must never reveal a hidden enemy's live position to scoring.
    for (const auto& N : Occupants) if (IsAlly(N)) Out.Add(N);
    for (const auto& N : Claims) if (IsAlly(N)) Out.Add(N);
}
void UDMCombatPositionRegistry::GatherFriendlyOccupants(AActor* Self, TArray<FDMPositionNeighbor>& Out)
{
	RefreshOccupants();
	Out.Reset();
	const auto Team = DeathMatchTeamUtils::GetTeamIdFromActor(Self);
	for (const FDMPositionNeighbor& Neighbor : Occupants)
	{
		if (Neighbor.Owner != Self && Alive(Neighbor.Owner.Get())
			&& DeathMatchTeamUtils::GetAttitudeTowards(Team, *Neighbor.Owner) == ETeamAttitude::Friendly)
		{
			Out.Add(Neighbor);
		}
	}
}
bool UDMCombatPositionRegistry::IsClaimed(AActor* Self, const FVector& Goal, float Radius, float Now)
{
    Prune(Now);
    const auto Team=DeathMatchTeamUtils::GetTeamIdFromActor(Self);
    for (const auto& N : Claims)
        if (N.Owner != Self && N.Owner.IsValid()
            && DeathMatchTeamUtils::GetAttitudeTowards(Team,*N.Owner)==ETeamAttitude::Friendly
            && Overlaps(Goal,N.Location,FMath::Max(Radius,N.Radius))) return true;
    return false;
}
bool UDMCombatPositionRegistry::Reserve(AActor* Owner, const TArray<FVector>& Goals, float Radius, float Now, float Lifetime)
{
    if (!Alive(Owner) || Goals.IsEmpty()) return false;
    for (const FVector& Goal : Goals) if (IsClaimed(Owner,Goal,Radius,Now)) return false;
    Release(Owner);
    for (const FVector& Goal : Goals)
    {
        FDMPositionNeighbor N; N.Owner=Owner; N.Location=Goal; N.Radius=Radius;
        N.bReserved=true; N.Expires=Now+FMath::Max(.1f,Lifetime); Claims.Add(N);
    }
    return true;
}
void UDMCombatPositionRegistry::Release(AActor* Owner)
{
    Claims.RemoveAll([Owner](const FDMPositionNeighbor& N) { return N.Owner == Owner; });
}
