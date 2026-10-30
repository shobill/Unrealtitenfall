#include "DMGameplayAbility_Fire.h"
#include "DeathMatch/AI/Fire/DMCombatFireControlComponent.h"
#include "DeathMatch/AI/Combat/DMCombatFire_Adaptive.h"
#include "DeathMatch/AI/Combat/DMCombatDebug.h"

#include "PartsCharacter/PartsCharacterBase.h"
#include "PartsCharacter/PartsComponent.h"
#include "PartsCharacter/PartsDamageManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/CharacterBase.h"
#include "DeathMatch/Character/DeathMatchPlayerCharacter.h"
#include "DeathMatch/Controller/DeathMatchRobotAIController.h"
#include "DeathMatch/Weapons/DMWeaponLoadoutComponent.h"
#include "DeathMatch/Weapons/DMAimZoomComponent.h"
#include "DeathMatch/Weapons/DMWeaponActor.h"
#include "DeathMatch/Character/DMAimTargetComponent.h"
#include "DeathMatch/Core/DeathMatchTeamUtils.h"
#include "DeathMatch/Core/DeathMatchLog.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimCompositeBase.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DeathMatch/Abilities/AnimNotify_DMNamed.h"
#include "TimerManager.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "GenericTeamAgentInterface.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameplayTagsManager.h"
#include "Particles/ParticleSystem.h"

UDMGameplayAbility_Fire::UDMGameplayAbility_Fire()
{
	bRequireCanShoot = true;
	// Semi-auto: press edge only (BP WhileInputActive would spam every tick).
	ActivationPolicy = ESkySeoulAbilityActivationPolicy::OnInputTriggered;
	bRetriggerInstancedAbility = false;

	const FGameplayTag FireTag = FGameplayTag::RequestGameplayTag(FName("Ability.DeathMatch.Fire"), false);
	if (FireTag.IsValid())
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(FireTag);
		SetAssetTags(Tags);

		// While this shot is active, block other Fire activations (AI BT spam).
		BlockAbilitiesWithTag.AddTag(FireTag);
	}
}

void UDMGameplayAbility_Fire::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	// BP child CDOs may override ActivationPolicy — force semi-auto.
	ActivationPolicy = ESkySeoulAbilityActivationPolicy::OnInputTriggered;
	bRetriggerInstancedAbility = false;
	Super::OnAvatarSet(ActorInfo, Spec);
}

bool UDMGameplayAbility_Fire::IsPlayerAimingAllowed(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!bRequirePlayerAiming || !ActorInfo)
	{
		return true;
	}

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	if (!Avatar)
	{
		return true;
	}

	const UDMAimZoomComponent* AimZoom = UDMAimZoomComponent::FindOnActor(Avatar);
	// No AimZoom component → not gated (legacy pawns).
	if (!AimZoom)
	{
		return true;
	}

	return AimZoom->IsAiming();
}

const UDMGameplayAbility_Fire* UDMGameplayAbility_Fire::ResolveInstanceForCanActivate(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo)
	{
		return this;
	}

	if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		if (const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle))
		{
			if (const UDMGameplayAbility_Fire* Inst = Cast<UDMGameplayAbility_Fire>(Spec->GetPrimaryInstance()))
			{
				return Inst;
			}
		}
	}

	return this;
}

bool UDMGameplayAbility_Fire::IsFireMontagePlaying(const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!FireMontage || !ActorInfo)
	{
		return false;
	}

	const UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	return AnimInstance && AnimInstance->Montage_IsPlaying(FireMontage);
}

void UDMGameplayAbility_Fire::SetLegacyNextFireAllowedAfter(float ExtraDelaySeconds)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Candidate = World->GetTimeSeconds() + FMath::Max(0.0f, ExtraDelaySeconds);
	LegacyNextFireAllowedTimeSeconds = FMath::Max(LegacyNextFireAllowedTimeSeconds, Candidate);
}

bool UDMGameplayAbility_Fire::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
    const auto Reject = [&](const TCHAR* Gate)
    {
        const auto* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
        const auto* Tactics = Pawn && Pawn->GetController() ? Pawn->GetController()->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr;
		DM_FIRE_LOG(Pawn, Tactics ? Tactics->GetFireAttemptId() : 0,
			TEXT("AbilityGateRejected"), FString(Gate));
        return false;
    };
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
        return Reject(TEXT("AbilitySystemOrParts"));
	}

    if (!IsCombatPatternAllowed(ActorInfo))
    {
        const auto* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
        const auto* Tactics = Pawn && Pawn->GetController() ? Pawn->GetController()->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr;
        return Reject(Tactics ? DMFireAttempt::Name(Tactics->GetFireFailure()) : TEXT("Tactics"));
    }
    if (!IsPlayerAimingAllowed(ActorInfo)) return Reject(TEXT("AimZoom"));

	const UDMGameplayAbility_Fire* Inst = ResolveInstanceForCanActivate(Handle, ActorInfo);
    if (Inst->bExecutingFire) return Reject(TEXT("ShotCallbackActive"));

	if (ActorInfo)
	{
		if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			if (const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle))
			{
				if (Spec->IsActive())
				{
					return Reject(TEXT("AbilityActive"));
				}
			}
		}
	}

	if (Inst->IsFireMontagePlaying(ActorInfo))
	{
        return Reject(TEXT("MontageActive"));
	}

	const UWorld* World = ActorInfo && ActorInfo->AvatarActor.IsValid()
		? ActorInfo->AvatarActor->GetWorld()
		: nullptr;
	const APawn* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UDMCombatFireControlComponent* FireControl = Pawn && Pawn->GetController()
		? Pawn->GetController()->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr;
	const bool bUsesFireControlTiming = FireControl && FireControl->bEnabled && FireControl->FireModule;
	if (!bUsesFireControlTiming && World && World->GetTimeSeconds() < Inst->LegacyNextFireAllowedTimeSeconds)
	{
        return Reject(TEXT("ShotCooldown"));
	}

	return true;
}

void UDMGameplayAbility_Fire::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bFireNotifyDiscovered = bFireNotifyReceived = bFireTimerReceived = bShotAttempted = false;
    bEndingFireAbility = false;
    bLegacySingleWithoutCycle = false;
    bShotCooldownCommitted = false;
	bShotOutcomeLogged = false;
    LastFireAttemptFailure = EDMFireAttemptFailure::None;
    LastShotResult = FDMShotResult();
    auto* AttemptPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
    auto* AttemptTactics = AttemptPawn && AttemptPawn->GetController() ? AttemptPawn->GetController()->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr;
    FireAttemptId = AttemptTactics ? AttemptTactics->EnsureFireAttempt() : DMFireAttempt::NextId();
	UE_LOG(LogDM, Log, TEXT("FireAbility Activated attempt=%llu cycle=%d owner=%s"),
		FireAttemptId, AttemptTactics ? AttemptTactics->FireCycle.CycleId : INDEX_NONE,
		*GetNameSafe(GetAvatarActorFromActorInfo()));
    DM_FIRE_LOG(GetAvatarActorFromActorInfo(), FireAttemptId, TEXT("AbilityActivated"));
    if (const auto* Character = Cast<ACharacterBase>(GetAvatarActorFromActorInfo()))
        DM_FIRE_LOG(Character, FireAttemptId, TEXT("AvatarHealth"), FString::Printf(TEXT("bodyHP=%.1f gasHP=%.1f dead=%d"), Character->CurHP, Character->GetHealth(), Character->IsDead));
	bFiredThisActivation = false;
	bMontagePathActive = false;
	bFinishFromMontageHandled = false;

	if (!IsCombatPatternAllowed(ActorInfo) || !IsPlayerAimingAllowed(ActorInfo))
	{
        LastFireAttemptFailure = AttemptTactics && AttemptTactics->bEnabled
            ? AttemptTactics->GetFireFailure() : EDMFireAttemptFailure::None;
        if (LastFireAttemptFailure == EDMFireAttemptFailure::None) LastFireAttemptFailure = EDMFireAttemptFailure::AimZoom;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
        LastFireAttemptFailure = EDMFireAttemptFailure::Commit;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (TriggerEventData && TriggerEventData->Target)
	{
		DesiredTarget = const_cast<AActor*>(TriggerEventData->Target.Get());
	}

	AActor* Target = GetDesiredTarget();
	if (bRequireTarget && !IsValid(Target))
	{
        LastFireAttemptFailure = EDMFireAttemptFailure::InvalidTarget;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!UsesAuthoritativeFireControl(AttemptTactics, false)
		&& EvaluateLegacyRangeFailure(Avatar, Target) == EDMFireAttemptFailure::Range)
	{
		LastFireAttemptFailure = EDMFireAttemptFailure::Range;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Face aim before montage so the fire pose is not played toward walk/back direction.
	if (Avatar)
	{
		FaceTowardFireAim(Avatar, GetFireAimLocation(Avatar, Target));
	}

	const bool bUseNotifyPath = bFireOnAnimNotify && FireMontage != nullptr && FireNotifyName != NAME_None;
    if (auto* Tactics = GetCombatBrain())
    {
        ActiveCombatBrain = Tactics;
        ActiveFireAction = Tactics->FireModule;
        ActiveCycle = &Tactics->FireCycle;
        CycleTarget = Target; CycleWeapon = ResolveActiveWeapon(Avatar);
        CycleController = AttemptPawn ? AttemptPawn->GetController() : nullptr;
        if (!Avatar || !Avatar->HasAuthority() || !bUseNotifyPath || !ActorInfo->GetAnimInstance() || !Tactics->AuthorizeFireCycle(true))
        {
            if (ActiveCycle->LastFailureReason == EDMFireAttemptFailure::None) ActiveCycle->LastFailureReason = EDMFireAttemptFailure::Montage;
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return;
        }
        DesiredTarget = Target;
        StartFireCycleMontage();
        if (IsActive()) GetWorld()->GetTimerManager().SetTimer(SafetyTimer, this, &UDMGameplayAbility_Fire::CheckCycleSafety, .03f, true);
        return;
    }
	if (!bUseNotifyPath)
	{
		const bool bFired = ExecutePatternFire(Target);
		if (bFired)
		{
			bFiredThisActivation = true;
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, !bFired);
		return;
	}

	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		const bool bFired = ExecutePatternFire(Target);
		if (bFired)
		{
			bFiredThisActivation = true;
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, !bFired);
		return;
	}

	// Active ability and montage checks prevent reentry without spending shot cooldown.

	bMontagePathActive = true;
	BindMontageNotify(AnimInstance);
	ScheduleFireNotifyTimers();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		FireMontage,
		MontagePlayRate,
		MontageStartSection,
		/*bStopWhenAbilityEnds*/ true);

	MontageTask->OnCompleted.AddDynamic(this, &UDMGameplayAbility_Fire::OnFireMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UDMGameplayAbility_Fire::OnFireMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &UDMGameplayAbility_Fire::OnFireMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UDMGameplayAbility_Fire::OnFireMontageCancelled);
	MontageTask->ReadyForActivation();
    if (IsActive() && AnimInstance->Montage_IsPlaying(FireMontage))
        DM_FIRE_LOG(Avatar, FireAttemptId, TEXT("MontageStarted"), GetNameSafe(FireMontage));
}

