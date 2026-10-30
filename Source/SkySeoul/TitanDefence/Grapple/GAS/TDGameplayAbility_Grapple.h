#pragma once
#include "Character/Abilities/CharacterGameplayAbility.h"
#include "TDGameplayAbility_Grapple.generated.h"

class UTDGrappleComponent;

/** GAS가 사용 가능 여부와 유지/취소를 관리하고, 컴포넌트는 지형 검사와 줄 표시만 처리합니다. */
UCLASS()
class SKYSEOUL_API UTDGameplayAbility_Grapple : public UCharacterGameplayAbility
{
	GENERATED_BODY()
public:
	UTDGameplayAbility_Grapple();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// [GAS 각주 1] 조준점 획득을 분리해 카메라 및 자동화 테스트가 같은 능력 수명주기를 사용합니다.
	virtual bool GetAimRay(const FGameplayAbilityActorInfo* ActorInfo, FVector& Start, FVector& Direction) const;

private:
	UFUNCTION() void OnInputReleased(float TimeHeld);
	UFUNCTION() void OnBlockedTagAdded();
	UFUNCTION() void OnConnectionReleased();
	TWeakObjectPtr<UTDGrappleComponent> ActiveGrapple;
};
