#include "TitanDefence/Robot/State/TDRobotStateComponent.h"

#include "AbilitySystemComponent.h"
#include "Character/Abilities/Attributes/CharacterAttributeSetBase.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "TitanDefence/Robot/Core/TDRobotLog.h"
#include "TitanDefence/Robot/GAS/TDRobotAbilitySystemComponent.h"
#include "TitanDefence/Robot/GAS/TDRobotAttributeSet.h"
#include "TitanDefence/Robot/GAS/TDRobotGameplayTags.h"

namespace
{
	void LogInvalidRobotEffectAttributes(
		const UGameplayEffect* Effect,
		const AActor* Owner,
		const TCHAR* EffectPurpose)
	{
		if (!Effect)
		{
			return;
		}

		for (int32 ModifierIndex = 0; ModifierIndex < Effect->Modifiers.Num(); ++ModifierIndex)
		{
			const FGameplayAttribute& Attribute = Effect->Modifiers[ModifierIndex].Attribute;
			if (!Attribute.IsValid())
			{
				UE_LOG(LogTDRobotDamage, Error,
					TEXT("Robot GE has an invalid attribute reference: owner='%s' purpose='%s' GE='%s' modifier=%d. The asset may still reference removed TDRobotAttributeSet Health/MaxHealth/IncomingDamage fields; reassign it to UCharacterAttributeSetBase.Health, MaxHealth, or Damage."),
					*GetNameSafe(Owner), EffectPurpose, *GetNameSafe(Effect->GetClass()),
					ModifierIndex);
			}
		}
	}
}

UTDRobotStateComponent::UTDRobotStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UTDRobotStateComponent::InitializeStateSystem(
	UTDRobotAbilitySystemComponent* InAbilitySystemComponent,
	UTDRobotAttributeSet* InAttributeSet)
{
	if (!IsValid(InAbilitySystemComponent) || !IsValid(InAttributeSet))
	{
		UE_LOG(LogTDRobot, Error,
			TEXT("Robot State '%s' rejected invalid ASC or AttributeSet injection."),
			*GetNameSafe(this));
		return false;
	}

	if (bStateInitialized
		&& RobotAbilitySystemComponent.Get() == InAbilitySystemComponent
		&& RobotAttributeSet.Get() == InAttributeSet)
	{
		ApplyInitialAttributesIfNeeded();
		return true;
	}

	RemoveAttributeDelegates();
	RobotAbilitySystemComponent = InAbilitySystemComponent;
	RobotAttributeSet = InAttributeSet;
	InAttributeSet->SetInternalBodyDamageEffectClass(BodyDamageEffectClass);

	if (!InAbilitySystemComponent->GetSpawnedAttributes().Contains(InAttributeSet))
	{
		InAbilitySystemComponent->AddAttributeSetSubobject(InAttributeSet);
	}

	HealthChangedDelegateHandle = InAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSetBase::GetHealthAttribute())
		.AddUObject(this, &UTDRobotStateComponent::HandleHealthChanged);
	MaxHealthChangedDelegateHandle = InAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSetBase::GetMaxHealthAttribute())
		.AddUObject(this, &UTDRobotStateComponent::HandleMaxHealthChanged);
	ExternalGameplayEffectDamageDelegateHandle = InAttributeSet->OnExternalGameplayEffectDamage()
		.AddUObject(this, &UTDRobotStateComponent::HandleExternalGameplayEffectDamage);

	bStateInitialized = true;
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Robot State initialized: owner='%s' ASC='%s' attributes='%s' initialGE='%s' damageGE='%s' deathGE='%s'."),
		*GetNameSafe(GetOwner()), *GetNameSafe(InAbilitySystemComponent), *GetNameSafe(InAttributeSet),
		*GetNameSafe(InitialAttributesEffectClass), *GetNameSafe(BodyDamageEffectClass),
		*GetNameSafe(DeathStateEffectClass));
	OnStateInitialized.Broadcast(this, InAbilitySystemComponent);
	ApplyInitialAttributesIfNeeded();
	return true;
}

