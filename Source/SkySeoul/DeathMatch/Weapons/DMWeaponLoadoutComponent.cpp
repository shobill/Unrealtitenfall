#include "DMWeaponLoadoutComponent.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "DeathMatch/Core/DeathMatchLog.h"

#include "DeathMatch/Weapons/DMWeaponActor.h"
#include "Character/CharacterBase.h"
#include "Character/Abilities/CharacterAbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UDMWeaponLoadoutComponent::UDMWeaponLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDMWeaponLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bGrantStartingWeaponsOnBeginPlay)
	{
		GrantStartingWeapons();
	}
}

void UDMWeaponLoadoutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAllWeapons();
	Super::EndPlay(EndPlayReason);
}

void UDMWeaponLoadoutComponent::GrantStartingWeapons()
{
	for (const FDMWeaponDefinition& Def : StartingWeapons)
	{
		AcquireWeapon(Def);
	}

	if (OwnedWeapons.Num() > 0 && ActiveIndex == INDEX_NONE)
	{
		SelectWeaponByIndex(0);
	}
}

void UDMWeaponLoadoutComponent::ClearAllWeapons()
{
    if (const auto* Pawn=Cast<APawn>(GetOwner()))
        if (const auto* Controller=Pawn->GetController())
            if (auto* Fire=Controller->FindComponentByClass<UDMCombatFireControlComponent>()) Fire->ResetFireControl();
	ClearActiveWeaponAbilities();

	for (FDMWeaponRuntimeSlot& Slot : OwnedWeapons)
	{
		if (IsValid(Slot.SpawnedActor))
		{
			Slot.SpawnedActor->Destroy();
			Slot.SpawnedActor = nullptr;
		}
	}
	OwnedWeapons.Reset();
	ActiveIndex = INDEX_NONE;
}

USkeletalMeshComponent* UDMWeaponLoadoutComponent::GetOwnerMesh() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		return Character->GetMesh();
	}
	return nullptr;
}