void UDMGameplayAbility_Fire::BindMontageNotify(UAnimInstance* AnimInstance)
{
	UnbindMontageNotify();
	if (!AnimInstance)
	{
		return;
	}

	BoundAnimInstance = AnimInstance;
	AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &UDMGameplayAbility_Fire::OnFireMontageNotifyBegin);
}

void UDMGameplayAbility_Fire::UnbindMontageNotify()
{
	if (UAnimInstance* AnimInstance = BoundAnimInstance.Get())
	{
		AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UDMGameplayAbility_Fire::OnFireMontageNotifyBegin);
	}
	BoundAnimInstance.Reset();
}

void UDMGameplayAbility_Fire::OnFireMontageNotifyBegin(
	FName NotifyName,
	const FBranchingPointNotifyPayload& BranchingPointPayload)
{
    if (ActiveCycle && BranchingPointPayload.MontageInstanceID != CycleMontageInstanceId) return;
	HandleNamedAnimNotify(NotifyName, BranchingPointPayload.MontageInstanceID);
}

bool UDMGameplayAbility_Fire::DoesNotifyNameMatch(FName IncomingNotifyName) const
{
	if (IncomingNotifyName.IsNone() || FireNotifyName.IsNone())
	{
		return false;
	}

	return IncomingNotifyName.IsEqual(FireNotifyName, ENameCase::IgnoreCase);
}

bool UDMGameplayAbility_Fire::DoesNotifyEventMatch(const FAnimNotifyEvent& NotifyEvent) const
{
	if (DoesNotifyNameMatch(NotifyEvent.NotifyName))
	{
		return true;
	}

	if (const UAnimNotify_DMNamed* Named = Cast<UAnimNotify_DMNamed>(NotifyEvent.Notify))
	{
		return DoesNotifyNameMatch(Named->NotifyName);
	}

	if (NotifyEvent.Notify)
	{
		const FString DisplayName = NotifyEvent.Notify->GetNotifyName();
		if (!DisplayName.IsEmpty() && DoesNotifyNameMatch(FName(*DisplayName)))
		{
			return true;
		}
	}

	return false;
}

void UDMGameplayAbility_Fire::CollectFireNotifyTriggerTimes(TArray<float>& OutMontageTimes) const
{
	OutMontageTimes.Reset();
	if (!FireMontage)
	{
		return;
	}

	auto AddUniqueTime = [&OutMontageTimes](float Time)
	{
		for (const float Existing : OutMontageTimes)
		{
			if (FMath::IsNearlyEqual(Existing, Time, 0.001f))
			{
				return;
			}
		}
		OutMontageTimes.Add(Time);
	};

	for (const FAnimNotifyEvent& Ev : FireMontage->Notifies)
	{
		if (DoesNotifyEventMatch(Ev))
		{
			AddUniqueTime(Ev.GetTriggerTime());
		}
	}

	// Notifies placed on source sequences inside montage slots.
	for (const FSlotAnimationTrack& SlotTrack : FireMontage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : SlotTrack.AnimTrack.AnimSegments)
		{
			const TObjectPtr<UAnimSequenceBase>& SeqPtr = Segment.GetAnimReference();
			UAnimSequenceBase* Sequence = SeqPtr.Get();
			if (!Sequence)
			{
				continue;
			}

			const float PlayRate = FMath::Max(Segment.AnimPlayRate, 0.01f);
			for (const FAnimNotifyEvent& Ev : Sequence->Notifies)
			{
				if (!DoesNotifyEventMatch(Ev))
				{
					continue;
				}

				const float LocalTime = Ev.GetTriggerTime();
				if (LocalTime + KINDA_SMALL_NUMBER < Segment.AnimStartTime
					|| LocalTime - KINDA_SMALL_NUMBER > Segment.AnimEndTime)
				{
					continue;
				}

				const float MontageTime = Segment.StartPos + (LocalTime - Segment.AnimStartTime) / PlayRate;
				AddUniqueTime(MontageTime);
			}
		}
	}

	OutMontageTimes.Sort();
}

void UDMGameplayAbility_Fire::ScheduleFireNotifyTimers()
{
	ClearFireNotifyTimers();

	if (!bFireOnAnimNotify)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CollectFireNotifyTriggerTimes(FireNotifyTriggerTimes);
    bFireNotifyDiscovered = !FireNotifyTriggerTimes.IsEmpty();
    DM_FIRE_LOG(GetAvatarActorFromActorInfo(), FireAttemptId, TEXT("NotifyDiscovered"),
        FString::Printf(TEXT("found=%d count=%d montage=%s"), bFireNotifyDiscovered, FireNotifyTriggerTimes.Num(), *GetNameSafe(FireMontage)));
    if (!bScheduleFireFromMontageNotifyTimes) return;

	if (FireNotifyTriggerTimes.Num() == 0)
	{
		UE_LOG(LogDM, Warning,
			TEXT("DM Fire: %s montage '%s' has no notify matching '%s' (check Montage Notify / DM Named Notify / sequence notifies)."),
			*GetName(),
			*GetNameSafe(FireMontage),
			*FireNotifyName.ToString());
		return;
	}

	const float Rate = FMath::Max(MontagePlayRate, 0.01f);
	float SectionStart = 0.f, SectionEnd = FireMontage->GetPlayLength();
    const int32 SectionIndex = FireMontage->GetSectionIndex(MontageStartSection);
    if (ActiveCycle && SectionIndex != INDEX_NONE) FireMontage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
	for (int32 NotifyIndex = 0; NotifyIndex < FireNotifyTriggerTimes.Num(); ++NotifyIndex)
	{
		const float MontageTime = FireNotifyTriggerTimes[NotifyIndex];
        if (ActiveCycle && (MontageTime < SectionStart || MontageTime >= SectionEnd)) continue;
		const float Delay = FMath::Max(KINDA_SMALL_NUMBER, (MontageTime - SectionStart) / Rate);
		FTimerHandle Handle;
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindUObject(this, &UDMGameplayAbility_Fire::OnScheduledFireNotify, NotifyIndex);
        if (ActiveCycle)
        {
            const int32 Id = ActiveCycle->CycleId;
            TimerDelegate = FTimerDelegate::CreateWeakLambda(this, [this, Id, NotifyIndex]()
            { if (ActiveCycle && ActiveCycle->CycleId == Id) OnScheduledFireNotify(NotifyIndex); });
        }
		World->GetTimerManager().SetTimer(
			Handle,
			TimerDelegate,
			Delay,
			false);
		FireNotifyTimerHandles.Add(Handle);
	}

	UE_CLOG(DMIsCombatDiagEnabled(), LogDM, Log,
		TEXT("DM Fire: %s scheduled %d shot(s) from notify times on '%s' (name='%s')."),
		*GetName(),
		FireNotifyTimerHandles.Num(),
		*GetNameSafe(FireMontage),
		*FireNotifyName.ToString());
}

