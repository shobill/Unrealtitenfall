// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayTags.h"
#include "CharacterBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCharacterDiedDelegate, ACharacterBase*, CharacterBase);

class UAbilitySetData;
class AProjectPlayerState;
class UStaticMeshComponent;
class UBoxComponent;
class UInventoryComponent;

UCLASS()
class SKYSEOUL_API ACharacterBase : public ACharacter, public IAbilitySystemInterface, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	ACharacterBase(const class FObjectInitializer& ObjectInitializer);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void Attack();

	UPROPERTY(BlueprintAssignable)
	FCharacterDiedDelegate OnCharacterDied;

	UFUNCTION(BlueprintCallable, Category = "Character")
	virtual bool IsAlive() const;

	UFUNCTION(BlueprintCallable, Category = "Level")
	float GetCharacterLevel() const;
	UFUNCTION(BlueprintCallable, Category = "Character")
	float GetHealth() const;
	UFUNCTION(BlueprintCallable, Category = "Character")
	float GetMaxHealth() const;
	UFUNCTION(BlueprintCallable, Category = "Character")
	float GetSp() const;
	UFUNCTION(BlueprintCallable, Category = "Character")
	float GetMaxSp() const;

	virtual void PossessedBy(AController* NewController) override;

	UFUNCTION(BlueprintCallable, Category = "Damage Event")
	void OnHitEvent(AActor* DamageAttacker, float DamageAmount = 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName CharacterName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	UInventoryComponent* InventoryComponent;

protected:

	TObjectPtr<class UCharacterAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<class UCharacterAttributeSetBase> AttributeSetBase;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Abilities")
	TArray<TObjectPtr<UAbilitySetData>> AbilitySets;

	FGameplayTag DeadTag;
	FGameplayTag EffectRemoveOnDeathTag;

	virtual void InitializeStartingValues(AProjectPlayerState* PS);

	virtual void SetHealth(float Health);
	virtual void SetSp(float Sp);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer GameplayTagContainer;

	// Gameplay Tag Interface implementation
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override { TagContainer = GameplayTagContainer; return; }

	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override { return GameplayTagContainer.HasTag(TagToCheck); }

	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override { return GameplayTagContainer.HasAll(TagContainer); }

	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override { return GameplayTagContainer.HasAny(TagContainer); }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//------------------------------------------------------------------------------------------New//

	// Take Damage ------------------------------------------------------- //
	virtual float TakeDamage(float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	// Melee Action ------------------------------------------------------ //
	/*UFUNCTION()
	void OnMeleeAreaOverlapBegin(UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);
	void DisableMeleeTrigger();*/

	UFUNCTION(BlueprintCallable, Category = "Down->StandUP")
	void StandUp();

	UFUNCTION(BlueprintCallable)
	void MeleeType_ComboAttack();
	UFUNCTION(BlueprintCallable)
	void GunType_Fire();
	/*UFUNCTION(BlueprintCallable)
	void JustMeleeAttack();*/

	UFUNCTION(BlueprintCallable)
	void CanNotAction();
	UFUNCTION(BlueprintCallable)
	void RecoveryActionState();
	UFUNCTION(BlueprintCallable)
	void CharacterDown();
	UFUNCTION(BlueprintCallable)
	void CharacterStandUp();

	UFUNCTION(BlueprintCallable, Category = "Collision Setting")
	void CollisionSet_Unbeatable();
	UFUNCTION(BlueprintCallable, Category = "Collision Setting")
	void CollisionSet_NormalCollision();
	UFUNCTION(BlueprintCallable, Category = "Collision Setting")
	void CollisionSet_CapsuleNormal();
	UFUNCTION(BlueprintCallable, Category = "Collision Setting")
	void CollisionSet_CapsuleIgnorePawn();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SpawnProjectile();

	UFUNCTION(BlueprintCallable, Category = "Tags")
	void AddTag(FName Tag);
	UFUNCTION(BlueprintCallable, Category = "Tags")
	void RemoveTag(FName Tag);

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void LogTags();

private:
	UFUNCTION(BlueprintCallable)
	void KnockBack(ACharacter* HitCharacter, float KnockbackPower);

	void CanActionStateReset();

	void SetAttackOptions();

	void SetMovementSpeed();

	// Attack State Admin ================================================ //
	void InitAttackState();
	void NowAttack();
	void StateInitTimer(float Delay);

	// ----------------------------------------------------------------------- //
			// Value

public:
	// 이후 제거
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float MaxHP = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float CurHP = 100.0f;
	// -------

	// 이후 수정 필요
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite)
	float AggressiveStrength;
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite)
	float MeleeStrength;
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite)
	float ProjectileStrength;
	// -----

	// 이후 수정 필요
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite)
	float Defense;
	// -----

	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite)
	float KnockBackPower;
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite)
	float MoveSpeed = 600.f; // MaxWalkSpeed; 0이면 BeginPlay에서 이동이 거의 안 됨
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite)
	float AttackDelay;
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite)
	float MeleeDelay;
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite)
	float FireMaxRange;
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite)
	float StandUpCastingTime = 3;
	UPROPERTY(EditAnywhere, Category = "Common Combat Stat", BlueprintReadWrite)
	float LaunchPower = 3000;
	// ------------------------------------------------ //
	UPROPERTY(EditAnywhere, Category = "Common Anim State Boolean", BlueprintReadWrite)
	bool IsMoving; // 실질적으론 스테이트 점검용으로만 사용
	UPROPERTY(EditAnywhere, Category = "Common Anim State Boolean", BlueprintReadWrite)
	bool IsDamaged;
	UPROPERTY(EditAnywhere, Category = "Common Anim State Boolean", BlueprintReadWrite)
	bool IsDead;
	UPROPERTY(EditAnywhere, Category = "Common Anim State Boolean", BlueprintReadWrite)
	bool IsDown;
	// ------------------------------------------------ //
	UPROPERTY(EditAnywhere, Category = "Character State Boolean", BlueprintReadWrite)
	bool CanAction;
	UPROPERTY(EditAnywhere, Category = "Character State Boolean", BlueprintReadWrite)
	bool CanAttack; // 총기 발사등 일반 공격
	UPROPERTY(EditAnywhere, Category = "Character State Boolean", BlueprintReadWrite)
	bool CanMelee; // 밀치기등의 근접 공격
	UPROPERTY(EditAnywhere, Category = "Character State Boolean", BlueprintReadWrite)
	bool CanControlCamera; // 카메라를 통한 시점 돌리기
	UPROPERTY(EditAnywhere, Category = "Character State Boolean", BlueprintReadWrite)
	bool WeakPointBreak;
	UPROPERTY(EditAnywhere, Category = "Character State Boolean", BlueprintReadWrite)
	bool ChangePointBreak;
	// ------------------------------------------------ //
	UPROPERTY(EditAnywhere, Category = "Common Anim Montage", BlueprintReadWrite)
	UAnimMontage* FireMontage;
	UPROPERTY(EditAnywhere, Category = "Common Anim Montage", BlueprintReadWrite)
	UAnimMontage* MeleeMontage;
	UPROPERTY(EditAnywhere, Category = "Common Anim Montage", BlueprintReadWrite)
	UAnimMontage* HitReactionMontage;
	// ------------------------------------------------ //
	UPROPERTY(EditAnywhere, Category = "Damage Event", BlueprintReadWrite)
	ACharacterBase* Attacker;
	// ------------------------------------------------ //
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<class AProjectileBase> ProjectileClass;

	// ------------------------------------------------ //
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tags")
	FName ReactionTag = TEXT("Reaction");

