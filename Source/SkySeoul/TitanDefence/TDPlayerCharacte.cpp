// Fill out your copyright notice in the Description page of Project Settings.


#include "TitanDefence/TDPlayerCharacte.h"
#include "TitanDefence/Grapple/TDGrappleComponent.h"
#include "TitanDefence/Grapple/GAS/TDGrappleAbilitySet.h"
#include "TitanDefence/Grapple/GAS/TDGrappleTags.h"
#include "Character/Abilities/CharacterAbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"

ATDPlayerCharacte::ATDPlayerCharacte(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GrappleComponent = CreateDefaultSubobject<UTDGrappleComponent>(TEXT("GrappleComponent"));
	GrappleInputAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/TitanDefense/Input/IA_TDGrapple.IA_TDGrapple")));
	GrappleMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/Game/TitanDefense/Input/IMC_TDGrapple.IMC_TDGrapple")));
}

void ATDPlayerCharacte::InitializeStartingValues(AProjectPlayerState* PS)
{
	Super::InitializeStartingValues(PS);
	UCharacterAbilitySystemComponent* ASC = Cast<UCharacterAbilitySystemComponent>(GetAbilitySystemComponent());
	if (GrappleASC.IsValid() && GrappleASC.Get() != ASC) CleanupGrappleAbility();
	GrappleASC = ASC;
	// [GAS 각주 10] PlayerState ASC가 준비된 뒤 서버가 한 번만 부여합니다. ASC를 새로 만들지 않습니다.
	if (HasAuthority() && ASC && !bGrappleAbilitiesGranted)
	{
		const UAbilitySetData* Set = GrappleAbilitySet ? GrappleAbilitySet.Get() : GetDefault<UTDGrappleAbilitySet>();
		Set->GiveToAbilitySystem(ASC, &GrappleGrantedHandles, this);
		bGrappleAbilitiesGranted = true;
	}
}

void ATDPlayerCharacte::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	RemoveGrappleInputContext();
	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	UInputAction* Action = GrappleInputAction.LoadSynchronous();
	APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC && PC->GetLocalPlayer()
		? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	UInputMappingContext* Context = GrappleMappingContext.LoadSynchronous();
	if (!Enhanced || !Action || !Subsystem || !Context)
	{
		UE_LOG(LogTemp, Warning, TEXT("TD Grapple: Enhanced Input assets or local player subsystem missing on %s"), *GetName());
		return;
	}
	// [GAS 각주 11] Started → InputTag → 기존 컨트롤러의 ProcessAbilityInput → GA.
	// Completed와 Canceled를 모두 전달해 입력 중단 시에도 줄이 남지 않게 합니다.
	GrappleInputBindingHandles.Add(Enhanced->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::GrappleInputPressed).GetHandle());
	GrappleInputBindingHandles.Add(Enhanced->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::GrappleInputReleased).GetHandle());
	GrappleInputBindingHandles.Add(Enhanced->BindAction(Action, ETriggerEvent::Canceled, this, &ThisClass::GrappleInputReleased).GetHandle());
	BoundGrappleInput = Enhanced;
	Subsystem->AddMappingContext(Context, 1);
	GrappleInputSubsystem = Subsystem;
	InstalledGrappleContext = Context;
}

void ATDPlayerCharacte::GrappleInputPressed()
{
	if (UCharacterAbilitySystemComponent* ASC = Cast<UCharacterAbilitySystemComponent>(GetAbilitySystemComponent()))
		ASC->AbilityInputTagPressed(TDGrappleTags::Input_Grapple);
}

void ATDPlayerCharacte::GrappleInputReleased()
{
	if (UCharacterAbilitySystemComponent* ASC = Cast<UCharacterAbilitySystemComponent>(GetAbilitySystemComponent()))
		ASC->AbilityInputTagReleased(TDGrappleTags::Input_Grapple);
}

void ATDPlayerCharacte::RemoveGrappleInputContext()
{
	if (UEnhancedInputComponent* Enhanced = BoundGrappleInput.Get())
		for (uint32 Handle : GrappleInputBindingHandles) Enhanced->RemoveBindingByHandle(Handle);
	GrappleInputBindingHandles.Reset();
	BoundGrappleInput.Reset();
	if (GrappleInputSubsystem.IsValid() && InstalledGrappleContext.IsValid())
		GrappleInputSubsystem->RemoveMappingContext(InstalledGrappleContext.Get());
	GrappleInputSubsystem.Reset();
	InstalledGrappleContext.Reset();
}

void ATDPlayerCharacte::CleanupGrappleAbility()
{
	// [GAS 각주 12] PlayerState는 캐릭터보다 오래 살아남으므로 이 Pawn이 부여한 능력만 정리합니다.
	if (UCharacterAbilitySystemComponent* ASC = GrappleASC.Get())
	{
		ASC->AbilityInputTagReleased(TDGrappleTags::Input_Grapple);
		TArray<FGameplayAbilitySpecHandle> Handles;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			if (Spec.SourceObject.Get() == this) Handles.Add(Spec.Handle);
		for (const FGameplayAbilitySpecHandle& Handle : Handles) ASC->CancelAbilityHandle(Handle);
		if (HasAuthority()) GrappleGrantedHandles.TakeFromAbilitySystem(ASC);
	}
	GrappleComponent->ReleaseGrapple();
	bGrappleAbilitiesGranted = false;
	GrappleASC.Reset();
	RemoveGrappleInputContext();
}

void ATDPlayerCharacte::UnPossessed()
{
	CleanupGrappleAbility();
	Super::UnPossessed();
}

void ATDPlayerCharacte::OnRep_Controller()
{
	Super::OnRep_Controller();
	if (!IsLocallyControlled()) CleanupGrappleAbility();
}

void ATDPlayerCharacte::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupGrappleAbility();
	Super::EndPlay(EndPlayReason);
}

