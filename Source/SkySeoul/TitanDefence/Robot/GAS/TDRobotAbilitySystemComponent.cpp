#include "TitanDefence/Robot/GAS/TDRobotAbilitySystemComponent.h"

UTDRobotAbilitySystemComponent::UTDRobotAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicated(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