int32 UDMWeaponLoadoutComponent::FindOwnedIndex(FGameplayTag WeaponId) const
{
	if (!WeaponId.IsValid())
	{
		return INDEX_NONE;
	}

	for (int32 i = 0; i < OwnedWeapons.Num(); ++i)
	{
		if (OwnedWeapons[i].Definition.WeaponId == WeaponId)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool UDMWeaponLoadoutComponent::HasWeapon(FGameplayTag WeaponId) const
{
	return FindOwnedIndex(WeaponId) != INDEX_NONE;
}

bool UDMWeaponLoadoutComponent::AcquireWeapon(const FDMWeaponDefinition& Definition)
{
	if (!Definition.IsValidDef())
	{
		UE_LOG(LogDM, Warning, TEXT("DMWeaponLoadout: AcquireWeapon failed — invalid definition."));
		return false;
	}

	if (HasWeapon(Definition.WeaponId))
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	USkeletalMeshComponent* OwnerMesh = GetOwnerMesh();
	UWorld* World = GetWorld();
	if (!OwnerActor || !OwnerMesh || !World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = Cast<APawn>(OwnerActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADMWeaponActor* WeaponActor = World->SpawnActor<ADMWeaponActor>(
		Definition.WeaponActorClass,
		OwnerMesh->GetComponentTransform(),
		SpawnParams
	);
	if (!WeaponActor)
	{
		UE_LOG(LogDM, Error, TEXT("DMWeaponLoadout: spawn failed for %s"), *Definition.WeaponId.ToString());
		return false;
	}

	WeaponActor->AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		Definition.AttachSocketName
	);
	WeaponActor->SetActorRelativeTransform(Definition.AttachRelativeTransform);
	WeaponActor->SetActorHiddenInGame(true);

	FDMWeaponRuntimeSlot& Slot = OwnedWeapons.AddDefaulted_GetRef();
	Slot.Definition = Definition;
	Slot.SpawnedActor = WeaponActor;

	const int32 NewIndex = OwnedWeapons.Num() - 1;
	OnWeaponAcquired.Broadcast(Definition.WeaponId, NewIndex);

	if (ActiveIndex == INDEX_NONE)
	{
		SelectWeaponByIndex(NewIndex);
	}

	return true;
}

bool UDMWeaponLoadoutComponent::RemoveWeapon(FGameplayTag WeaponId)
{
	const int32 Index = FindOwnedIndex(WeaponId);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	const bool bWasActive = (Index == ActiveIndex);
	if (bWasActive)
	{
        if (const auto* Pawn=Cast<APawn>(GetOwner()))
            if (const auto* Controller=Pawn->GetController())
                if (auto* Fire=Controller->FindComponentByClass<UDMCombatFireControlComponent>()) Fire->ResetFireControl();
		ClearActiveWeaponAbilities();
	}

	if (IsValid(OwnedWeapons[Index].SpawnedActor))
	{
		OwnedWeapons[Index].SpawnedActor->Destroy();
	}
	OwnedWeapons.RemoveAt(Index);

	if (OwnedWeapons.Num() == 0)
	{
		ActiveIndex = INDEX_NONE;
		#if !UE_BUILD_SHIPPING
        if (const auto* Pawn = Cast<APawn>(GetOwner()))
            if (const auto* Controller = Pawn->GetController())
                if (auto* Fire = Controller->FindComponentByClass<UDMCombatFireControlComponent>()) Fire->ResetFireControl();
#endif
        OnActiveWeaponChanged.Broadcast(FGameplayTag(), INDEX_NONE);
		return true;
	}

	if (bWasActive)
	{
		SelectWeaponByIndex(FMath::Clamp(Index, 0, OwnedWeapons.Num() - 1));
	}
	else if (ActiveIndex > Index)
	{
		--ActiveIndex;
	}

	return true;
}

bool UDMWeaponLoadoutComponent::CycleWeaponNext()
{
	if (OwnedWeapons.Num() <= 1)
	{
		return false;
	}
	return SelectWeaponByIndex((ActiveIndex + 1) % OwnedWeapons.Num());
}

bool UDMWeaponLoadoutComponent::CycleWeaponPrev()
{
	if (OwnedWeapons.Num() <= 1)
	{
		return false;
	}
	return SelectWeaponByIndex((ActiveIndex - 1 + OwnedWeapons.Num()) % OwnedWeapons.Num());
}

bool UDMWeaponLoadoutComponent::SelectWeaponByIndex(int32 Index)
{
	if (!OwnedWeapons.IsValidIndex(Index))
	{
		return false;
	}

	if (Index == ActiveIndex)
	{
		ApplyActiveWeaponVisuals();
		return true;
	}

    if (const auto* Pawn=Cast<APawn>(GetOwner()))
        if (const auto* Controller=Pawn->GetController())
            if (auto* Fire=Controller->FindComponentByClass<UDMCombatFireControlComponent>()) Fire->ResetFireControl();
	ActiveIndex = Index;
	ApplyActiveWeaponVisuals();
	RefreshActiveWeaponAbilities();
	#if !UE_BUILD_SHIPPING
        if (const auto* Pawn = Cast<APawn>(GetOwner()))
            if (const auto* Controller = Pawn->GetController())
                if (auto* Fire = Controller->FindComponentByClass<UDMCombatFireControlComponent>()) Fire->ResetFireControl();
#endif
        OnActiveWeaponChanged.Broadcast(GetActiveWeaponId(), ActiveIndex);
	return true;
}

bool UDMWeaponLoadoutComponent::SelectWeaponById(FGameplayTag WeaponId)
{
	return SelectWeaponByIndex(FindOwnedIndex(WeaponId));
}

void UDMWeaponLoadoutComponent::ApplyActiveWeaponVisuals()
{
	for (int32 i = 0; i < OwnedWeapons.Num(); ++i)
	{
		if (ADMWeaponActor* Actor = OwnedWeapons[i].SpawnedActor)
		{
			Actor->SetActorHiddenInGame(i != ActiveIndex);
		}
	}
}

void UDMWeaponLoadoutComponent::ClearActiveWeaponAbilities()
{
	if (!bHasActiveWeaponAbilityHandles)
	{
		return;
	}

	if (ACharacterBase* Character = Cast<ACharacterBase>(GetOwner()))
	{
		if (UCharacterAbilitySystemComponent* ASC =
			Cast<UCharacterAbilitySystemComponent>(Character->GetAbilitySystemComponent()))
		{
			ActiveWeaponAbilityHandles.TakeFromAbilitySystem(ASC);
		}
	}

	ActiveWeaponAbilityHandles = FAbilitySetData_GrantedHandles();
	bHasActiveWeaponAbilityHandles = false;
}

void UDMWeaponLoadoutComponent::RefreshActiveWeaponAbilities()
{
	ClearActiveWeaponAbilities();

	if (!OwnedWeapons.IsValidIndex(ActiveIndex))
	{
		return;
	}

	UAbilitySetData* Set = nullptr;
	if (ADMWeaponActor* Weapon = OwnedWeapons[ActiveIndex].SpawnedActor)
	{
		Set = Weapon->GetAbilitySetWhileActive();
	}
	if (!Set)
	{
		return;
	}

	ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
	if (!Character)
	{
		return;
	}

	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	Set->GiveToAbilitySystem(ASC, &ActiveWeaponAbilityHandles, GetOwner());
	bHasActiveWeaponAbilityHandles = true;
}

FGameplayTag UDMWeaponLoadoutComponent::GetActiveWeaponId() const
{
	return OwnedWeapons.IsValidIndex(ActiveIndex)
		? OwnedWeapons[ActiveIndex].Definition.WeaponId
		: FGameplayTag();
}

ADMWeaponActor* UDMWeaponLoadoutComponent::GetActiveWeaponActor() const
{
	return OwnedWeapons.IsValidIndex(ActiveIndex)
		? OwnedWeapons[ActiveIndex].SpawnedActor
		: nullptr;
}

FTransform UDMWeaponLoadoutComponent::GetActiveMuzzleTransform() const
{
	if (const ADMWeaponActor* Weapon = GetActiveWeaponActor())
	{
		return Weapon->GetMuzzleTransform();
	}
	if (const AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetActorTransform();
	}
	return FTransform::Identity;
}

bool UDMWeaponLoadoutComponent::GetOwnedWeaponDefinition(int32 Index, FDMWeaponDefinition& OutDef) const
{
	if (!OwnedWeapons.IsValidIndex(Index))
	{
		return false;
	}
	OutDef = OwnedWeapons[Index].Definition;
	return true;
}
