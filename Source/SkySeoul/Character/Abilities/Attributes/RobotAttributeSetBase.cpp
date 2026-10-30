// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Abilities/Attributes/RobotAttributeSetBase.h"
#include "Net/UnrealNetwork.h"

void URobotAttributeSetBase::OnRep_RobotCost(const FGameplayAttributeData& OldRobotCost)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URobotAttributeSetBase, RobotCost, OldRobotCost);
}

void URobotAttributeSetBase::OnRep_CurrentHackingPercent(const FGameplayAttributeData& OldCurrentHackingPercent)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URobotAttributeSetBase, CurrentHackingPercent, OldCurrentHackingPercent);
}

void URobotAttributeSetBase::OnRep_MaxHackingPercent(const FGameplayAttributeData& OldMaxHackingPercent)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URobotAttributeSetBase, MaxHackingPercent, OldMaxHackingPercent);
}

void URobotAttributeSetBase::OnRep_MaxAttackRange(const FGameplayAttributeData& OldMaxAttackRange)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URobotAttributeSetBase, MaxAttackRange, OldMaxAttackRange);
}

void URobotAttributeSetBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(URobotAttributeSetBase, RobotCost, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URobotAttributeSetBase, CurrentHackingPercent, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URobotAttributeSetBase, MaxHackingPercent, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URobotAttributeSetBase, MaxAttackRange, COND_None, REPNOTIFY_Always);
}
