// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Character/Abilities/CharacterAbilitySystemComponent.h"
#include "Character/Abilities/Attributes/CharacterAttributeSetBase.h"
#include "Character/Abilities/CharacterGameplayAbility.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Damage.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/InventoryComponent.h"

#include "ProjectPlayerState.h"
#include "Abilities/AbilitySetData.h"
#include "Actor/ProjectileBase.h"

#include "Perception/AISense.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
ACharacterBase::ACharacterBase(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bAlwaysRelevant = true;

	//GetCapsuleComponent()->

	DeadTag = FGameplayTag::RequestGameplayTag(FName("State.Dead"));
	EffectRemoveOnDeathTag = FGameplayTag::RequestGameplayTag(FName("State.RemoveOnDeath"));

	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	/*MeleeArea = CreateDefaultSubobject<UBoxComponent>
		(TEXT("Melee Attack Area"));
	MeleeArea->SetupAttachment(GetMesh(),
		TEXT("Melee Attack Area"));*/

	ProjectileSpawnPoint = CreateDefaultSubobject<USceneComponent>
		(TEXT("ProjectileSpawnPoint"));
	ProjectileSpawnPoint->SetupAttachment(GetMesh(),
		TEXT("ProjectileSpawnPoint"));
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	CanAction = true;
	IsDead = false;
	IsDamaged = false;
	IsDown = false;
}

// Called when the game starts or when spawned
void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();
	/*MeleeArea->OnComponentBeginOverlap.AddDynamic(this,
		&ACharacterBase::OnMeleeAreaOverlapBegin);*/

	CollisionSet_NormalCollision(); // ??? ????? ????
	SetMovementSpeed(); // ??? ??? ????
	SetAttackOptions(); // ???? ???? ??? ????
	//DisableMeleeTrigger(); // ???? ???? ????? off
	RecoveryActionState(); // ???? ???? ????
}

// Called every frame
void ACharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

UAbilitySystemComponent* ACharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

void ACharacterBase::Attack()
{
}

bool ACharacterBase::IsAlive() const
{
	return GetHealth() > 0.f;
}

float ACharacterBase::GetCharacterLevel() const
{
	if (AttributeSetBase.IsValid())
		return AttributeSetBase->GetLevel();
	return 0.0f;
}

float ACharacterBase::GetHealth() const
{
	if (AttributeSetBase.IsValid())
		return AttributeSetBase->GetHealth();
	return 0.0f;
}

float ACharacterBase::GetMaxHealth() const
{
	if (AttributeSetBase.IsValid())
		return AttributeSetBase->GetMaxHealth();
	return 0.0f;
}

float ACharacterBase::GetSp() const
{
	if (AttributeSetBase.IsValid())
		return AttributeSetBase->GetSp();
	return 0.0f;
}

float ACharacterBase::GetMaxSp() const
{
	if (AttributeSetBase.IsValid())
		return AttributeSetBase->GetMaxSp();
	return 0.0f;
}

void ACharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	AProjectPlayerState* PS = GetPlayerState<AProjectPlayerState>();
	if (PS)
	{
		InitializeStartingValues(PS);
		for (const UAbilitySetData* AbilitySet : AbilitySets)
		{
			if (AbilitySet)
			{
				AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, nullptr);
			}
		}
		SetHealth(GetMaxHealth());
		SetSp(GetMaxSp());
		IsDead = false;
	}

	// Always sync body HP — AI may possess before PlayerState is ready, and CurHP==0
	// makes the first part-hit body share trigger instant death + all-parts explosion.
	if (MaxHP <= 0.0f)
	{
		MaxHP = 100.0f;
	}
	if (CurHP <= 0.0f || CurHP > MaxHP)
	{
		CurHP = MaxHP;
	}
}

void ACharacterBase::OnHitEvent(AActor* DamageAttacker, float DamageAmount)
{
	if (!HitReactionMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Character [%s]: Not Set Hit Reaction Montage // [CharacterBase.cpp Line.141]"),
		*GetActorNameOrLabel());

		// return // ????? ????????? ?????? ??????????.
	}

	else
	{
		PlayAnimMontage(HitReactionMontage);
	}

	if(!ActorHasTag("Player"))
	{
		if (!DamageAttacker)
		{
			UE_LOG(LogTemp, Error, TEXT("DamageAttacker Is Null// [CharacterBase.cpp Line.151]"));

			return;
		}
		if (!DamageAmount)
		{
			UE_LOG(LogTemp, Error, TEXT("DamageAmount Is Null// [CharacterBase.cpp Line.159]"));

			return;
		}

		UAISense_Damage::ReportDamageEvent(GetWorld(), this, DamageAttacker,
			DamageAmount, this->GetActorLocation(), DamageAttacker->GetActorLocation());
	}
}