void UDMGameplayAbility_Fire::ClearFireNotifyTimers()
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Handle : FireNotifyTimerHandles)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
	FireNotifyTimerHandles.Reset();
	FireNotifyTriggerTimes.Reset();
	ProcessedFireNotifyIndices.Reset();
}

void UDMGameplayAbility_Fire::OnScheduledFireNotify(int32 NotifyIndex)
{
	if (!IsActive() || !bMontagePathActive)
	{
		return;
	}

	bFireTimerReceived = true;
    if (ActiveCycle)
    {
        const auto* Instance = BoundAnimInstance.IsValid() ? BoundAnimInstance->GetActiveInstanceForMontage(FireMontage) : nullptr;
        if (!Instance || Instance->GetInstanceID() != CycleMontageInstanceId)
        {
            ActiveCycle->LastFailureReason = EDMFireAttemptFailure::Montage;
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
            return;
        }
    }
    DM_FIRE_LOG(GetAvatarActorFromActorInfo(), FireAttemptId, TEXT("NotifyReceived"), FString::Printf(TEXT("source=Timer index=%d"), NotifyIndex));
    HandleFireFromNotify(NotifyIndex);
}

int32 UDMGameplayAbility_Fire::ResolveNotifyIndexFromCurrentMontagePosition() const
{
	if (FireNotifyTriggerTimes.Num() == 0)
	{
		return INDEX_NONE;
	}

	const UAnimInstance* AnimInstance = BoundAnimInstance.Get();
	const float MontagePosition = AnimInstance && FireMontage
		? AnimInstance->Montage_GetPosition(FireMontage)
		: -1.0f;
	int32 BestIndex = INDEX_NONE;
	float BestDistance = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < FireNotifyTriggerTimes.Num(); ++Index)
	{
		const float Distance = MontagePosition >= 0.0f
			? FMath::Abs(FireNotifyTriggerTimes[Index] - MontagePosition)
			: static_cast<float>(Index);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestIndex = Index;
		}
	}
	return BestIndex;
}

void UDMGameplayAbility_Fire::HandleNamedAnimNotify(FName IncomingNotifyName, int32 MontageInstanceId)
{
    if (ActiveCycle && MontageInstanceId != CycleMontageInstanceId) return;
	if (!IsActive() || !bMontagePathActive)
	{
		return;
	}

    if (IncomingNotifyName == FireCycleNotifyName)
    { if (ActiveCycle) HandleFireCycle(); return; }
	if (!DoesNotifyNameMatch(IncomingNotifyName))
	{
		UE_LOG(LogDM, Warning,
			TEXT("DM Fire: %s got notify '%s' but expect '%s'"),
			*GetName(),
			*IncomingNotifyName.ToString(),
			*FireNotifyName.ToString());
		return;
	}

	bFireNotifyReceived = true;
    DM_FIRE_LOG(GetAvatarActorFromActorInfo(), FireAttemptId, TEXT("NotifyReceived"), TEXT("source=Callback"));
    HandleFireFromNotify(ResolveNotifyIndexFromCurrentMontagePosition());
}

void UDMGameplayAbility_Fire::HandleFireFromNotify(int32 NotifyIndex)
{
    if (bEndingFireAbility || (ActiveCycle && (bShotAttempted || bCycleQueued))) return;
    if (ActiveCycle)
    { ActiveCycle->FireNotifyReceived = true; ActiveCycle->CycleState = EDMFireCycleState::ShotResolved; }
    if (bShotAttempted && !bAllowMultipleFireNotifies) return;
	if (NotifyIndex != INDEX_NONE)
	{
		if (ProcessedFireNotifyIndices.Contains(NotifyIndex))
		{
			return;
		}
		// Consume before executing: one authored notify represents one shot attempt,
		// even when both the runtime callback and fallback timer deliver it.
		ProcessedFireNotifyIndices.Add(NotifyIndex);
	}
	UE_LOG(LogDM, Log, TEXT("FireNotify Received attempt=%llu cycle=%d owner=%s"),
		FireAttemptId, ActiveCycle ? ActiveCycle->CycleId : INDEX_NONE,
		*GetNameSafe(GetAvatarActorFromActorInfo()));

	// A failed notify spends neither weapon cadence nor the successful-shot cooldown.
	if (ExecutePatternFire(GetDesiredTarget()))
	{
		bFiredThisActivation = true;
	}
    else if (ActiveCycle)
    { ActiveCycle->LastFailureReason = LastFireAttemptFailure; QueueFireCycle(true); }
    if (ActiveCombatBrain) ActiveCombatBrain->PublishFireCycle();
}

void UDMGameplayAbility_Fire::FinishFromMontage(bool bWasCancelled)
{
    if (bEndingFireAbility) return;
    if (ActiveCycle && bCycleQueued && !bWasCancelled) return;
    if (ActiveCycle && !bWasCancelled && (!ActiveCycle->FireNotifyReceived
        || (!ActiveCycle->FireCycleNotifyReceived && !bLegacySingleWithoutCycle)))
    {
        ActiveCycle->LastFailureReason = LastFireAttemptFailure = !ActiveCycle->FireNotifyReceived
            ? EDMFireAttemptFailure::NoNotifyReceived : EDMFireAttemptFailure::MissingFireCycleNotify;
        UE_LOG(LogDM, Warning, TEXT("Fire Cycle %d ended with missing %s: %s"), ActiveCycle->CycleId,
            ActiveCycle->FireNotifyReceived ? TEXT("FireCycle") : TEXT("Fire"), *GetNameSafe(FireMontage));
        bWasCancelled = true;
    }
	if (!IsActive() || bFinishFromMontageHandled)
	{
		return;
	}

	bFinishFromMontageHandled = true;
	bMontagePathActive = false;
	ClearFireNotifyTimers();

    if (!bWasCancelled && !bFireNotifyReceived && !bFireTimerReceived)
    {
        LastFireAttemptFailure = EDMFireAttemptFailure::NoNotifyReceived;
        UE_LOG(LogDM, Warning, TEXT("DM FireAttempt id=%llu NoNotifyReceived discovered=%d montage=%s fallback=%d"),
            FireAttemptId, bFireNotifyDiscovered, *GetNameSafe(FireMontage), bFallbackFireIfNotifyMissed);
        if (bFallbackFireIfNotifyMissed && !ActiveCycle)
            bFiredThisActivation |= ExecutePatternFire(GetDesiredTarget());
    }
    else if (!bWasCancelled && !bFiredThisActivation)
    {
        UE_LOG(LogDM, Warning, TEXT("DM FireAttempt id=%llu NotifyReceivedButBlocked gate=%s callback=%d timer=%d attempted=%d"),
            FireAttemptId, DMFireAttempt::Name(LastFireAttemptFailure), bFireNotifyReceived, bFireTimerReceived, bShotAttempted);
    }
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled || !bFiredThisActivation);
}

void UDMGameplayAbility_Fire::OnFireMontageCompleted()
{
	FinishFromMontage(false);
}

void UDMGameplayAbility_Fire::OnFireMontageBlendOut()
{
	// Keep ability alive through blend-out; OnCompleted ends it.
}

void UDMGameplayAbility_Fire::OnFireMontageInterrupted()
{
	FinishFromMontage(true);
}

void UDMGameplayAbility_Fire::OnFireMontageCancelled()
{
    if (!bShotAttempted) LastFireAttemptFailure = EDMFireAttemptFailure::Montage;
	FinishFromMontage(true);
}

