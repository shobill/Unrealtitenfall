#include "TitanDefence/Grapple/GAS/TDGameplayAbility_Grapple.h"
#include "TitanDefence/Grapple/GAS/TDGrappleTags.h"
#include "TitanDefence/Grapple/TDGrappleComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "GameFramework/PlayerController.h"

UTDGameplayAbility_Grapple::UTDGameplayAbility_Grapple()
{
	// [GAS 각주 2] 이번 기능은 로컬 줄 표시입니다. 서버 타깃 검증/이동 예측을 구현한 것은 아닙니다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ActivationPolicy = ESkySeoulAbilityActivationPolicy::OnInputTriggered;
	InputTag = TDGrappleTags::Input_Grapple;
	FGameplayTagContainer Tags;
	Tags.AddTag(TDGrappleTags::Ability_Grapple);
	SetAssetTags(Tags);
	// 부모가 이미 State.Dead와 State.Debuff.Stun을 발동 금지 태그로 지정합니다.
	ActivationOwnedTags.AddTag(TDGrappleTags::State_Grappling);
}

bool UTDGameplayAbility_Grapple::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& ActorInfo && ActorInfo->AvatarActor.IsValid()
		&& ActorInfo->AvatarActor->FindComponentByClass<UTDGrappleComponent>();
}

bool UTDGameplayAbility_Grapple::GetAimRay(const FGameplayAbilityActorInfo* ActorInfo,
	FVector& Start, FVector& Direction) const
{
	const APlayerController* PC = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;
	if (!PC || !PC->IsLocalController()) return false;
	FRotator Rotation;
	PC->GetPlayerViewPoint(Start, Rotation);
	Direction = Rotation.Vector();
	return true;
}

void UTDGameplayAbility_Grapple::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	UTDGrappleComponent* Grapple = ActorInfo->AvatarActor->FindComponentByClass<UTDGrappleComponent>();
	FVector Start, Direction;
	FHitResult Hit;
	// [GAS 각주 3] 지형 검사를 먼저 합니다. 빗나간 경우 비용/쿨다운을 소비하지 않고 종료합니다.
	if (!Grapple || !GetAimRay(ActorInfo, Start, Direction) || !Grapple->FindTerrainTarget(Start, Direction, Hit))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// GAS 표준 비용/쿨다운 확장 지점입니다. 기본값은 무료이며 별도 효과를 임의로 부과하지 않습니다.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ActiveGrapple = Grapple;
	Grapple->OnGrappleReleased.AddDynamic(this, &ThisClass::OnConnectionReleased);
	if (!Grapple->AttachToTerrain(Hit))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// [GAS 각주 4] Tick으로 입력을 감시하지 않고 AbilityTask가 입력 해제를 기다립니다.
	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
	ReleaseTask->ReadyForActivation();
	if (!IsActive()) return;
	// [GAS 각주 5] 사용 중 사망/기절도 같은 취소 경로로 처리합니다.
	for (const FGameplayTag& Tag : ActivationBlockedTags)
	{
		UAbilityTask_WaitGameplayTagAdded* Task = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, Tag, nullptr, true);
		Task->Added.AddDynamic(this, &ThisClass::OnBlockedTagAdded);
		Task->ReadyForActivation();
		if (!IsActive()) return;
	}
}

void UTDGameplayAbility_Grapple::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UTDGameplayAbility_Grapple::OnBlockedTagAdded()
{
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UTDGameplayAbility_Grapple::OnConnectionReleased()
{
	if (IsActive()) CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UTDGameplayAbility_Grapple::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive()) return;
	// [GAS 각주 6] 모든 종료 사유가 이곳을 거칩니다. 델리게이트를 먼저 해제해 재귀 취소를 막습니다.
	if (UTDGrappleComponent* Grapple = ActiveGrapple.Get())
	{
		Grapple->OnGrappleReleased.RemoveDynamic(this, &ThisClass::OnConnectionReleased);
		Grapple->ReleaseGrapple();
	}
	ActiveGrapple.Reset();
	// Super가 AbilityTask와 ActivationOwnedTags(State.TitanDefense.Grappling)를 정리합니다.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