void ACharacterBase::InitializeStartingValues(AProjectPlayerState* PS)
{
	AbilitySystemComponent = Cast<UCharacterAbilitySystemComponent>(PS->GetAbilitySystemComponent());
	AbilitySystemComponent->InitAbilityActorInfo(PS, this);
	AttributeSetBase = PS->GatAttributeSetBase();
	AbilitySystemComponent->SetTagMapCount(DeadTag, 0);
	AbilitySystemComponent->AddLooseGameplayTags(GameplayTagContainer);
}

void ACharacterBase::SetHealth(float Health)
{
	if (AttributeSetBase.IsValid())
		return AttributeSetBase->SetHealth(Health);
}

void ACharacterBase::SetSp(float Sp)
{
	if (AttributeSetBase.IsValid())
		return AttributeSetBase->SetSp(Sp);
}

float ACharacterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (IsDead)
	{
		return 0.f;
	}

	float DamageToApply = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (DamageToApply <= 0.f)
	{
		return 0.f;
	}

	Attacker = Cast<ACharacterBase>(DamageCauser);

	CurHP = FMath::Max(0.f, CurHP - DamageToApply);
	IsDamaged = true;

	if (AttributeSetBase.IsValid())
	{
		SetHealth(CurHP);
	}

	if (DamageCauser)
	{
		if (DamageCauser->ActorHasTag("Player") || DamageCauser->ActorHasTag("Enemy"))
		{
			if (Attacker && Attacker->HitBreakPoint && BreakPointHP > 0)
			{
				BreakPartsCalculating(1);
			}
			else if (Attacker && Attacker->HitChangePoint && ChangePointHP > 0)
			{
				ChangingPartsCalculating(1);
			}
		}
		else if (DamageCauser->ActorHasTag("Projectile"))
		{
			CharacterDown();
		}
	}

	if (CurHP <= 0.f && !IsDead)
	{
		IsDead = true;

		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			const FGameplayTag TagToAdd = DeadTag.IsValid()
				? DeadTag
				: FGameplayTag::RequestGameplayTag(FName("State.Dead"));
			if (TagToAdd.IsValid())
			{
				ASC->AddLooseGameplayTag(TagToAdd);
			}
		}

		OnCharacterDied.Broadcast(this);
	}

	UE_LOG(LogTemp, Warning, TEXT("--- Hit Target Data ---"));
	UE_LOG(LogTemp, Log, TEXT("MaxHP: %f"), MaxHP);
	UE_LOG(LogTemp, Log, TEXT("NowHP: %f"), CurHP);
	UE_LOG(LogTemp, Log, TEXT("Damage: %f"), DamageToApply);

	return DamageToApply;
}

/*
void ACharacterBase::OnMeleeAreaOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UE_LOG(LogTemp, Error, TEXT("Melee call"));
	if (OtherActor && OtherActor != this)
	{
		UE_LOG(LogTemp, Error, TEXT("Actor Overlap"));

		// ?????? ???????? ??????? ????? ????
		if (OtherActor->ActorHasTag("Player") && ActorHasTag("Tank"))
		{
			//UE_LOG(LogTemp, Error, TEXT("Player Overlap"));
			ACharacter* HitCharacter = Cast<ACharacter>(OtherActor);
			KnockBack(HitCharacter, LaunchPower);
		}

		UGameplayStatics::ApplyDamage(OtherActor, MeleeStrength,
			GetController(), this, nullptr);

		UE_LOG(LogTemp, Display, TEXT("%f"), MeleeStrength);
	}
}

void ACharacterBase::DisableMeleeTrigger()
{
	MeleeArea->SetGenerateOverlapEvents(false);
	//UE_LOG(LogTemp, Error, TEXT("Melee Disable"));
}
*/