void UDMGameplayAbility_Fire::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
    if (bEndingFireAbility) return;
    bEndingFireAbility = true;
    bMontagePathActive = false;
    ClearFireNotifyTimers();
    UnbindMontageNotify();
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(CycleTimer);
        GetWorld()->GetTimerManager().ClearTimer(CycleNotifyTimer);
        GetWorld()->GetTimerManager().ClearTimer(CycleWatchdogTimer);
        GetWorld()->GetTimerManager().ClearTimer(SafetyTimer);
    }
    if (ActiveCycle)
    {
        if (bWasCancelled && ActiveCycle->LastFailureReason == EDMFireAttemptFailure::None)
            ActiveCycle->LastFailureReason = LastFireAttemptFailure != EDMFireAttemptFailure::None
                ? LastFireAttemptFailure : EDMFireAttemptFailure::Cancelled;
        LastFireAttemptFailure = ActiveCycle->LastFailureReason;
        ActiveCycle->CycleState = ActiveCycle->bGroupComplete && ActiveCycle->Settings.Pattern == EDMAdaptiveFirePattern::Burst
            && GetWorld() && GetWorld()->GetTimeSeconds() < ActiveCycle->NextShotTime ? EDMFireCycleState::BurstPause : EDMFireCycleState::Ending;
        ++ActiveCycle->CycleId; // Invalidate all already-delivered timer callbacks.
        if (ActiveCombatBrain) ActiveCombatBrain->PublishFireCycle();
    }
	UDMCombatFireControlComponent* FailureControl = ActiveCombatBrain ? ActiveCombatBrain.Get() : GetCombatBrain();
	if ((bWasCancelled || !LastShotResult.bShotExecuted) && FailureControl)
	{
		const EDMFireAttemptFailure Failure = LastFireAttemptFailure != EDMFireAttemptFailure::None
			? LastFireAttemptFailure : EDMFireAttemptFailure::Cancelled;
		FailureControl->RecordFailedFireAttempt(Failure);
		if (!bShotOutcomeLogged)
		{
			UE_LOG(LogDM, Log, TEXT("ShotExecuted attempt=%llu cycle=%d result=false reason=%s"),
				FireAttemptId, FailureControl->FireCycle.CycleId, DMFireAttempt::Name(Failure));
			bShotOutcomeLogged = true;
		}
	}
    ClearCycleMontageTask();
    ActiveCycle = nullptr;
    ActiveFireAction = nullptr;
    ActiveCombatBrain = nullptr;
    CycleController.Reset(); CycleTarget.Reset(); CycleWeapon.Reset();
    bCycleQueued = false;
    DM_FIRE_LOG(GetAvatarActorFromActorInfo(), FireAttemptId, TEXT("AbilityEnded"),
        FString::Printf(TEXT("discovered=%d callback=%d timer=%d attempted=%d fired=%d gate=%s cancelled=%d"),
            bFireNotifyDiscovered, bFireNotifyReceived, bFireTimerReceived, bShotAttempted, bFiredThisActivation, DMFireAttempt::Name(LastFireAttemptFailure), bWasCancelled));
    ClearFireNotifyTimers();
    UnbindMontageNotify();
	bMontagePathActive = false;
	DesiredTarget.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FVector UDMGameplayAbility_Fire::GetFireStartLocation(AActor* Avatar) const
{
	if (bUseWeaponMuzzle && Avatar)
	{
		if (const UDMWeaponLoadoutComponent* Loadout = Avatar->FindComponentByClass<UDMWeaponLoadoutComponent>())
		{
			if (Loadout->GetActiveWeaponActor())
			{
				return Loadout->GetActiveMuzzleTransform().GetLocation();
			}
		}
	}

	if (const ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		return Character->GetPawnViewLocation();
	}

	return Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector;
}

FVector UDMGameplayAbility_Fire::GetCameraAimPoint(AActor* Avatar) const
{
	if (const AController* Controller = GetControllerFromActorInfo())
	{
		FVector ViewLoc;
		FRotator ViewRot;
		Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
		const FVector CamEnd = ViewLoc + ViewRot.Vector() * ResolveMaxFireRange(Avatar);

		if (UWorld* World = Avatar ? Avatar->GetWorld() : nullptr)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(DMFireAim), bTraceComplex);
			Params.AddIgnoredActor(Avatar);

			TArray<AActor*> Attached;
			Avatar->GetAttachedActors(Attached);
			Params.AddIgnoredActors(Attached);

			FHitResult AimHit;
			if (World->LineTraceSingleByChannel(AimHit, ViewLoc, CamEnd, TraceChannel, Params))
			{
				return AimHit.ImpactPoint;
			}
		}

		return CamEnd;
	}

	if (Avatar)
	{
		return Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * ResolveMaxFireRange(Avatar);
	}

	return FVector::ZeroVector;
}

FVector UDMGameplayAbility_Fire::GetFireAimLocation(AActor* Avatar, AActor* Target) const
{
	if (IsValid(Target))
	{
		return UDMCombatFireControlComponent::ResolveIntendedTargetLocation(
			Avatar, Target, GetFireStartLocation(Avatar));
	}

	if (const AController* Controller = GetControllerFromActorInfo())
	{
		if (const ADeathMatchRobotAIController* RobotAI = Cast<ADeathMatchRobotAIController>(Controller))
		{
			const FVector Focus = RobotAI->GetFocusLocation();
			if (!Focus.IsNearlyZero())
			{
				return Focus;
			}
		}
	}

	return GetCameraAimPoint(Avatar);
}

ADMWeaponActor* UDMGameplayAbility_Fire::ResolveActiveWeapon(AActor* Avatar) const
{
	if (!Avatar)
	{
		return nullptr;
	}

	if (const UDMWeaponLoadoutComponent* Loadout = Avatar->FindComponentByClass<UDMWeaponLoadoutComponent>())
	{
		return Loadout->GetActiveWeaponActor();
	}

	return nullptr;
}

float UDMGameplayAbility_Fire::ResolveMaxFireRange(AActor* Avatar) const
{
	if (const ADMWeaponActor* Weapon = ResolveActiveWeapon(Avatar))
	{
		return Weapon->GetMaxFireRange();
	}
	return FMath::Max(1.0f, FallbackFireRange);
}

EDMFireAttemptFailure UDMGameplayAbility_Fire::EvaluateLegacyRangeFailure(
	AActor* Avatar, AActor* Target) const
{
	if (!IsValid(Avatar) || !IsValid(Target))
	{
		return EDMFireAttemptFailure::None;
	}
	const FVector FireOrigin = GetFireStartLocation(Avatar);
	const FDMFireRangeEvaluation Range = UDMCombatFireControlComponent::ResolveFireRange(
		Avatar, Target, FireOrigin, ResolveMaxFireRange(Avatar));
	return Range.bAllowed ? EDMFireAttemptFailure::None : EDMFireAttemptFailure::Range;
}

bool UDMGameplayAbility_Fire::IsShooterAds(AActor* Avatar) const
{
	const UDMAimZoomComponent* AimZoom = UDMAimZoomComponent::FindOnActor(Avatar);
	return AimZoom && AimZoom->IsAiming();
}

float UDMGameplayAbility_Fire::ResolveShotSpreadDegrees(AActor* Avatar) const
{
	if (!bApplyWeaponSpread)
	{
		return 0.0f;
	}

	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	const float Now = World ? World->GetTimeSeconds() : 0.0f;
	const bool bAds = IsShooterAds(Avatar);
	const APawn* Pawn = Cast<APawn>(Avatar);
	const float MovingSpread = Pawn && Cast<AAIController>(Pawn->GetController())
		? FMath::Max(0.f, MovingAISpreadDegrees) * FMath::Clamp(Pawn->GetVelocity().Size2D() / 600.f, 0.f, 2.f) : 0.f;

	if (ADMWeaponActor* Weapon = ResolveActiveWeapon(Avatar))
	{
		return Weapon->GetSpreadDegreesForShot(Now, bAds) + MovingSpread;
	}

	return (bAds ? FallbackSpreadDegrees * 0.45f : FallbackSpreadDegrees) + MovingSpread;
}

FVector UDMGameplayAbility_Fire::ApplySpreadToDirection(const FVector& BaseDir, float SpreadDegrees) const
{
	FVector Dir = BaseDir.GetSafeNormal();
	if (Dir.IsNearlyZero() || SpreadDegrees <= KINDA_SMALL_NUMBER)
	{
		return Dir;
	}

	const float ConeRad = FMath::DegreesToRadians(SpreadDegrees);
	return FMath::VRandCone(Dir, ConeRad).GetSafeNormal();
}

void UDMGameplayAbility_Fire::NotifyWeaponShotFired(AActor* Avatar) const
{
	ADMWeaponActor* Weapon = ResolveActiveWeapon(Avatar);
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!Weapon || !World)
	{
		return;
	}

	Weapon->NotifyShotFired(World->GetTimeSeconds());
}

