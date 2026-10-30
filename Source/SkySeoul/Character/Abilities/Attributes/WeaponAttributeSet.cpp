// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Abilities/Attributes/WeaponAttributeSet.h"
#include "Net/UnrealNetwork.h"

void UWeaponAttributeSet::OnRep_WeaponLevel(const FGameplayAttributeData& OldWeaponLevel)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponAttributeSet, WeaponLevel, OldWeaponLevel);
}

void UWeaponAttributeSet::OnRep_CurrentMagazine(const FGameplayAttributeData& OldCurrentMagazine)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponAttributeSet, CurrentMagazine, OldCurrentMagazine);
}

void UWeaponAttributeSet::OnRep_MaxMagazine(const FGameplayAttributeData& OldMaxMagazine)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponAttributeSet, MaxMagazine, OldMaxMagazine);
}

void UWeaponAttributeSet::OnRep_ReloadTime(const FGameplayAttributeData& OldReloadTime)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponAttributeSet, ReloadTime, OldReloadTime);
}

void UWeaponAttributeSet::OnRep_FireTime(const FGameplayAttributeData& OldFireTime)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponAttributeSet, FireTime, OldFireTime);
}

void UWeaponAttributeSet::OnRep_WeaponPowerLevel(const FGameplayAttributeData& OldWeaponPowerLevel)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWeaponAttributeSet, WeaponPowerLevel, OldWeaponPowerLevel);
}

void UWeaponAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponAttributeSet, WeaponLevel, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponAttributeSet, CurrentMagazine, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponAttributeSet, MaxMagazine, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponAttributeSet, ReloadTime, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponAttributeSet, FireTime, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponAttributeSet, WeaponPowerLevel, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWeaponAttributeSet, Distance, COND_None, REPNOTIFY_Always);
}