void ACharacterBase::KnockBack(ACharacter* HitCharacter, float KnockbackPower)
{
	FVector AtkDir = HitCharacter->GetActorLocation() - GetActorLocation();
	AtkDir.Normalize();
	FVector KnockBackDistance = AtkDir * KnockbackPower;

	HitCharacter->LaunchCharacter(KnockBackDistance, false, false);
}

void ACharacterBase::CanActionStateReset()
{
	CanAction = true;
}

void ACharacterBase::SetAttackOptions()
{
	Params.AddIgnoredActor(this);
	// ???? ???????? ???? ???? ????
}

void ACharacterBase::SetMovementSpeed()
{
	MoveComponent = GetCharacterMovement();
	if (!MoveComponent)
	{
		return;
	}

	// Avoid wiping ctor/BP MaxWalkSpeed with an unset 0 MoveSpeed.
	const float AppliedSpeed = MoveSpeed > 0.f ? MoveSpeed : 600.f;
	MoveComponent->MaxWalkSpeed = AppliedSpeed;
}

void ACharacterBase::StandUp()
{
	IsDown = false;

	FTimerHandle ActionTimer;
	GetWorldTimerManager().SetTimer(ActionTimer, this,
		&ACharacterBase::CanActionStateReset, 1, false);
}

void ACharacterBase::MeleeType_ComboAttack()
{
	UE_LOG(LogTemp, Warning, TEXT("Melee Combo"));
}

/*
void ACharacterBase::JustMeleeAttack()
{
	if (CanMelee && CanAction)
	{
		NowAttack();

		if (MeleeMontage)
			PlayAnimMontage(MeleeMontage);

		MeleeArea->SetGenerateOverlapEvents(true);

		FTimerHandle DisableTriggerTimer;
		GetWorldTimerManager().SetTimer(DisableTriggerTimer, this,
			&ACharacterBase::DisableMeleeTrigger, 0.2f, false);

		StateInitTimer(MeleeDelay);
	}
}
*/

void ACharacterBase::CanNotAction()
{
	CanControlCamera = false;
	CanAction = false;
	CanAttack = false;
	CanMelee = false;
}

void ACharacterBase::RecoveryActionState()
{
	CanControlCamera = true;
	CanAction = true;
	CanAttack = true;
	CanMelee = true;
}

void ACharacterBase::CharacterDown()
{
	IsDown = true;
	CanNotAction();

	FTimerHandle StandUpTimer;
	GetWorld()->GetTimerManager().SetTimer(StandUpTimer, this,
		&ACharacterBase::CharacterStandUp, StandUpCastingTime, false);
}

void ACharacterBase::CharacterStandUp()
{
	IsDown = false;
	RecoveryActionState();
}

void ACharacterBase::CollisionSet_Unbeatable()
{
	GetMesh()->SetCollisionProfileName(TEXT("Unbeatable"));
}

void ACharacterBase::CollisionSet_NormalCollision()
{
	GetMesh()->SetCollisionProfileName(TEXT("NormalCollision"));
}

void ACharacterBase::CollisionSet_CapsuleNormal()
{
	GetMesh()->SetCollisionProfileName(TEXT("CapsuleCompSet"));
}

void ACharacterBase::CollisionSet_CapsuleIgnorePawn()
{
	GetMesh()->SetCollisionProfileName(TEXT("Capsule_PawnIgnore"));
}

void ACharacterBase::SpawnProjectile()
{
	FVector SpawnLocation = ProjectileSpawnPoint->GetComponentLocation();
	FRotator SpawnRotator = GetActorRotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this; // ?????? ????
	SpawnParams.Instigator = GetInstigator(); // Instigator ????

	GetWorld()->SpawnActor<AProjectileBase>(ProjectileClass, SpawnLocation, SpawnRotator, SpawnParams);
}

void ACharacterBase::AddTag(FName Tag)
{
	Tags.Add(Tag);
}

void ACharacterBase::RemoveTag(FName Tag)
{
	Tags.Remove(Tag);
}

void ACharacterBase::LogTags()
{
	for (const FName& Tag : Tags)
	{
		// ?? ????? ????? ???????.
		UE_LOG(LogTemp, Log, TEXT("Tag: %s"), *Tag.ToString());
	}
}

