// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/SkySeoulCharacter.h"
#include "Character/Abilities/AbilitySetData.h"
#include "TDPlayerCharacte.generated.h"

class UTDGrappleComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
class UCharacterAbilitySystemComponent;
class UEnhancedInputComponent;

/**
 * 
 */
UCLASS()
class SKYSEOUL_API ATDPlayerCharacte : public ASkySeoulCharacter
{
	GENERATED_BODY()

public:
	ATDPlayerCharacte(const FObjectInitializer& ObjectInitializer);

	//  BP_TDPlayerCharacter에도 이 컴포넌트가 상속
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grapple")
	TObjectPtr<UTDGrappleComponent> GrappleComponent;

	// [GAS 각주 9] 기존 AbilitySetData 에셋으로 능력/효과를 편집할 수 있습니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|GAS")
	TObjectPtr<UAbilitySetData> GrappleAbilitySet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Input")
	TSoftObjectPtr<UInputAction> GrappleInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grapple|Input")
	TSoftObjectPtr<UInputMappingContext> GrappleMappingContext;

	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void InitializeStartingValues(AProjectPlayerState* PS) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void GrappleInputPressed();
	void GrappleInputReleased();
	void CleanupGrappleAbility();
	void RemoveGrappleInputContext();
	UPROPERTY(Transient)
	FAbilitySetData_GrantedHandles GrappleGrantedHandles;
	TArray<uint32> GrappleInputBindingHandles;
	TWeakObjectPtr<UEnhancedInputComponent> BoundGrappleInput;
	TWeakObjectPtr<UCharacterAbilitySystemComponent> GrappleASC;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> GrappleInputSubsystem;
	TWeakObjectPtr<UInputMappingContext> InstalledGrappleContext;
	bool bGrappleAbilitiesGranted = false;
};
