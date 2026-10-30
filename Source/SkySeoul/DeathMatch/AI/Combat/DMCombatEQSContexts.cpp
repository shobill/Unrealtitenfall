#include "DMCombatEQSContexts.h"

#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/AI/Combat/DMCombatPositionRegistry.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "GameFramework/Pawn.h"

namespace
{
UDMCombatKnowledgeComponent* ResolveKnowledge(const FEnvQueryInstance& QueryInstance, ADeathMatchRobotAIController** OutAI = nullptr)
{
    AActor* Owner = Cast<AActor>(QueryInstance.Owner.Get());
    ADeathMatchRobotAIController* AI = Cast<ADeathMatchRobotAIController>(Owner);
    if (!AI)
        if (const APawn* Pawn = Cast<APawn>(Owner)) AI = Cast<ADeathMatchRobotAIController>(Pawn->GetController());
    if (OutAI) *OutAI = AI;
    return AI ? AI->FindComponentByClass<UDMCombatKnowledgeComponent>() : nullptr;
}
}

void UDMEnvQueryContext_FocusingTarget::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
    const UDMCombatKnowledgeComponent* Knowledge = ResolveKnowledge(QueryInstance);
    UEnvQueryItemType_Actor::SetContextHelper(ContextData, Knowledge ? Knowledge->GetCombatTarget() : nullptr);
}
void UDMEnvQueryContext_VisibleEnemies::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
    TArray<AActor*> Actors; if (UDMCombatKnowledgeComponent* Knowledge=ResolveKnowledge(QueryInstance)) Knowledge->GetVisibleThreatActors(Actors);
    UEnvQueryItemType_Actor::SetContextHelper(ContextData, Actors);
}
void UDMEnvQueryContext_RememberedEnemyLocations::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
    TArray<FVector> Locations; if (UDMCombatKnowledgeComponent* Knowledge=ResolveKnowledge(QueryInstance)) Knowledge->GetRememberedThreatLocations(Locations);
    UEnvQueryItemType_Point::SetContextHelper(ContextData, Locations);
}
void UDMEnvQueryContext_LastKnownThreatLocation::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
    const UDMCombatKnowledgeComponent* Knowledge=ResolveKnowledge(QueryInstance);
    UEnvQueryItemType_Point::SetContextHelper(ContextData, Knowledge ? Knowledge->GetLastKnownThreatLocation() : FVector::ZeroVector);
}
void UDMEnvQueryContext_DamageOrigin::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
    const UDMCombatKnowledgeComponent* Knowledge=ResolveKnowledge(QueryInstance);
    UEnvQueryItemType_Point::SetContextHelper(ContextData, Knowledge ? Knowledge->GetDamageOrigin() : FVector::ZeroVector);
}
void UDMEnvQueryContext_FriendlyActors::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
    TArray<AActor*> Actors;
    ADeathMatchRobotAIController* AI = nullptr;
    ResolveKnowledge(QueryInstance, &AI);
    if (AI && AI->GetWorld())
    {
        if (UDMCombatPositionRegistry* Registry = AI->GetWorld()->GetSubsystem<UDMCombatPositionRegistry>())
        {
            TArray<FDMPositionNeighbor> Neighbors;
            Registry->Gather(AI, AI->GetWorld()->GetTimeSeconds(), Neighbors);
            for (const FDMPositionNeighbor& Neighbor : Neighbors)
                if (AActor* Actor = Neighbor.Owner.Get()) Actors.AddUnique(Actor);
        }
    }
    UEnvQueryItemType_Actor::SetContextHelper(ContextData, Actors);
}
