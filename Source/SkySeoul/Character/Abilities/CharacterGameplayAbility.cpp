// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Abilities/CharacterGameplayAbility.h"
#include "Character/Abilities/CharacterAbilitySystemComponent.h"

#include "Character/CharacterBase.h"

UCharacterGameplayAbility::UCharacterGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	//죽거나 스턴일때 실행되지 않음
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Debuff.Stun")));
}

UCharacterAbilitySystemComponent* UCharacterGameplayAbility::GetCharacterAbilitySystemComponentFromActorInfo() const
{
	return CurrentActorInfo ? Cast<UCharacterAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()) : nullptr;
}

APlayerController* UCharacterGameplayAbility::GetPlayerControllerFromActorInfo() const
{
	return CurrentActorInfo ? Cast<APlayerController>(CurrentActorInfo->PlayerController.Get()) : nullptr;;
}

AController* UCharacterGameplayAbility::GetControllerFromActorInfo() const
{
	if (CurrentActorInfo)
	{
		if (AController* PC = CurrentActorInfo->PlayerController.Get())
		{
			return PC;
		}
		// Look for a player controller or pawn in the owner chain.
		AActor* TestActor = CurrentActorInfo->OwnerActor.Get();
		while (TestActor)
		{
			if (AController* C = Cast<AController>(TestActor))
			{
				return C;
			}
			if (APawn* Pawn = Cast<APawn>(TestActor))
			{
				return Pawn->GetController();
			}
			TestActor = TestActor->GetOwner();
		}
	}
	return nullptr;
}

ACharacterBase* UCharacterGameplayAbility::GetCharacterFromActorInfo() const
{
	return (CurrentActorInfo ? Cast<ACharacterBase>(CurrentActorInfo->AvatarActor.Get()) : nullptr);
}

void UCharacterGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfor, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfor, Spec);
	if (ActivateAbilityOnGranted)
	{
		ActorInfor->AbilitySystemComponent->TryActivateAbility(Spec.Handle, false);
	}
}
