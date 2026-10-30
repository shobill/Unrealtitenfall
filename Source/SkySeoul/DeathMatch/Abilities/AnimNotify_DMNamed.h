#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_DMNamed.generated.h"

/**
 * Classic montage/sequence AnimNotify that forwards NotifyName to an active
 * UDMGameplayAbility_Fire (same string as ability FireNotifyName).
 * Use this OR AnimNotify_PlayMontageNotify — both are supported.
 */
UCLASS(meta = (DisplayName = "DM Named Notify"))
class SKYSEOUL_API UAnimNotify_DMNamed : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** Editor migration: preserve authored timing and replace bare named events with dispatchable notifies. */
	UFUNCTION(BlueprintCallable, Category = "DeathMatch|Editor")
	static int32 UpgradeMontageFireNotifies(UAnimMontage* Montage);
    UFUNCTION(BlueprintCallable, Category = "DeathMatch|Editor")
    static FString InspectFireMontage(UAnimMontage* Montage);
    UFUNCTION(BlueprintCallable, Category = "DeathMatch|Editor")
    static bool AddFireCycleNotify(UAnimMontage* Montage, float Time);

	/** Must match UDMGameplayAbility_Fire::FireNotifyName (e.g. "Fire"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeathMatch")
	FName NotifyName = TEXT("Fire");

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

#if WITH_EDITOR
	virtual FString GetNotifyName_Implementation() const override;
#endif
};