void UDMGameplayAbility_Fire::FaceTowardFireAim(AActor* Avatar, const FVector& AimPoint) const
{
	if (!bFaceAimDirectionOnFire || !IsValid(Avatar) || AimPoint.IsNearlyZero())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(Avatar);
	if (!Pawn)
	{
		return;
	}

	AController* Controller = Pawn->GetController();
	const bool bIsAI = Controller && Controller->IsA<AAIController>();
	if (bFaceAimDirectionOnlyForAI && !bIsAI)
	{
		return;
	}

	FVector ToAim = AimPoint - Pawn->GetActorLocation();
	if (ToAim.SizeSquared() < 1.0f)
	{
		return;
	}

	// Pawn body: yaw only (keep grounded stance).
	FVector Flat = ToAim;
	Flat.Z = 0.0f;
	if (!Flat.Normalize())
	{
		return;
	}

	FRotator BodyRot = Pawn->GetActorRotation();
	BodyRot.Yaw = Flat.Rotation().Yaw;
	Pawn->SetActorRotation(BodyRot);

	if (Controller)
	{
		const FRotator LookRot = ToAim.GetSafeNormal().Rotation();
		Controller->SetControlRotation(LookRot);

		if (AAIController* AI = Cast<AAIController>(Controller))
		{
			AI->SetFocalPoint(AimPoint);
		}
	}

	// Robot pawns default to OrientToMovement — switch to controller look while shooting.
	if (ACharacter* Char = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
		{
			Move->bOrientRotationToMovement = false;
			Move->bUseControllerDesiredRotation = true;
		}
	}
}

bool UDMGameplayAbility_Fire::PerformFireTrace(
	AActor* Avatar,
	AActor* Target,
	FHitResult& OutHit,
	FVector& OutStart,
	FVector& OutEnd) const
{
	OutStart = FVector::ZeroVector;
	OutEnd = FVector::ZeroVector;

	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	const FVector AimPoint = GetFireAimLocation(Avatar, Target);
	FaceTowardFireAim(Avatar, AimPoint);

	// Sample muzzle AFTER facing so barrel follows body yaw.
	OutStart = GetFireStartLocation(Avatar);

	FVector Dir = AimPoint - OutStart;
	if (!Dir.Normalize())
	{
		if (const AController* Controller = GetControllerFromActorInfo())
		{
			FVector ViewLoc;
			FRotator ViewRot;
			Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
			Dir = ViewRot.Vector();
		}
		else
		{
			Dir = Avatar->GetActorForwardVector();
		}
	}

	const float SpreadDeg = ResolveShotSpreadDegrees(Avatar);
	Dir = ApplySpreadToDirection(Dir, SpreadDeg);
	OutEnd = OutStart + Dir * ResolveMaxFireRange(Avatar);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DMFireTrace), bTraceComplex);
	Params.AddIgnoredActor(Avatar);

	TArray<AActor*> AttachedActors;
	Avatar->GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);

#if !UE_BUILD_SHIPPING
    for (const AActor* Actor : {Avatar, Target})
        if (IsValid(Actor))
            if (const auto* Parts = Actor->FindComponentByClass<UPartsComponent>())
                Parts->ValidateWeaponTraceChannel(this, TraceChannel);
#endif

	const APawn* FiringPawn = Cast<APawn>(Avatar);
	const ADeathMatchRobotAIController* RobotAI = FiringPawn
		? Cast<ADeathMatchRobotAIController>(FiringPawn->GetController()) : nullptr;
	const ECollisionChannel EffectiveTraceChannel = RobotAI
		? UDMCombatFireControlComponent::ResolveWeaponTraceChannel(RobotAI) : TraceChannel.GetValue();
	return World->LineTraceSingleByChannel(OutHit, OutStart, OutEnd, EffectiveTraceChannel, Params);
}

void UDMGameplayAbility_Fire::DrawFireTrail(
	UWorld* World,
	const FVector& Start,
	const FVector& End,
	bool bHit,
	const FVector& ImpactPoint) const
{
	if (!bShowFireTrail || !World)
	{
		return;
	}

	const FVector LineEnd = bHit ? ImpactPoint : End;
	const FColor Color = bHit ? FireTrailHitColor : FireTrailMissColor;

	DrawDebugLine(World, Start, LineEnd, Color, false, FireTrailDuration, 0, FireTrailThickness);

	if (bHit)
	{
		DrawDebugPoint(World, ImpactPoint, 12.0f, Color, false, FireTrailDuration);

		if (FireTrailImpactEffect)
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				World,
				FireTrailImpactEffect,
				ImpactPoint,
				(ImpactPoint - Start).Rotation(),
				true);
		}
	}
}

float UDMGameplayAbility_Fire::ApplyFireDamage(const FHitResult& Hit, AActor* Avatar, float RequestedDamage, FGameplayTag& OutHitPart) const
{
    OutHitPart = FGameplayTag();
    AActor* HitActor = Hit.GetActor();
    const auto RecordFireDamageResult = [&](float PartDamage, float BodyDamage, bool bFriendlyBlocked, const TCHAR* Reason)
    {
        DM_FIRE_LOG(Avatar, FireAttemptId, TEXT("DamageRouting"), FString::Printf(
            TEXT("target=%s requested=%.3f partApplied=%.3f bodyApplied=%.3f totalApplied=%.3f friendlyBlocked=%d zeroReason=%s"),
            *GetNameSafe(HitActor), RequestedDamage, PartDamage, BodyDamage, PartDamage + BodyDamage, bFriendlyBlocked, Reason));
        return PartDamage + BodyDamage;
    };
#if !UE_BUILD_SHIPPING
    if (IsValid(HitActor))
    {
        const auto* Parts = HitActor->FindComponentByClass<UPartsComponent>();
        if (Parts) Parts->ValidateWeaponTraceChannel(this, TraceChannel);
        if (DMIsCombatDiagEnabled())
        {
            const auto* Victim = Cast<APartsCharacterBase>(HitActor);
            const auto* Manager = Victim ? Victim->GetPartsDamageManager() : nullptr;
            const FGameplayTag Tag = Manager ? Manager->ResolveHitPartTag(Hit) : FGameplayTag();
            const bool bOwnerMesh = Victim && Hit.GetComponent() == Victim->GetMesh();
            const bool bAttached = Parts && Parts->ResolvePartTagFromComponent(Hit.GetComponent()).IsValid();
            DM_FIRE_LOG(Avatar, FireAttemptId, TEXT("PartResolution"), FString::Printf(
                TEXT("part=%s exists=%d destroyed=%d ownerMesh=%d attachedPart=%d detachedDebris=%d bodyOnly=%d fallbackTrace=0"),
                *Tag.ToString(), Parts && Parts->HasPart(Tag), Parts && Parts->IsPartDestroyed(Tag), bOwnerMesh,
                bAttached, HitActor->ActorHasTag(PartsHitCollision::DetachedDebrisTag), bOwnerMesh && (!Tag.IsValid() || !Parts || !Parts->HasPart(Tag))));
        }
    }
#endif
    if (!IsValid(HitActor)) return RecordFireDamageResult(0, 0, false, TEXT("InvalidTarget"));
    if (!FMath::IsFinite(RequestedDamage) || RequestedDamage <= 0.f) return RecordFireDamageResult(0, 0, false, TEXT("InvalidOrZeroRequestedDamage"));
    if (HitActor->ActorHasTag(PartsHitCollision::DetachedDebrisTag)) return RecordFireDamageResult(0, 0, false, TEXT("DetachedDebris"));
    if (IsActorDead(HitActor)) return RecordFireDamageResult(0, 0, false, TEXT("DeadTarget"));

	AController* InstigatorController = nullptr;
	if (APawn* Pawn = Cast<APawn>(Avatar))
	{
		InstigatorController = Pawn->GetController();
	}

	if (!bAllowFriendlyFire)
	{
		AController* VictimController = nullptr;
		if (const APawn* VictimPawn = Cast<APawn>(HitActor))
		{
			VictimController = VictimPawn->GetController();
		}

		const FGenericTeamId ShooterTeam = DeathMatchTeamUtils::GetTeamIdFromController(InstigatorController);
		const FGenericTeamId VictimTeam = DeathMatchTeamUtils::GetTeamIdFromController(VictimController);
		if (ShooterTeam != FGenericTeamId::NoTeam
			&& VictimTeam != FGenericTeamId::NoTeam
			&& FGenericTeamId::GetAttitude(ShooterTeam, VictimTeam) == ETeamAttitude::Friendly)
		{
			return RecordFireDamageResult(0, 0, true, TEXT("FriendlyFireBlocked"));
		}
	}

	if (APartsCharacterBase* PartsVictim = Cast<APartsCharacterBase>(HitActor))
	{
		if (!PartsVictim->GetPartsDamageManager()) return RecordFireDamageResult(0, 0, false, TEXT("MissingDamageManager"));
		const FPartsHitDamageResult DamageResult = PartsVictim->ApplyHitDamage(Hit, RequestedDamage, InstigatorController, Avatar);
		OutHitPart = DamageResult.HitPartTag;
		return RecordFireDamageResult(DamageResult.PartDamageApplied, DamageResult.BodyDamageApplied, false, DamageResult.ZeroDamageReason);
	}

	const float Applied = UGameplayStatics::ApplyDamage(
		HitActor,
		RequestedDamage,
		InstigatorController,
		Avatar,
		UDamageType::StaticClass());
    return RecordFireDamageResult(0, Applied, false, Applied > 0.f ? TEXT("None") : TEXT("DamageRejected"));
}

