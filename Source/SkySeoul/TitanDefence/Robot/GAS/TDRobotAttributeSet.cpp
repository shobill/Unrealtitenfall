#include "TitanDefence/Robot/GAS/TDRobotAttributeSet.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Pawn.h"
#include "TitanDefence/Robot/Core/TDRobotLog.h"

bool UTDRobotAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	const float Magnitude = Data.EvaluatedData.Magnitude;
	if (Data.EvaluatedData.Attribute != GetHealthAttribute()
		|| !FMath::IsFinite(Magnitude)
		|| Magnitude >= 0.0f)
	{
		return true;
	}

	UAbilitySystemComponent* SourceASC =
		Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = &Data.Target;
	const UGameplayEffect* Effect = Data.EffectSpec.Def;
	const bool bIsInternalBodyDamageEffect = Effect
		&& InternalBodyDamageEffectClass
		&& Effect->IsA(InternalBodyDamageEffectClass);
	if (!SourceASC || SourceASC == TargetASC || bIsInternalBodyDamageEffect)
	{
		return true;
	}

	FTDRobotExternalGameplayEffectDamage Request;
	Request.Damage = -Magnitude;
	Request.GameplayEffectClass = Effect ? Effect->GetClass() : nullptr;

	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetContext();
	if (const FHitResult* HitResult = Context.GetHitResult())
	{
		Request.HitResult = *HitResult;
		Request.HitBoneName = HitResult->BoneName;
		Request.HitLocation = HitResult->ImpactPoint;
		Request.bHasValidHitResult = true;
	}

	Request.SourceActor = SourceASC->GetAvatarActor();
	if (!Request.SourceActor)
	{
		Request.SourceActor = Context.GetOriginalInstigator();
	}
	if (!Request.SourceActor)
	{
		Request.SourceActor = Context.GetEffectCauser();
	}

	AActor* InstigatorActor = Context.GetOriginalInstigator();
	if (!InstigatorActor)
	{
		InstigatorActor = Request.SourceActor;
	}
	if (const APawn* InstigatorPawn = Cast<APawn>(InstigatorActor))
	{
		Request.InstigatorController = InstigatorPawn->GetController();
	}
	else if (InstigatorActor)
	{
		Request.InstigatorController = InstigatorActor->GetInstigatorController();
	}

	UE_LOG(LogTDRobotDamage, Log,
		TEXT("Intercepted external Gameplay Effect damage: owner='%s' effect='%s' damage=%.2f source='%s' bone='%s' hit=%s. Direct Health modifier blocked."),
		*GetNameSafe(TargetASC->GetAvatarActor()), *GetNameSafe(Request.GameplayEffectClass),
		Request.Damage, *GetNameSafe(Request.SourceActor), *Request.HitBoneName.ToString(),
		*Request.HitLocation.ToCompactString());
	ExternalGameplayEffectDamageDelegate.Broadcast(Request);
	return false;
}
