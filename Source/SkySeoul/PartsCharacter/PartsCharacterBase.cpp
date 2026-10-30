#include "PartsCharacterBase.h"
#include "PartsComponent.h"
#include "PartsDamageManager.h"
#include "PartsSetupData.h"

#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

APartsCharacterBase::APartsCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PartsComponent = CreateDefaultSubobject<UPartsComponent>(TEXT("PartsComponent"));
	PartsDamageManager = CreateDefaultSubobject<UPartsDamageManager>(TEXT("PartsDamageManager"));
}

void APartsCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Recover if BP cleared the default-subobject pointer / component.
	if (!PartsComponent)
	{
		PartsComponent = FindComponentByClass<UPartsComponent>();
	}
	if (!PartsComponent)
	{
		PartsComponent = NewObject<UPartsComponent>(this, TEXT("PartsComponent_Runtime"));
		PartsComponent->RegisterComponent();
		UE_LOG(LogTemp, Error,
			TEXT("Parts: %s had no PartsComponent — created at runtime. Re-add PartsComponent on the BP CDO."),
			*GetName());
	}

	if (!PartsDamageManager)
	{
		PartsDamageManager = FindComponentByClass<UPartsDamageManager>();
	}

	if (PartsComponent)
	{
		if (PartsSetupData)
		{
			PartsComponent->SetupData = PartsSetupData;
		}

		PartsComponent->DeathExplosionImpulse = DeathExplosionImpulse;
		PartsComponent->DebrisLifeSpan = DebrisLifeSpan;
		PartsComponent->DefaultSeparateEffect = DefaultSeparateEffect;
		PartsComponent->DefaultSeparateSound = DefaultSeparateSound;
		PartsComponent->DefaultSeparateEffectScale = DefaultSeparateEffectScale;

		PartsComponent->OnPartDamaged.AddDynamic(this, &APartsCharacterBase::HandlePartDamaged);
		PartsComponent->OnPartDestroyed.AddDynamic(this, &APartsCharacterBase::HandlePartDestroyed);
		PartsComponent->OnDeathPartsExplosion.AddDynamic(this, &APartsCharacterBase::HandleDeathPartsExplosion);

		PartsComponent->InitializeParts();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Parts: %s still has no PartsComponent — part hits will never map."), *GetName());
	}
}

bool APartsCharacterBase::ApplyPartDamage(FGameplayTag PartTag, float Damage)
{
	return PartsComponent ? PartsComponent->ApplyPartDamage(PartTag, Damage) : false;
}

bool APartsCharacterBase::ApplyPartDamageAtBone(FName BoneOrSocketName, float Damage)
{
	return PartsComponent ? PartsComponent->ApplyPartDamageAtBone(BoneOrSocketName, Damage) : false;
}

FPartsHitDamageResult APartsCharacterBase::ApplyHitDamage(
	const FHitResult& Hit,
	float Damage,
	AController* InstigatorController,
	AActor* DamageCauser)
{
	if (PartsDamageManager)
	{
		return PartsDamageManager->ApplyHitDamage(Hit, Damage, InstigatorController, DamageCauser);
	}
	return FPartsHitDamageResult();
}

void APartsCharacterBase::DestroyPart(FGameplayTag PartTag)
{
	if (PartsComponent)
	{
		PartsComponent->DestroyPart(PartTag);
	}
}

void APartsCharacterBase::HandleOwnerDeathPartsExplosion()
{
	if (!PartsComponent)
	{
		return;
	}

	PartsComponent->DeathExplosionImpulse = DeathExplosionImpulse;
	PartsComponent->DebrisLifeSpan = DebrisLifeSpan;
	PartsComponent->DefaultSeparateEffect = DefaultSeparateEffect;
	PartsComponent->DefaultSeparateSound = DefaultSeparateSound;
	PartsComponent->DefaultSeparateEffectScale = DefaultSeparateEffectScale;
	PartsComponent->HandleOwnerDeathPartsExplosion();
}

void APartsCharacterBase::ResetAllParts()
{
	if (PartsComponent)
	{
		PartsComponent->ResetAllParts();
	}
}

FGameplayTag APartsCharacterBase::ResolvePartTagFromBone(FName BoneOrSocketName) const
{
	return PartsComponent ? PartsComponent->ResolvePartTagFromBone(BoneOrSocketName) : FGameplayTag();
}

bool APartsCharacterBase::IsPartDestroyed(FGameplayTag PartTag) const
{
	return PartsComponent && PartsComponent->IsPartDestroyed(PartTag);
}

float APartsCharacterBase::GetPartHP(FGameplayTag PartTag) const
{
	return PartsComponent ? PartsComponent->GetPartHP(PartTag) : 0.0f;
}

EPartsArmState APartsCharacterBase::GetLeftArmState() const
{
	return PartsComponent ? PartsComponent->GetLeftArmState() : EPartsArmState::Intact;
}

EPartsArmState APartsCharacterBase::GetRightArmState() const
{
	return PartsComponent ? PartsComponent->GetRightArmState() : EPartsArmState::Intact;
}

bool APartsCharacterBase::IsHeadIntact() const
{
	return PartsComponent ? PartsComponent->IsHeadIntact() : true;
}

bool APartsCharacterBase::CanShoot() const
{
	return PartsComponent ? PartsComponent->CanShoot() : true;
}

void APartsCharacterBase::HandlePartDamaged(FGameplayTag PartTag, float RemainingHP)
{
	OnPartDamaged.Broadcast(PartTag, RemainingHP);
}

void APartsCharacterBase::HandlePartDestroyed(FGameplayTag PartTag)
{
	OnPartDestroyed.Broadcast(PartTag);
	BP_OnPartDestroyedFX(PartTag);
}

void APartsCharacterBase::HandleDeathPartsExplosion(FVector Origin, float Impulse)
{
	OnDeathPartsExplosion.Broadcast(Origin, Impulse);
	PlayDeathExplosionBuiltinFX(Origin);
	BP_OnDeathPartsExplosionFX(Origin, Impulse);
}

void APartsCharacterBase::PlayDeathExplosionBuiltinFX(FVector Origin) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (DeathExplosionEffect)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			World,
			DeathExplosionEffect,
			Origin,
			GetActorRotation(),
			FVector(DeathExplosionFXScale),
			true
		);
	}

	if (DeathExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, DeathExplosionSound, Origin);
	}
}