bool UDMGameplayAbility_Fire::IsActorDead(const AActor* Actor) const
{
	if (const APartsCharacterBase* PartsCharacter = Cast<APartsCharacterBase>(Actor))
    {
        // Parts/DeathMatch damage and death use body CurHP. GAS attributes may be absent.
        return PartsCharacter->IsDead || PartsCharacter->CurHP <= 0.0f;
    }
    if (const ACharacterBase* Character = Cast<ACharacterBase>(Actor))
	{
		return Character->IsDead || Character->GetHealth() <= 0.0f;
	}
	return !IsValid(Actor);
}

FDMShotResult UDMGameplayAbility_Fire::BuildShotResult(AActor* Avatar, const FHitResult& Hit, bool bHit) const
{
	FDMShotResult Result;
	Result.bShotExecuted = true;
	Result.bHitAnything = bHit;
	Result.BaseDamage = FMath::Max(0.0f, FireDamage);
	if (!bHit)
	{
		return Result;
	}

	Result.HitActor = Hit.GetActor();
	if (const ADMWeaponActor* Weapon = ResolveActiveWeapon(Avatar))
	{
		Result.DistanceDamageMultiplier = Weapon->GetDamageMultiplierAtDistance(
			FVector::Distance(Hit.TraceStart, Hit.ImpactPoint));
	}
	Result.RequestedDamage = Result.BaseDamage * Result.DistanceDamageMultiplier;
	AController* ShooterController = Cast<APawn>(Avatar) ? Cast<APawn>(Avatar)->GetController() : nullptr;
	const FGenericTeamId ShooterTeam = DeathMatchTeamUtils::GetTeamIdFromController(ShooterController);
	const ETeamAttitude::Type Attitude = Result.HitActor
		? DeathMatchTeamUtils::GetAttitudeTowards(ShooterTeam, *Result.HitActor)
		: ETeamAttitude::Neutral;
	Result.bHitFriendly = Attitude == ETeamAttitude::Friendly;
	Result.bHitHostile = Attitude == ETeamAttitude::Hostile;
	Result.bBlockedByWorld = Result.HitActor == nullptr
		|| (Cast<APawn>(Result.HitActor) == nullptr && Cast<ACharacterBase>(Result.HitActor) == nullptr);

	const bool bWasDead = IsActorDead(Result.HitActor);
	Result.DamageApplied = ApplyFireDamage(Hit, Avatar, Result.RequestedDamage, Result.HitPart);
	Result.bKilledTarget = !bWasDead && Result.DamageApplied > 0.0f && IsActorDead(Result.HitActor);
	return Result;
}

bool UDMGameplayAbility_Fire::ExecuteFire_Implementation(AActor* Target)
{
	LastShotResult = FDMShotResult();
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		return false;
	}

	FHitResult Hit;
	FVector TraceStart;
	FVector TraceEnd;
	const bool bHit = PerformFireTrace(Avatar, Target, Hit, TraceStart, TraceEnd);
    DM_FIRE_LOG(Avatar, FireAttemptId, TEXT("LineTrace"), FString::Printf(TEXT("hit=%d actor=%s component=%s class=%s bone=%s impact=%s start=%s end=%s channel=%d complex=%d"), bHit, *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *GetNameSafe(Hit.GetComponent() ? Hit.GetComponent()->GetClass() : nullptr), *Hit.BoneName.ToString(), *Hit.ImpactPoint.ToString(), *TraceStart.ToString(), *TraceEnd.ToString(), int32(TraceChannel), bTraceComplex));

	DrawFireTrail(
		Avatar->GetWorld(),
		TraceStart,
		TraceEnd,
		bHit,
		bHit ? Hit.ImpactPoint : TraceEnd);

	LastShotResult = BuildShotResult(Avatar, Hit, bHit);

	DM_FIRE_LOG(Avatar, FireAttemptId, TEXT("ShotResult"), FString::Printf(TEXT("executed=%d hit=%d base=%.3f distanceMultiplier=%.3f requested=%.3f totalApplied=%.3f route=%s"), LastShotResult.bShotExecuted, LastShotResult.bHitAnything, LastShotResult.BaseDamage, LastShotResult.DistanceDamageMultiplier, LastShotResult.RequestedDamage, LastShotResult.DamageApplied, bHit ? TEXT("SeeDamageRouting") : TEXT("TraceMiss")));
    NotifyWeaponShotFired(Avatar);
    DM_FIRE_LOG(Avatar, FireAttemptId, TEXT("WeaponShotFired"));
#if !UE_BUILD_SHIPPING
	if (DMCombatDebug::IsEnabled(Avatar->GetWorld()))
	{
		const APawn* Pawn = Cast<APawn>(Avatar);
		AController* Controller = Pawn ? Pawn->GetController() : nullptr;
		if (auto* Tactics = Controller ? Controller->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr)
		{
			// Copy the exact OnShotResolved payload before listeners can mutate game state.
			const FVector DebugEnd = bHit ? Hit.ImpactPoint : TraceEnd;
			Tactics->RecordDebugShot(LastShotResult, FVector::Distance(TraceStart, DebugEnd), &TraceStart, &DebugEnd);
		}
	}
#endif
	OnShotResolved.Broadcast(LastShotResult);

	return LastShotResult.bShotExecuted;
}

bool UDMGameplayAbility_Fire::IsCombatPatternAllowed(const FGameplayAbilityActorInfo* ActorInfo) const
{
    APawn* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
    AController* Controller = Pawn ? Pawn->GetController() : nullptr;
    const auto* Tactics = Controller ? Controller->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr;
    return !UsesAuthoritativeFireControl(Tactics, false) || Tactics->AllowsFire();
}

bool UDMGameplayAbility_Fire::UsesAuthoritativeFireControl(
	const UDMCombatFireControlComponent* FireControl, bool bHasActiveCycle)
{
	return bHasActiveCycle
		|| (FireControl && FireControl->bEnabled && IsValid(FireControl->FireModule));
}

bool UDMGameplayAbility_Fire::BlockShot(EDMFireAttemptFailure Failure)
{
    if (ActiveCycle) ActiveCycle->LastFailureReason = Failure;
    if (ActiveCombatBrain) ActiveCombatBrain->PublishFireCycle();
    LastFireAttemptFailure = Failure;
    if (const auto* Character = Cast<ACharacterBase>(GetAvatarActorFromActorInfo()))
        DM_FIRE_LOG(Character, FireAttemptId, TEXT("GateSnapshot"), FString::Printf(TEXT("bodyHP=%.1f gasHP=%.1f dead=%d speed=%.1f ads=%d"),
            Character->CurHP, Character->GetHealth(), Character->IsDead, Character->GetVelocity().Size2D(), IsShooterAds(const_cast<ACharacterBase*>(Character))));
    DM_FIRE_LOG(GetAvatarActorFromActorInfo(), FireAttemptId, TEXT("ShotBlocked"), FString::Printf(TEXT("gate=%s"), DMFireAttempt::Name(Failure)));
	if (!bShotOutcomeLogged)
	{
		UE_LOG(LogDM, Log, TEXT("ShotExecuted attempt=%llu cycle=%d result=false reason=%s"),
			FireAttemptId, ActiveCycle ? ActiveCycle->CycleId : INDEX_NONE, DMFireAttempt::Name(Failure));
		bShotOutcomeLogged = true;
	}
    return false;
}