bool UTDRobotStateComponent::ApplyResolvedBodyDamage(
	float DamageAmount,
	const FTDDamageResult& DamageResult)
{
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Robot State body-damage request: owner='%s' damage=%.2f part='%s' bone='%s' health=%.2f/%.2f damageGE='%s'."),
		*GetNameSafe(GetOwner()), DamageAmount, *DamageResult.PartTag.ToString(),
		*DamageResult.HitBoneName.ToString(), GetHealth(), GetMaxHealth(),
		*GetNameSafe(BodyDamageEffectClass));

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return RejectBodyDamage(DamageResult, ETDRobotBodyDamageRejectionReason::NotAuthority);
	}
	if (!bDamageEnabled)
	{
		return RejectBodyDamage(DamageResult, ETDRobotBodyDamageRejectionReason::DamageDisabled);
	}
	if (!bStateInitialized || !RobotAbilitySystemComponent.IsValid() || !RobotAttributeSet.IsValid())
	{
		return RejectBodyDamage(DamageResult, ETDRobotBodyDamageRejectionReason::StateNotInitialized);
	}
	if (IsDead())
	{
		return RejectBodyDamage(DamageResult, ETDRobotBodyDamageRejectionReason::AlreadyDead);
	}
	if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		return RejectBodyDamage(DamageResult, ETDRobotBodyDamageRejectionReason::InvalidDamage);
	}
	if (!FMath::IsFinite(GetHealth()) || !FMath::IsFinite(GetMaxHealth())
		|| GetHealth() <= 0.0f || GetMaxHealth() <= 0.0f)
	{
		return RejectBodyDamage(DamageResult, ETDRobotBodyDamageRejectionReason::InvalidAttributes);
	}
	if (!BodyDamageEffectClass)
	{
		return RejectBodyDamage(DamageResult, ETDRobotBodyDamageRejectionReason::BodyDamageEffectNotSet);
	}

	UTDRobotAbilitySystemComponent* ASC = RobotAbilitySystemComponent.Get();
	RobotAttributeSet->SetInternalBodyDamageEffectClass(BodyDamageEffectClass);
	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(DamageResult.DamageCauser.Get());
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(BodyDamageEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return RejectBodyDamage(DamageResult, ETDRobotBodyDamageRejectionReason::SpecCreationFailed);
	}

	SpecHandle.Data->SetSetByCallerMagnitude(TDRobotGameplayTags::Data_Damage, DamageAmount);
	if (DamageResult.PartTag.IsValid())
	{
		SpecHandle.Data->AddDynamicAssetTag(DamageResult.PartTag);
	}

	const float PreviousHealth = GetHealth();
	const float StoredSetByCaller = SpecHandle.Data->GetSetByCallerMagnitude(
		TDRobotGameplayTags::Data_Damage, false, -1.0f);
	const UGameplayEffect* DamageEffectCDO = BodyDamageEffectClass->GetDefaultObject<UGameplayEffect>();
	LogInvalidRobotEffectAttributes(DamageEffectCDO, Owner, TEXT("BodyDamage"));
	UE_LOG(LogTDRobotDamage, Warning,
		TEXT("Body Damage GE spec ready: owner='%s' GE='%s' modifiers=%d requestedDamage=%.2f storedSetByCaller=%.2f partTagAdded=%s ASCActorInfoValid=%s."),
		*GetNameSafe(Owner), *GetNameSafe(BodyDamageEffectClass),
		DamageEffectCDO ? DamageEffectCDO->Modifiers.Num() : -1,
		DamageAmount, StoredSetByCaller, DamageResult.PartTag.IsValid() ? TEXT("true") : TEXT("false"),
		ASC->AbilityActorInfo.IsValid() ? TEXT("true") : TEXT("false"));
	PendingDamageResult = DamageResult;
	bHasPendingDamageResult = true;
	const FActiveGameplayEffectHandle AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	bHasPendingDamageResult = false;

	if (!AppliedHandle.WasSuccessfullyApplied())
	{
		return RejectBodyDamage(DamageResult, ETDRobotBodyDamageRejectionReason::EffectApplicationFailed);
	}

	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Body Damage GE applied: owner='%s' GE='%s' requestedDamage=%.2f health=%.2f->%.2f part='%s'."),
		*GetNameSafe(Owner), *GetNameSafe(BodyDamageEffectClass), DamageAmount,
		PreviousHealth, GetHealth(), *DamageResult.PartTag.ToString());
	if (FMath::IsNearlyEqual(PreviousHealth, GetHealth()))
	{
		UE_LOG(LogTDRobotDamage, Error,
			TEXT("Body Damage GE applied but shared Health did not change: owner='%s' GE='%s' SetByCaller=%.2f. The asset may still reference removed TDRobotAttributeSet Health/MaxHealth/IncomingDamage fields; reconfigure it to the UCharacterAttributeSetBase Damage/Health contract."),
			*GetNameSafe(Owner), *GetNameSafe(BodyDamageEffectClass), StoredSetByCaller);
	}

	OnBodyDamaged.Broadcast(DamageAmount, PreviousHealth, GetHealth(), DamageResult);
	return true;
}

