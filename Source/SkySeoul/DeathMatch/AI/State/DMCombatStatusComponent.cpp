#include "DMCombatStatusComponent.h"

#include "DeathMatch/AI/Knowledge/DMCombatKnowledgeComponent.h"
#include "DeathMatch/Core/DMHealthResolver.h"
#include "AIController.h"

UDMCombatStatusComponent::UDMCombatStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = .1f;
}

void UDMCombatStatusComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshStatus();
}

void UDMCombatStatusComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshStatus();
}

void UDMCombatStatusComponent::RefreshStatus()
{
	const AAIController* AI = Cast<AAIController>(GetOwner());
	const AActor* Avatar = AI ? AI->GetPawn() : GetOwner();
	const FDMHealthSnapshot Health = DMHealth::Resolve(Avatar);
	HealthRatio = Health.bValid ? Health.HealthRatio : 1.f;
	bDead = Health.bValid && !Health.bAlive;
	RiskTier = HealthRatio <= .25f ? EDMHealthRiskTier::Critical
		: HealthRatio <= .5f ? EDMHealthRiskTier::Dangerous
		: HealthRatio <= .75f ? EDMHealthRiskTier::Caution
		: EDMHealthRiskTier::Safe;

	const UDMCombatKnowledgeComponent* Knowledge = GetOwner()
		? GetOwner()->FindComponentByClass<UDMCombatKnowledgeComponent>() : nullptr;
	const bool bVisible = Knowledge && Knowledge->HasVisibleTarget();
	const bool bRemembered = Knowledge && Knowledge->HasRememberedTarget();
	bEmergency = !bDead && bRemembered && HealthRatio <= EmergencyHealthRatio;
	bShouldRetreat = !bDead && bRemembered && HealthRatio <= RetreatHealthRatio;
	bNeedsCombatCover = !bDead && bVisible && RiskTier >= EDMHealthRiskTier::Dangerous;
	bShouldInvestigate = !bDead && bRemembered && !bVisible;
	bShouldDisengage = !bDead && !bRemembered;
	AIState = bDead ? EDMCombatAIState::Dead
		: bVisible ? EDMCombatAIState::Combat
		: bRemembered ? EDMCombatAIState::Alert
		: EDMCombatAIState::Idle;
}

void UDMCombatStatusComponent::ResetStatus()
{
	HealthRatio = 1.f;
	bDead = false;
	RiskTier = EDMHealthRiskTier::Safe;
	AIState = EDMCombatAIState::Idle;
	bEmergency = false;
	bShouldRetreat = false;
	bNeedsCombatCover = false;
	bShouldInvestigate = false;
	bShouldDisengage = true;
}