bool UDMGameplayAbility_Fire::ExecutePatternFire(AActor* Target)
{
    if (bExecutingFire) return false;
    TGuardValue<bool> ExecutingGuard(bExecutingFire, true);
	bShotOutcomeLogged = false;
    bShotAttempted = true;
    LastShotResult = FDMShotResult();
    LastFireAttemptFailure = EDMFireAttemptFailure::None;
    DM_FIRE_LOG(GetAvatarActorFromActorInfo(), FireAttemptId, TEXT("ShotAttempted"));
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!IsValid(Avatar) || !Avatar->GetWorld() || IsActorDead(Avatar)) return BlockShot(EDMFireAttemptFailure::InvalidAvatar);
    APawn* Pawn = Cast<APawn>(Avatar);
    AController* Controller = Pawn ? Pawn->GetController() : nullptr;
    auto* Tactics = Controller ? Controller->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr;
	const bool bUsesFireControl = UsesAuthoritativeFireControl(Tactics, ActiveCycle != nullptr);
    if (bUsesFireControl && Tactics) Target=Tactics->GetCombatTarget();
    if (bUsesFireControl)
    {
        const auto Failure = ActiveCycle ? CycleSafetyFailure() : Tactics->GetFireFailure();
        if (Failure != EDMFireAttemptFailure::None) return BlockShot(Failure);
    }
    if (!ActiveCycle && !IsPlayerAimingAllowed(CurrentActorInfo)) return BlockShot(EDMFireAttemptFailure::AimZoom);
    if (bRequireTarget && (!IsValid(Target) || IsActorDead(Target))) return BlockShot(EDMFireAttemptFailure::InvalidTarget);
    if (const auto* Parts = Avatar->FindComponentByClass<UPartsComponent>())
        if (bRequireCanShoot && !Parts->CanShoot()) return BlockShot(EDMFireAttemptFailure::Parts);
	if (!bUsesFireControl && EvaluateLegacyRangeFailure(Avatar, Target) == EDMFireAttemptFailure::Range)
        return BlockShot(EDMFireAttemptFailure::Range);
    if (ActiveCycle && Avatar->GetWorld()->GetTimeSeconds() + .001f < ActiveCycle->NextShotTime)
        return BlockShot(EDMFireAttemptFailure::Cadence);
    DM_FIRE_LOG(Avatar, FireAttemptId, TEXT("ExecuteFire"));
    const bool bCycleShot = ActiveCycle != nullptr;
    const TWeakObjectPtr<UDMCombatFireControlComponent> ShotBrain = ActiveCombatBrain.Get();
    const TWeakObjectPtr<UDMCombatFireModule> ShotAction = ActiveFireAction.Get();
    const int32 ShotCycleId = ActiveCycle ? ActiveCycle->CycleId : INDEX_NONE;
    if (!ExecuteFire(Target)) return BlockShot(EDMFireAttemptFailure::ExecuteFire);
    // BlueprintNativeEvent returns success; do not count montage/notifies as shots.
    LastShotResult.bShotExecuted = true;
    bFiredThisActivation = true;
    if (auto* Brain = ShotBrain.Get())
    {
        // Count the successful call even if its result delegate ended the ability.
        // Reentry is blocked until this call returns; never count against a new action.
        if (Brain->FireModule == ShotAction.Get() && (Brain->FireCycle.CycleId == ShotCycleId
            || (bEndingFireAbility && Brain->FireCycle.CycleId == ShotCycleId + 1)))
        {
            const auto EndState = Brain->FireCycle.CycleState;
            Brain->NotifyShotFired();
            if (bEndingFireAbility) { Brain->FireCycle.CycleState = EndState; Brain->PublishFireCycle(); }
        }
    }
	bool bFireControlShot = bCycleShot;
	if (!bCycleShot && Tactics && Tactics->bEnabled && Tactics->IsEngaged())
	{
		Tactics->NotifyShotFired();
		bFireControlShot = true;
	}
	UE_LOG(LogDM, Log, TEXT("ShotExecuted attempt=%llu cycle=%d result=true reason=None"),
		FireAttemptId, ShotCycleId);
	bShotOutcomeLogged = true;
    DM_FIRE_LOG(Avatar, FireAttemptId, TEXT("ShotExecuted"));
    if (!bShotCooldownCommitted)
    {
        bShotCooldownCommitted = true;
        if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
            CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
    }
	if (!bFireControlShot)
	{
		SetLegacyNextFireAllowedAfter(FireCooldown);
		UE_LOG(LogDM, Log, TEXT("FireCooldown Committed attempt=%llu cycle=%d next=%.3f"),
			FireAttemptId, INDEX_NONE, LegacyNextFireAllowedTimeSeconds);
	}
    DM_FIRE_LOG(Avatar, FireAttemptId, TEXT("CooldownStarted"), FString::Printf(TEXT("until=%.3f"),
		bFireControlShot && Tactics ? Tactics->GetNextShotAllowedTime() : LegacyNextFireAllowedTimeSeconds));
    DM_FIRE_LOG(Avatar, FireAttemptId, TEXT("ShotFired"));
    return true;
}


void UDMGameplayAbility_Fire::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    // CommitAbility runs before the montage, so legacy GAS cooldown belongs to a real shot.
    // AI Fire Control owns cadence exclusively; applying a GAS cooldown there would create
    // another activation-time clock that can disagree with FireCycle.NextShotTime.
    if (bFiredThisActivation && !GetCombatBrain())
        Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
}

UDMCombatFireControlComponent* UDMGameplayAbility_Fire::GetCombatBrain() const
{
    const auto* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
    auto* Controller = Pawn ? Cast<ADeathMatchRobotAIController>(Pawn->GetController()) : nullptr;
    auto* Tactics = Controller ? Controller->FindComponentByClass<UDMCombatFireControlComponent>() : nullptr;
    return Tactics && Tactics->bEnabled && Tactics->FireModule ? Tactics : nullptr;
}

EDMFireAttemptFailure UDMGameplayAbility_Fire::CycleSafetyFailure(bool /*bCheckMovement*/) const
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!IsValid(Avatar) || !Avatar->GetWorld() || IsActorDead(Avatar)) return EDMFireAttemptFailure::AvatarDead;
    if (!CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid()) return EDMFireAttemptFailure::Context;
    if (!CycleTarget.IsValid()) return EDMFireAttemptFailure::InvalidTarget;
    if (IsActorDead(CycleTarget.Get())) return EDMFireAttemptFailure::TargetDead;
    const auto* Pawn = Cast<APawn>(Avatar);
    if (!CycleController.IsValid() || !Pawn || Pawn->GetController() != CycleController.Get()) return EDMFireAttemptFailure::Context;
    const auto* AI = Cast<ADeathMatchRobotAIController>(CycleController.Get());
    if (!AI) return EDMFireAttemptFailure::Context;
    if (!CycleWeapon.IsValid()) return EDMFireAttemptFailure::MissingWeapon;
    if (ResolveActiveWeapon(Avatar) != CycleWeapon.Get()) return EDMFireAttemptFailure::WeaponChanged;
    auto* Tactics = GetCombatBrain();
    if (!Tactics || Tactics != ActiveCombatBrain || Tactics->FireModule != ActiveFireAction) return EDMFireAttemptFailure::Context;
    if (Tactics->GetCombatTarget()!=CycleTarget.Get()) return EDMFireAttemptFailure::TargetChanged;
    if (!ActiveFireAction || !ActiveFireAction->AllowsMoveSpeed(Pawn->GetVelocity().Size2D())) return EDMFireAttemptFailure::MoveSpeed;
    // Even the first montage notify must obey a newly chosen emergency/hidden policy.
    const auto Failure = Tactics->GetFireSafetyFailure(true);
    if (Failure != EDMFireAttemptFailure::None) return Failure;
    if (const auto* Parts = Avatar->FindComponentByClass<UPartsComponent>())
        if (!Parts->CanShoot()) return EDMFireAttemptFailure::CannotShoot;
	// Fire Control just re-sampled muzzle, aim point, weapon maximum range, and weapon trace channel.
	// Do not re-run ActorLocation range or an Ability trace in another coordinate/channel contract.
    return Failure;
}

void UDMGameplayAbility_Fire::CheckCycleSafety()
{
    if (!IsActive() || !ActiveCycle) return;
    const auto Failure = CycleSafetyFailure(bShotAttempted);
    if (Failure != EDMFireAttemptFailure::None)
    {
        ActiveCycle->LastFailureReason = LastFireAttemptFailure = Failure;
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
    }
}

void UDMGameplayAbility_Fire::ClearCycleMontageTask()
{
    if (CycleMontageTask)
    {
        CycleMontageTask->OnCompleted.RemoveAll(this);
        CycleMontageTask->OnBlendOut.RemoveAll(this);
        CycleMontageTask->OnInterrupted.RemoveAll(this);
        CycleMontageTask->OnCancelled.RemoveAll(this);
        CycleMontageTask->EndTask();
        CycleMontageTask = nullptr;
    }
    if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        auto* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
        if (ASC->GetAnimatingAbility() == this && ASC->GetCurrentMontage() == FireMontage) ASC->CurrentMontageStop(0.f);
    }
}