float UTDRobotStateComponent::GetHealth() const
{
	return RobotAttributeSet.IsValid() ? RobotAttributeSet->GetHealth() : 0.0f;
}

float UTDRobotStateComponent::GetMaxHealth() const
{
	return RobotAttributeSet.IsValid() ? RobotAttributeSet->GetMaxHealth() : 0.0f;
}

float UTDRobotStateComponent::GetHealthRatio() const
{
	const float MaxHealth = GetMaxHealth();
	return MaxHealth > 0.0f ? FMath::Clamp(GetHealth() / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

bool UTDRobotStateComponent::IsDead() const
{
	return bDeathEventBroadcast
		|| (RobotAbilitySystemComponent.IsValid()
			&& RobotAbilitySystemComponent->HasMatchingGameplayTag(TDRobotGameplayTags::State_Dead))
		|| (bStateInitialized && GetHealth() <= 0.0f);
}

UAbilitySystemComponent* UTDRobotStateComponent::GetAbilitySystemComponent() const
{
	return RobotAbilitySystemComponent.Get();
}

void UTDRobotStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExternalGameplayEffectDamageTimerHandle);
	}
	QueuedExternalGameplayEffectDamage.Reset();
	RemoveAttributeDelegates();
	Super::EndPlay(EndPlayReason);
}

void UTDRobotStateComponent::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Robot body health changed: owner='%s' %.2f -> %.2f (max=%.2f)."),
		*GetNameSafe(GetOwner()), Data.OldValue, Data.NewValue, GetMaxHealth());
	OnHealthChanged.Broadcast(Data.OldValue, Data.NewValue, GetMaxHealth());
	if (Data.OldValue > 0.0f && Data.NewValue <= 0.0f && !bDeathEventBroadcast)
	{
		HandleDeath(bHasPendingDamageResult ? PendingDamageResult : FTDDamageResult());
	}
}

void UTDRobotStateComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!FMath::IsNearlyEqual(Data.OldValue, Data.NewValue))
	{
		OnHealthChanged.Broadcast(GetHealth(), GetHealth(), Data.NewValue);
	}
}

