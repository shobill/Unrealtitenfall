#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TitanDefence/Robot/Damage/TDBodyDamageReceiver.h"
#include "TitanDefence/Robot/State/TDRobotStateTypes.h"
#include "TDRobotStateComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UTDRobotAbilitySystemComponent;
class UTDRobotAttributeSet;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDRobotStateInitialized,
	UTDRobotStateComponent*, RobotState, UAbilitySystemComponent*, AbilitySystemComponent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FTDRobotBodyDamaged,
	float, RequestedDamage, float, PreviousHealth, float, CurrentHealth,
	const FTDDamageResult&, DamageResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FTDRobotHealthChanged,
	float, PreviousHealth, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDRobotBodyDeath,
	const FTDDamageResult&, DamageResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTDRobotBodyDamageRejected,
	const FTDDamageResult&, DamageResult,
	ETDRobotBodyDamageRejectionReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTDRobotExternalGameplayEffectDamageReceived,
	const FTDRobotExternalGameplayEffectDamage&, DamageRequest);

/** GAS-backed body-state facade owned exactly once by ATDBaseRobotCharacter. */
UCLASS(ClassGroup = (TitanDefence))
class SKYSEOUL_API UTDRobotStateComponent : public UActorComponent, public ITDBodyDamageReceiver
{
	GENERATED_BODY()

public:
	UTDRobotStateComponent();

	/** Character-only dependency injection. This component never searches its owner. */
	bool InitializeStateSystem(
		UTDRobotAbilitySystemComponent* InAbilitySystemComponent,
		UTDRobotAttributeSet* InAttributeSet);

	virtual bool ApplyResolvedBodyDamage(
		float DamageAmount, const FTDDamageResult& DamageResult) override;

	UFUNCTION(BlueprintCallable, Category = "Titan Defence|Robot|State")
	void SetDamageEnabled(bool bEnabled) { bDamageEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|State")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|State")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|State")
	float GetHealthRatio() const;

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|State")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|State")
	bool IsStateInitialized() const { return bStateInitialized; }

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|State")
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "Titan Defence|Robot|State")
	UTDRobotAttributeSet* GetRobotAttributeSet() const { return RobotAttributeSet.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|State")
	FTDRobotStateInitialized OnStateInitialized;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|State")
	FTDRobotBodyDamaged OnBodyDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|State")
	FTDRobotHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|State")
	FTDRobotBodyDeath OnBodyDeath;

	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|State")
	FTDRobotBodyDamageRejected OnBodyDamageRejected;

	/** Emitted on the tick after an external GE's direct Health modifier was intercepted. */
	UPROPERTY(BlueprintAssignable, Category = "Titan Defence|Robot|State|Damage")
	FTDRobotExternalGameplayEffectDamageReceived OnExternalGameplayEffectDamageReceived;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Titan Defence|Robot|State|Damage",
		meta = (AllowPrivateAccess = "true"))
	ETDRobotExternalDamageFallbackPolicy ExternalDamageFallbackPolicy =
		ETDRobotExternalDamageFallbackPolicy::ApplyToBody;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Titan Defence|Robot|State|Effects",
		meta = (AllowPrivateAccess = "true", DisplayName = "Initial Attributes Effect Class"))
	TSubclassOf<UGameplayEffect> InitialAttributesEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Titan Defence|Robot|State|Effects",
		meta = (AllowPrivateAccess = "true", DisplayName = "Body Damage Effect Class"))
	TSubclassOf<UGameplayEffect> BodyDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Titan Defence|Robot|State|Effects",
		meta = (AllowPrivateAccess = "true", DisplayName = "Death State Effect Class"))
	TSubclassOf<UGameplayEffect> DeathStateEffectClass;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void HandleExternalGameplayEffectDamage(
		const FTDRobotExternalGameplayEffectDamage& DamageRequest);
	void ProcessQueuedExternalGameplayEffectDamage();
	void RemoveAttributeDelegates();
	void ApplyInitialAttributesIfNeeded();
	void HandleDeath(const FTDDamageResult& DamageResult);
	bool RejectBodyDamage(
		const FTDDamageResult& DamageResult,
		ETDRobotBodyDamageRejectionReason Reason);

	TWeakObjectPtr<UTDRobotAbilitySystemComponent> RobotAbilitySystemComponent;
	TWeakObjectPtr<UTDRobotAttributeSet> RobotAttributeSet;
	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle ExternalGameplayEffectDamageDelegateHandle;
	FTimerHandle ExternalGameplayEffectDamageTimerHandle;

	UPROPERTY(Transient)
	TArray<FTDRobotExternalGameplayEffectDamage> QueuedExternalGameplayEffectDamage;

	FTDDamageResult PendingDamageResult;
	bool bHasPendingDamageResult = false;
	bool bStateInitialized = false;
	bool bDamageEnabled = true;
	bool bDeathEventBroadcast = false;
	bool bInitialAttributesEffectApplied = false;
};