void UDMGameplayAbility_Fire::StartFireCycleMontage()
{
    if (!IsActive() || !ActiveCycle || !GetWorld()) return;
    ActiveCycle->CycleState = EDMFireCycleState::Starting;
    bMontagePathActive = false;
    ClearFireNotifyTimers(); UnbindMontageNotify(); ClearCycleMontageTask();
    GetWorld()->GetTimerManager().ClearTimer(CycleNotifyTimer);
    GetWorld()->GetTimerManager().ClearTimer(CycleWatchdogTimer);
    bCycleQueued = false; bShotAttempted = false; bFinishFromMontageHandled = false;
    LastShotResult = FDMShotResult();
    bFireNotifyReceived = bFireTimerReceived = false;
    ActiveCycle->BeginCycle();
    UAnimInstance* Anim = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
    if (!Anim) { ActiveCycle->LastFailureReason = EDMFireAttemptFailure::Montage; EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
    // Repeating patterns require both authored notifies. Single can finish at montage end.
    // Completion never synthesizes a FireCycle or a shot.
    TArray<float> CycleTimes, ShotTimes;
    CollectFireNotifyTriggerTimes(ShotTimes);
    const FName SavedName = FireNotifyName; FireNotifyName = FireCycleNotifyName;
    CollectFireNotifyTriggerTimes(CycleTimes); FireNotifyName = SavedName;
    const int32 Section = FireMontage->GetSectionIndex(MontageStartSection);
    float Start = 0.f, End = FireMontage->GetPlayLength();
    if (Section != INDEX_NONE) FireMontage->GetSectionStartAndEndTime(Section, Start, End);
    ShotTimes.RemoveAll([Start, End](float T) { return T < Start || T >= End; });
    CycleTimes.RemoveAll([Start, End](float T) { return T < Start || T >= End; });
    bLegacySingleWithoutCycle = ActiveCycle->Settings.Pattern == EDMAdaptiveFirePattern::Single && CycleTimes.IsEmpty();
    if (ShotTimes.IsEmpty() || (!bLegacySingleWithoutCycle && (CycleTimes.IsEmpty() || CycleTimes[0] <= ShotTimes[0])))
    {
        ActiveCycle->LastFailureReason = ShotTimes.IsEmpty() ? EDMFireAttemptFailure::NoNotifyReceived : EDMFireAttemptFailure::MissingFireCycleNotify;
        UE_LOG(LogDM, Warning, TEXT("Fire Cycle requires Fire then FireCycle (Single may omit FireCycle): %s section %s, failure=%s"),
            *GetNameSafe(FireMontage), *MontageStartSection.ToString(), DMFireAttempt::Name(ActiveCycle->LastFailureReason));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return;
    }
    CycleFireNotifyOffsetSeconds = (ShotTimes[0] - Start) / FMath::Max(.01f, MontagePlayRate);
    bMontagePathActive = true;
    BindMontageNotify(Anim);
    CycleMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, FireMontage, FMath::Max(.01f, MontagePlayRate), MontageStartSection, true);
    if (!CycleMontageTask)
    { ActiveCycle->LastFailureReason = EDMFireAttemptFailure::Montage; EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
    CycleMontageTask->OnCompleted.AddDynamic(this, &UDMGameplayAbility_Fire::OnFireMontageCompleted);
    CycleMontageTask->OnInterrupted.AddDynamic(this, &UDMGameplayAbility_Fire::OnFireMontageInterrupted);
    CycleMontageTask->OnCancelled.AddDynamic(this, &UDMGameplayAbility_Fire::OnFireMontageCancelled);
    CycleMontageTask->ReadyForActivation();
    if (!IsActive() || !ActiveCycle) return;
    const auto* Instance = Anim->GetActiveInstanceForMontage(FireMontage);
    CycleMontageInstanceId = Instance ? Instance->GetInstanceID() : INDEX_NONE;
    if (!Instance)
    { ActiveCycle->LastFailureReason = EDMFireAttemptFailure::Montage; EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
    ScheduleFireNotifyTimers();
    const int32 WatchCycle = ActiveCycle->CycleId;
    GetWorld()->GetTimerManager().SetTimer(CycleWatchdogTimer, FTimerDelegate::CreateWeakLambda(this, [this, WatchCycle]()
    {
        if (IsActive() && ActiveCycle && ActiveCycle->CycleId == WatchCycle && !bCycleQueued)
        {
            ActiveCycle->LastFailureReason = !ActiveCycle->FireNotifyReceived ? EDMFireAttemptFailure::NoNotifyReceived
                : !ActiveCycle->FireCycleNotifyReceived && !bLegacySingleWithoutCycle ? EDMFireAttemptFailure::MissingFireCycleNotify : EDMFireAttemptFailure::Montage;
            UE_LOG(LogDM, Warning, TEXT("Fire Cycle %d timeout: %s (%s)"), WatchCycle,
                DMFireAttempt::Name(ActiveCycle->LastFailureReason), *GetNameSafe(FireMontage));
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        }
    }), FMath::Max(.1f, (End - Start) / FMath::Max(.01f, MontagePlayRate) + .25f), false);
    if (ActiveCombatBrain) ActiveCombatBrain->PublishFireCycle();
    if (bScheduleFireFromMontageNotifyTimes && !CycleTimes.IsEmpty())
    {
        const int32 Id = ActiveCycle->CycleId;
        GetWorld()->GetTimerManager().SetTimer(CycleNotifyTimer, FTimerDelegate::CreateWeakLambda(this, [this, Id]()
        { if (IsActive() && ActiveCycle && ActiveCycle->CycleId == Id) HandleFireCycle(); }),
            FMath::Max(.001f, (CycleTimes[0] - Start) / FMath::Max(.01f, MontagePlayRate)), false);
    }
}

void UDMGameplayAbility_Fire::HandleFireCycle()
{
    if (bExecutingFire || bEndingFireAbility || !ActiveCycle || !IsActive() || bCycleQueued || ActiveCycle->FireCycleNotifyReceived) return;
    ActiveCycle->FireCycleNotifyReceived = true;
    if (!bShotAttempted)
    {
        ActiveCycle->LastFailureReason = EDMFireAttemptFailure::NoNotifyReceived;
        UE_LOG(LogDM, Warning, TEXT("FireCycle received before Fire in cycle %d (%s)"), ActiveCycle->CycleId, *GetNameSafe(FireMontage));
    }
    QueueFireCycle(!bShotAttempted || !LastShotResult.bShotExecuted || !ActiveCycle->ShouldContinue());
}

void UDMGameplayAbility_Fire::QueueFireCycle(bool bEnd)
{
    if (bEndingFireAbility || !ActiveCycle || !GetWorld() || bCycleQueued) return;
    bCycleQueued = true;
    ActiveCycle->CycleState = EDMFireCycleState::CycleQueued;
    ClearFireNotifyTimers();
    GetWorld()->GetTimerManager().ClearTimer(CycleNotifyTimer);
    GetWorld()->GetTimerManager().ClearTimer(CycleWatchdogTimer);
    if (ActiveCombatBrain) ActiveCombatBrain->PublishFireCycle();
    const int32 Id = ActiveCycle->CycleId;
    // Always yield out of the notify stack, including a zero remaining cadence.
    CycleTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Id, bEnd]()
    { ResumeFireCycle(Id, bEnd); }));
}

void UDMGameplayAbility_Fire::ResumeFireCycle(int32 ExpectedCycle, bool bEnd)
{
    if (!IsActive() || !ActiveCycle || ExpectedCycle != ActiveCycle->CycleId) return;
    // A queued boundary is safe for a confirmed normal switch, even during long automatic groups.
    if (ActiveCombatBrain) ActiveCombatBrain->EvaluatePendingTargetAtCycleBoundary();
    if (!IsActive() || !ActiveCycle || ExpectedCycle != ActiveCycle->CycleId) return;
    if (bEnd)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !LastShotResult.bShotExecuted); return;
    }
    const auto Failure = CycleSafetyFailure();
    if (Failure != EDMFireAttemptFailure::None)
    {
        ActiveCycle->LastFailureReason = Failure;
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return;
    }
    const float Delay = ActiveCycle->GetNextShotAllowedTime() - GetWorld()->GetTimeSeconds() - CycleFireNotifyOffsetSeconds;
    if (Delay > 0.f)
    {
        GetWorld()->GetTimerManager().SetTimer(CycleTimer, FTimerDelegate::CreateWeakLambda(this, [this, ExpectedCycle]()
        { ResumeFireCycle(ExpectedCycle, false); }), FMath::Max(.001f, Delay), false);
        return;
    }
    auto* Tactics = GetCombatBrain();
    if (!Tactics || !Tactics->AuthorizeFireCycle(false))
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return;
    }
    StartFireCycleMontage();
}