void UTDRobotStateComponent::HandleExternalGameplayEffectDamage(
	const FTDRobotExternalGameplayEffectDamage& DamageRequest)
{
	QueuedExternalGameplayEffectDamage.Add(DamageRequest);
	if (ExternalGameplayEffectDamageTimerHandle.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTDRobotDamage, Error,
			TEXT("External GE damage could not be queued without a World: owner='%s' effect='%s' damage=%.2f."),
			*GetNameSafe(GetOwner()), *GetNameSafe(DamageRequest.GameplayEffectClass),
			DamageRequest.Damage);
		return;
	}

	ExternalGameplayEffectDamageTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		this, &UTDRobotStateComponent::ProcessQueuedExternalGameplayEffectDamage);
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Queued external GE damage for next tick: owner='%s' effect='%s' damage=%.2f bone='%s' pending=%d."),
		*GetNameSafe(GetOwner()), *GetNameSafe(DamageRequest.GameplayEffectClass),
		DamageRequest.Damage, *DamageRequest.HitBoneName.ToString(),
		QueuedExternalGameplayEffectDamage.Num());
}

void UTDRobotStateComponent::ProcessQueuedExternalGameplayEffectDamage()
{
	ExternalGameplayEffectDamageTimerHandle.Invalidate();
	TArray<FTDRobotExternalGameplayEffectDamage> Requests =
		MoveTemp(QueuedExternalGameplayEffectDamage);
	QueuedExternalGameplayEffectDamage.Reset();

	for (const FTDRobotExternalGameplayEffectDamage& Request : Requests)
	{
		if (OnExternalGameplayEffectDamageReceived.IsBound())
		{
			UE_LOG(LogTDRobotDamage, Log,
				TEXT("Dispatching external GE damage: owner='%s' effect='%s' damage=%.2f bone='%s'."),
				*GetNameSafe(GetOwner()), *GetNameSafe(Request.GameplayEffectClass),
				Request.Damage, *Request.HitBoneName.ToString());
			OnExternalGameplayEffectDamageReceived.Broadcast(Request);
			continue;
		}

		UE_LOG(LogTDRobotDamage, Warning,
			TEXT("External GE damage has no bound receiver: owner='%s' effect='%s' damage=%.2f policy=%s."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Request.GameplayEffectClass), Request.Damage,
			*UEnum::GetValueAsString(ExternalDamageFallbackPolicy));
		if (ExternalDamageFallbackPolicy == ETDRobotExternalDamageFallbackPolicy::ApplyToBody)
		{
			FTDDamageResult DamageResult;
			DamageResult.OriginalDamage = Request.Damage;
			DamageResult.FinalBodyDamage = Request.Damage;
			DamageResult.DamageCauser = Request.SourceActor;
			DamageResult.InstigatorController = Request.InstigatorController;
			DamageResult.HitLocation = Request.HitLocation;
			DamageResult.HitBoneName = Request.HitBoneName;
			DamageResult.DamageEventType = Request.bHasValidHitResult
				? ETDDamageEventType::Point
				: ETDDamageEventType::Generic;
			ApplyResolvedBodyDamage(Request.Damage, DamageResult);
		}
	}
}

void UTDRobotStateComponent::RemoveAttributeDelegates()
{
	if (UTDRobotAbilitySystemComponent* ASC = RobotAbilitySystemComponent.Get())
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSetBase::GetHealthAttribute())
				.Remove(HealthChangedDelegateHandle);
		}
		if (MaxHealthChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSetBase::GetMaxHealthAttribute())
				.Remove(MaxHealthChangedDelegateHandle);
		}
	}
	if (UTDRobotAttributeSet* Attributes = RobotAttributeSet.Get())
	{
		if (ExternalGameplayEffectDamageDelegateHandle.IsValid())
		{
			Attributes->OnExternalGameplayEffectDamage().Remove(
				ExternalGameplayEffectDamageDelegateHandle);
		}
	}
	HealthChangedDelegateHandle.Reset();
	MaxHealthChangedDelegateHandle.Reset();
	ExternalGameplayEffectDamageDelegateHandle.Reset();
}

