#include "AnimNotify_DMNamed.h"
#include "Animation/AnimMontage.h"
#include "Animation/ActiveMontageInstanceScope.h"

FString UAnimNotify_DMNamed::InspectFireMontage(UAnimMontage* Montage)
{
    FString Result;
#if WITH_EDITOR
    if (!Montage) return Result;
    for (const auto& Event : Montage->Notifies)
        Result += FString::Printf(TEXT("Notify=%s time=%.4f type=%s; "), *Event.NotifyName.ToString(), Event.GetTriggerTime(), *GetNameSafe(Event.Notify));
    for (const auto& Track : Montage->SlotAnimTracks)
    {
        Result += TEXT("Slot=") + Track.SlotName.ToString() + TEXT("; ");
        for (const auto& Segment : Track.AnimTrack.AnimSegments)
            Result += FString::Printf(TEXT("Sequence=%s RootMotion=%d; "), *GetNameSafe(Segment.GetAnimReference()), Segment.GetAnimReference() && Segment.GetAnimReference()->HasRootMotion());
    }
    for (const auto& Section : Montage->CompositeSections)
        Result += FString::Printf(TEXT("Section=%s time=%.3f; "), *Section.SectionName.ToString(), Section.GetTime());
#endif
    return Result;
}
bool UAnimNotify_DMNamed::AddFireCycleNotify(UAnimMontage* Montage, float Time)
{
#if WITH_EDITOR
    if (!Montage || Time <= 0 || Time >= Montage->GetPlayLength()) return false;
    for (const auto& Event : Montage->Notifies)
        if (Event.NotifyName == TEXT("FireCycle") || (Cast<UAnimNotify_DMNamed>(Event.Notify) && Cast<UAnimNotify_DMNamed>(Event.Notify)->NotifyName == TEXT("FireCycle"))) return true;
    Montage->Modify();
    FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
    auto* Notify = NewObject<UAnimNotify_DMNamed>(Montage, NAME_None, RF_Transactional);
    Notify->NotifyName = TEXT("FireCycle"); Event.Notify = Notify; Event.NotifyName = Notify->NotifyName;
    Event.Link(Montage, Time); Event.TriggerTimeOffset = GetTriggerTimeOffsetForType(Montage->CalculateOffsetForNotify(Time));
    Event.bTriggerOnDedicatedServer = true;
    Montage->MarkPackageDirty(); return true;
#else
    return false;
#endif
}

int32 UAnimNotify_DMNamed::UpgradeMontageFireNotifies(UAnimMontage* Montage)
{
	int32 Count = 0;
#if WITH_EDITOR
	if (!Montage) return Count;
	for (FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (!Event.Notify && !Event.NotifyStateClass && Event.NotifyName == TEXT("Fire"))
		{
			Montage->Modify();
			Event.Notify = NewObject<UAnimNotify_DMNamed>(Montage, NAME_None, RF_Transactional);
			++Count;
		}
	}
	if (Count) Montage->MarkPackageDirty();
#endif
	return Count;
}

#include "DeathMatch/Abilities/DMGameplayAbility_Fire.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Abilities/GameplayAbility.h"

void UAnimNotify_DMNamed::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || NotifyName.IsNone())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	if (!ASC)
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}

		TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
		for (UGameplayAbility* Instance : Instances)
		{
			if (UDMGameplayAbility_Fire* Fire = Cast<UDMGameplayAbility_Fire>(Instance))
			{
                const auto* Context = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
				Fire->HandleNamedAnimNotify(NotifyName, Context ? Context->MontageInstanceID : INDEX_NONE);
			}
		}
	}
}

#if WITH_EDITOR
FString UAnimNotify_DMNamed::GetNotifyName_Implementation() const
{
	return NotifyName.IsNone() ? TEXT("DM Named") : NotifyName.ToString();
}
#endif