// ???? ????
void ACharacterBase::GunType_Fire()
{
	if (CanAttack && CanAction)
	{
		NowAttack();

		AController* ShooterController;
		FVector CurrentLocation;
		FRotator CurrentRotation;
		// ------------------------------- //
		ShooterController = this->GetController();

		if (ShooterController == nullptr)
			return;

		ShooterController->GetPlayerViewPoint(CurrentLocation, CurrentRotation);

		FVector ShotDirection = -CurrentRotation.Vector();

		FHitResult HitResult;

		PlayAnimMontage(FireMontage);

		FVector InRange = CurrentLocation + CurrentRotation.Vector() * FireMaxRange;

		bool bSuccessHit = GetWorld()->LineTraceSingleByChannel(HitResult,
			CurrentLocation, InRange, ECC_GameTraceChannel1, Params);

		if (bSuccessHit)
		{
			AActor* HitActor = HitResult.GetActor();
			if (HitActor != nullptr)
			{

				FPointDamageEvent DamageEvent(AggressiveStrength, HitResult,
					ShotDirection, nullptr);

				float GiveDamage = AggressiveStrength;

				// ???? ????
				if (HitResult.BoneName == BreakPoint)
				{
					HitBreakPoint = true;
					HitChangePoint = false;

					GiveDamage *= 2; // ???? ????

					//UE_LOG(LogTemp, Warning, TEXT("<<< HeadShot >>>"));
					//UE_LOG(LogTemp, Warning, TEXT("Damage: %f"), GiveDamage);

					HitActor->TakeDamage(GiveDamage, DamageEvent, ShooterController,
						this);
				}

				// ??? ???? ???? ????
				else if (HitResult.BoneName == ChangePoint)
				{
					HitChangePoint = true;
					HitBreakPoint = false;

					//UE_LOG(LogTemp, Warning, TEXT("<<< HeadShot >>>"));
					//UE_LOG(LogTemp, Warning, TEXT("Damage: %f"), GiveDamage);

					HitActor->TakeDamage(GiveDamage, DamageEvent, ShooterController,
						this);
				}

				// ??? ????
				else
				{
					HitChangePoint = false;
					HitBreakPoint = false;

					//UE_LOG(LogTemp, Display, TEXT("--- Normal ---"));
					//UE_LOG(LogTemp, Display, TEXT("Damage: %f"), GiveDamage);

					HitActor->TakeDamage(GiveDamage, DamageEvent, ShooterController,
						this);
				}

				UE_LOG(LogTemp, Error, TEXT("Hit Point: %s"), *HitResult.BoneName.ToString());
			}
		}

		StateInitTimer(AttackDelay);
	}
}


void ACharacterBase::InitAttackState()
{
	CanAttack = true;
	CanMelee = true;
}

void ACharacterBase::NowAttack()
{
	CanAttack = false;
	CanMelee = false;
}

void ACharacterBase::StateInitTimer(float Delay)
{
	FTimerHandle DelayTimer;
	GetWorldTimerManager().SetTimer(DelayTimer, this,
		&ACharacterBase::InitAttackState, Delay, false);
}

void ACharacterBase::BreakPartsCalculating(int BreakPointDamage)
{
	BreakPointHP -= BreakPointDamage;

	if (BreakPointHP <= 0)
	{
		WeakPointBreak = true;

		if (WeakPointBreak)
		{
			BreakParts();

			// IsDead
		}
	}
}

void ACharacterBase::BreakParts()
{
	GetMesh()->HideBoneByName(BreakPoint, EPhysBodyOp::PBO_Term);
	// Term: ???? ?????? ???? ???? ????
	GetMesh()->SetAllBodiesBelowSimulatePhysics(BreakPoint, false);
}

void ACharacterBase::ChangingPartsCalculating(int PointDamaged)
{
	ChangePointHP -= PointDamage;

	if (ChangePointHP <= 0)
	{
		ChangePointBreak = true;

		if (ChangePointBreak)
		{
			ChangingParts();
		}
	}
}

void ACharacterBase::ChangingParts()
{
	GetMesh()->HideBoneByName(ChangePoint, EPhysBodyOp::PBO_None);
	/* ??? ?????? ???? ???? ???? ???? ?? ????? ????
	Gun->AttachToComponent(GetMesh(),
		FAttachmentTransformRules::KeepRelativeTransform,
		TEXT("WeaponSocket"));
	Gun->SetOwner(this);
	*/
}


