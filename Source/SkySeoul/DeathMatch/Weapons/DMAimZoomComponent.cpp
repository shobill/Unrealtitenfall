#include "DMAimZoomComponent.h"

#include "Character/SkySeoulCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

UDMAimZoomComponent::UDMAimZoomComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UDMAimZoomComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bApplyCameraZoom)
	{
		CacheCameraDefaults();
		ApplyZoom(0.0f);
	}
}

UDMAimZoomComponent* UDMAimZoomComponent::FindOnActor(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UDMAimZoomComponent>() : nullptr;
}

void UDMAimZoomComponent::CacheCameraDefaults()
{
	if (!bApplyCameraZoom)
	{
		return;
	}
	if (UCameraComponent* Camera = GetFollowCamera())
	{
		DefaultFOV = Camera->FieldOfView;
	}
	if (USpringArmComponent* Boom = GetCameraBoom())
	{
		DefaultArmLength = Boom->TargetArmLength;
	}
	bCachedDefaults = true;
}

UCameraComponent* UDMAimZoomComponent::GetFollowCamera() const
{
	if (const ASkySeoulCharacter* Character = Cast<ASkySeoulCharacter>(GetOwner()))
	{
		return Character->GetFollowCamera();
	}
	return nullptr;
}

USpringArmComponent* UDMAimZoomComponent::GetCameraBoom() const
{
	if (const ASkySeoulCharacter* Character = Cast<ASkySeoulCharacter>(GetOwner()))
	{
		return Character->GetCameraBoom();
	}
	return nullptr;
}

void UDMAimZoomComponent::SetAiming(bool bNewAiming)
{
	if (bIsAiming == bNewAiming)
	{
		return;
	}

	bIsAiming = bNewAiming;

	if (bSetPlayerActionZoomState)
	{
		if (ASkySeoulCharacter* Character = Cast<ASkySeoulCharacter>(GetOwner()))
		{
			Character->PlayerActionState = bIsAiming
				? EPlayerActionState::E_ZoomAction
				: EPlayerActionState::E_None;
		}
	}

	OnAimingChanged.Broadcast(bIsAiming);
}

void UDMAimZoomComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bApplyCameraZoom && !bCachedDefaults)
	{
		CacheCameraDefaults();
	}

	const float Target = bIsAiming ? 1.0f : 0.0f;
	AimAlpha = FMath::FInterpTo(AimAlpha, Target, DeltaTime, ZoomInterpSpeed);

	if (bApplyCameraZoom)
	{
		ApplyZoom(AimAlpha);
	}

	if (!bIsAiming && AimAlpha <= KINDA_SMALL_NUMBER)
	{
		AimAlpha = 0.0f;
		if (bApplyCameraZoom)
		{
			ApplyZoom(0.0f);
		}
	}
}

void UDMAimZoomComponent::ApplyZoom(float Alpha)
{
	if (!bApplyCameraZoom)
	{
		return;
	}

	const float Clamped = FMath::Clamp(Alpha, 0.0f, 1.0f);

	if (UCameraComponent* Camera = GetFollowCamera())
	{
		Camera->SetFieldOfView(FMath::Lerp(DefaultFOV, AimFOV, Clamped));
	}
	if (USpringArmComponent* Boom = GetCameraBoom())
	{
		Boom->TargetArmLength = FMath::Lerp(DefaultArmLength, AimArmLength, Clamped);
	}
}