protected:
	UPROPERTY(EditAnywhere, Category = "Common Parts Action Value", BlueprintReadWrite)
	int PointDamage; // 필요에따라 다른 값 지정

	UPROPERTY(EditAnywhere, Category = "Common Parts Action Value", BlueprintReadWrite)
	int BreakPointHP; // 해당 캐릭터에서 값 지정
	UPROPERTY(EditAnywhere, Category = "Common Parts Action Value", BlueprintReadWrite)
	FName BreakPoint; // 해당 캐릭터에서 값 지정
	UPROPERTY(EditAnywhere, Category = "Common Parts Action Value", BlueprintReadWrite)
	bool HitBreakPoint;
	void BreakPartsCalculating(int BreakPointDamage);
	void BreakParts();

	UPROPERTY(EditAnywhere, Category = "Common Parts Action Value", BlueprintReadWrite)
	int ChangePointHP; // 해당 캐릭터에서 값 지정
	UPROPERTY(EditAnywhere, Category = "Common Parts Action Value", BlueprintReadWrite)
	FName ChangePoint; // 해당 캐릭터에서 값 지정
	UPROPERTY(EditAnywhere, Category = "Common Parts Action Value", BlueprintReadWrite)
	bool HitChangePoint;
	/* 체인지할 다른 액터 클래스 추가 필요
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AMyGun> GunClass;

	AMyGun* Gun;
	*/
	void ChangingPartsCalculating(int PointDamaged);
	void ChangingParts();

	FCollisionQueryParams Params;

	UPROPERTY(VisibleAnywhere, Category = "Move Component")
	UCharacterMovementComponent* MoveComponent;

private:
	/*UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UBoxComponent* MeleeArea;*/

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USceneComponent* ProjectileSpawnPoint;

};