void UTDRobotStateComponent::ApplyInitialAttributesIfNeeded()
{
	AActor* Owner = GetOwner();
	UTDRobotAbilitySystemComponent* ASC = RobotAbilitySystemComponent.Get();
	if (!Owner || !Owner->HasAuthority() || !ASC || bInitialAttributesEffectApplied)
	{
		return;
	}

	bInitialAttributesEffectApplied = true;
	if (InitialAttributesEffectClass)
	{
		LogInvalidRobotEffectAttributes(
			InitialAttributesEffectClass->GetDefaultObject<UGameplayEffect>(),
			Owner,
			TEXT("InitialAttributes"));
		const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
			InitialAttributesEffectClass, 1.0f, Context);
		if (Spec.IsValid() && Spec.Data.IsValid())
		{
			const FActiveGameplayEffectHandle AppliedHandle =
				ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			UE_LOG(LogTDRobotDamage, Log,
				TEXT("Initial Attributes GE application: owner='%s' GE='%s' applied=%s health=%.2f/%.2f."),
				*GetNameSafe(Owner), *GetNameSafe(InitialAttributesEffectClass),
				AppliedHandle.WasSuccessfullyApplied() ? TEXT("true") : TEXT("false"),
				GetHealth(), GetMaxHealth());
		}
		else
		{
			UE_LOG(LogTDRobotDamage, Warning,
				TEXT("Initial Attributes GE spec creation failed: owner='%s' GE='%s'."),
				*GetNameSafe(Owner), *GetNameSafe(InitialAttributesEffectClass));
		}
	}
	else
	{
		UE_LOG(LogTDRobotDamage, Warning,
			TEXT("Robot '%s' has no Initial Attributes Effect Class."), *GetNameSafe(Owner));
	}

	if (GetMaxHealth() <= 0.0f || GetHealth() <= 0.0f)
	{
		UE_LOG(LogTDRobot, Warning,
			TEXT("Robot '%s' has invalid initial Health %.2f / MaxHealth %.2f. Configure Initial Attributes Effect Class; body damage remains rejected."),
			*GetNameSafe(Owner), GetHealth(), GetMaxHealth());
	}
}

void UTDRobotStateComponent::HandleDeath(const FTDDamageResult& DamageResult)
{
	if (bDeathEventBroadcast)
	{
		return;
	}

	bDeathEventBroadcast = true;
	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Robot body death: owner='%s' part='%s' causer='%s'."),
		*GetNameSafe(GetOwner()), *DamageResult.PartTag.ToString(),
		*GetNameSafe(DamageResult.DamageCauser.Get()));
	if (UTDRobotAbilitySystemComponent* ASC = RobotAbilitySystemComponent.Get())
	{
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			ASC->AddLooseGameplayTag(TDRobotGameplayTags::State_Dead);
			ASC->AddReplicatedLooseGameplayTag(TDRobotGameplayTags::State_Dead);
			if (DeathStateEffectClass)
			{
				const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
				const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
					DeathStateEffectClass, 1.0f, Context);
				if (Spec.IsValid() && Spec.Data.IsValid())
				{
					ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				}
			}
		}
	}
	OnBodyDeath.Broadcast(DamageResult);
}

bool UTDRobotStateComponent::RejectBodyDamage(
	const FTDDamageResult& DamageResult,
	ETDRobotBodyDamageRejectionReason Reason)
{
	UE_LOG(LogTDRobotDamage, Warning,
		TEXT("Robot State rejected body damage: owner='%s' reason=%s requestedOriginal=%.2f resolvedBody=%.2f part='%s' bone='%s' health=%.2f/%.2f."),
		*GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Reason),
		DamageResult.OriginalDamage, DamageResult.FinalBodyDamage,
		*DamageResult.PartTag.ToString(), *DamageResult.HitBoneName.ToString(),
		GetHealth(), GetMaxHealth());
	OnBodyDamageRejected.Broadcast(DamageResult, Reason);
	return false;
}

