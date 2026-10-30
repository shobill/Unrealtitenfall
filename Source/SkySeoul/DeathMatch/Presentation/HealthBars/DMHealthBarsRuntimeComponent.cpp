#include "DMHealthBarsRuntimeComponent.h"

#include "DeathMatch/Presentation/HealthBars/DMHealthBarWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

FVector DMHealthBars::ResolveAnchorLocation(const AActor* Target, FName HealthBarSocketName, float HeightOffset)
{
	if (!IsValid(Target))
	{
		return FVector::ZeroVector;
	}

	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		if (const USkeletalMeshComponent* Mesh = Character->GetMesh(); Mesh && !HealthBarSocketName.IsNone() && Mesh->DoesSocketExist(HealthBarSocketName))
		{
			return Mesh->GetSocketLocation(HealthBarSocketName) + Target->GetActorUpVector() * HeightOffset;
		}

		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			return Target->GetActorLocation() + Target->GetActorUpVector() * (Capsule->GetScaledCapsuleHalfHeight() + HeightOffset);
		}
	}

	FVector BoundsOrigin = Target->GetActorLocation();
	FVector BoundsExtent = FVector::ZeroVector;
	Target->GetActorBounds(false, BoundsOrigin, BoundsExtent);
	return BoundsOrigin + FVector::UpVector * (BoundsExtent.Z + HeightOffset);
}

FVector DMHealthBars::ResolveLineOfSightLocation(const AActor* Target)
{
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const float ChestHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() * 0.5f : 45.0f;
		return Target->GetActorLocation() + Target->GetActorUpVector() * ChestHeight;
	}
	return IsValid(Target) ? Target->GetActorLocation() : FVector::ZeroVector;
}

UDMHealthBarsRuntimeComponent::UDMHealthBarsRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);
	WidgetClass = UDMHealthBarWidget::StaticClass();
}

void UDMHealthBarsRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();
	SetIsReplicated(false);
}

void UDMHealthBarsRuntimeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FDMHealthBarTargetEntry& Entry : TargetEntries)
	{
		if (Entry.WidgetComponent)
		{
			Entry.WidgetComponent->DestroyComponent();
		}
	}
	for (UWidgetComponent* Component : WidgetPool)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	TargetEntries.Reset();
	WidgetPool.Reset();
	UpdateDiagnosticCounts();
	Super::EndPlay(EndPlayReason);
}

void UDMHealthBarsRuntimeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RemoveInvalidEntries();
	for (FDMHealthBarTargetEntry& Entry : TargetEntries)
	{
		if (Entry.bCurrentlyDisplayed)
		{
			UpdateWidgetLocation(Entry);
		}
	}
}

void UDMHealthBarsRuntimeComponent::BeginDisplayPass()
{
	++DisplayPassSerial;
	if (DisplayPassSerial == 0)
	{
		DisplayPassSerial = 1;
		for (FDMHealthBarTargetEntry& Entry : TargetEntries)
		{
			Entry.LastDisplayPass = 0;
		}
	}
}

void UDMHealthBarsRuntimeComponent::ShowTarget(AActor* Target, const FDMHealthSnapshot& Snapshot, float Now, bool bRefreshLastVisibleTime)
{
	if (!IsValid(Target) || !Snapshot.bValid)
	{
		return;
	}

	FDMHealthBarTargetEntry* Entry = FindTargetEntry(Target);
	if (!Entry)
	{
		Entry = &TargetEntries.AddDefaulted_GetRef();
		Entry->TargetActor = Target;
	}

	Entry->LastDisplayPass = DisplayPassSerial;
	if (bRefreshLastVisibleTime)
	{
		Entry->LastVisibleTime = Now;
	}
	if (!Entry->WidgetComponent)
	{
		Entry->WidgetComponent = AcquireWidgetComponent();
	}

	const bool bNeedsRefresh = !Entry->bCurrentlyDisplayed || !Entry->LastHealthSnapshot.IsNearlyEqual(Snapshot);
	Entry->bCurrentlyDisplayed = Entry->WidgetComponent != nullptr;
	if (Entry->WidgetComponent)
	{
		Entry->WidgetComponent->SetVisibility(true);
		UpdateWidgetLocation(*Entry);
		if (bNeedsRefresh)
		{
			RefreshWidget(*Entry, Target, Snapshot);
		}
	}
	Entry->LastHealthSnapshot = Snapshot;
	UpdateDiagnosticCounts();
}

void UDMHealthBarsRuntimeComponent::ShowTargetUsingCachedSnapshot(AActor* Target, float Now, bool bRefreshLastVisibleTime)
{
	if (FDMHealthBarTargetEntry* Entry = FindTargetEntry(Target); Entry && Entry->LastHealthSnapshot.bValid)
	{
		ShowTarget(Target, Entry->LastHealthSnapshot, Now, bRefreshLastVisibleTime);
	}
}

void UDMHealthBarsRuntimeComponent::EndDisplayPass(float Now, float EntryRetentionSeconds)
{
	for (int32 Index = TargetEntries.Num() - 1; Index >= 0; --Index)
	{
		FDMHealthBarTargetEntry& Entry = TargetEntries[Index];
		if (Entry.LastDisplayPass != DisplayPassSerial)
		{
			ReleaseWidgetComponent(Entry);
		}

		const bool bExpired = !Entry.TargetActor.IsValid()
			|| (!Entry.bCurrentlyDisplayed && Now - Entry.LastVisibleTime > FMath::Max(0.0f, EntryRetentionSeconds));
		if (bExpired)
		{
			TargetEntries.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
	UpdateDiagnosticCounts();
}

void UDMHealthBarsRuntimeComponent::HideAllTargets()
{
	for (FDMHealthBarTargetEntry& Entry : TargetEntries)
	{
		ReleaseWidgetComponent(Entry);
	}
	TargetEntries.Reset();
	UpdateDiagnosticCounts();
}

void UDMHealthBarsRuntimeComponent::RemoveInvalidEntries()
{
	for (int32 Index = TargetEntries.Num() - 1; Index >= 0; --Index)
	{
		if (!TargetEntries[Index].TargetActor.IsValid())
		{
			ReleaseWidgetComponent(TargetEntries[Index]);
			TargetEntries.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
	UpdateDiagnosticCounts();
}

FDMHealthBarTargetEntry* UDMHealthBarsRuntimeComponent::FindTargetEntry(const AActor* Target)
{
	return TargetEntries.FindByPredicate([Target](const FDMHealthBarTargetEntry& Entry)
	{
		return Entry.TargetActor.Get() == Target;
	});
}

const FDMHealthBarTargetEntry* UDMHealthBarsRuntimeComponent::FindTargetEntry(const AActor* Target) const
{
	return TargetEntries.FindByPredicate([Target](const FDMHealthBarTargetEntry& Entry)
	{
		return Entry.TargetActor.Get() == Target;
	});
}

bool UDMHealthBarsRuntimeComponent::IsHealthRefreshDue(const AActor* Target, float Now) const
{
	const FDMHealthBarTargetEntry* Entry = FindTargetEntry(Target);
	return !Entry || !Entry->LastHealthSnapshot.bValid || Now >= Entry->NextHealthUpdateTime;
}

void UDMHealthBarsRuntimeComponent::SetNextHealthRefresh(AActor* Target, float NextUpdateTime)
{
	if (FDMHealthBarTargetEntry* Entry = FindTargetEntry(Target))
	{
		Entry->NextHealthUpdateTime = NextUpdateTime;
	}
}

APlayerController* UDMHealthBarsRuntimeComponent::ResolveLocalViewerController() const
{
	if (APlayerController* PlayerController = Cast<APlayerController>(GetOwner()); PlayerController && PlayerController->IsLocalController())
	{
		return PlayerController;
	}
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController()); PlayerController && PlayerController->IsLocalController())
		{
			return PlayerController;
		}
	}
	return nullptr;
}

UWidgetComponent* UDMHealthBarsRuntimeComponent::AcquireWidgetComponent()
{
	UWidgetComponent* Component = nullptr;
	while (WidgetPool.Num() > 0 && !Component)
	{
		Component = WidgetPool.Pop(EAllowShrinking::No);
	}
	if (!Component && GetOwner())
	{
		Component = NewObject<UWidgetComponent>(GetOwner());
		GetOwner()->AddInstanceComponent(Component);
		Component->SetIsReplicated(false);
		Component->SetWidgetSpace(EWidgetSpace::Screen);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->RegisterComponent();
	}

	if (Component)
	{
		Component->SetWidgetClass(WidgetClass ? WidgetClass.Get() : UDMHealthBarWidget::StaticClass());
		Component->SetDrawSize(DrawSize);
		Component->SetPivot(Pivot);
		Component->SetOwnerPlayer(ResolveLocalViewerController() ? ResolveLocalViewerController()->GetLocalPlayer() : nullptr);
		Component->InitWidget();
		Component->SetVisibility(true);
	}
	UpdateDiagnosticCounts();
	return Component;
}

void UDMHealthBarsRuntimeComponent::ReleaseWidgetComponent(FDMHealthBarTargetEntry& Entry)
{
	if (Entry.WidgetComponent)
	{
		Entry.WidgetComponent->SetVisibility(false);
		WidgetPool.AddUnique(Entry.WidgetComponent);
		Entry.WidgetComponent = nullptr;
	}
	Entry.bCurrentlyDisplayed = false;
}

void UDMHealthBarsRuntimeComponent::RefreshWidget(FDMHealthBarTargetEntry& Entry, AActor* Target, const FDMHealthSnapshot& Snapshot)
{
	if (!Entry.WidgetComponent)
	{
		return;
	}
	if (UDMHealthBarWidget* Widget = Cast<UDMHealthBarWidget>(Entry.WidgetComponent->GetUserWidgetObject()))
	{
		Widget->SetHealthDisplay(Target, Snapshot, DisplayOptions);
	}
}

void UDMHealthBarsRuntimeComponent::UpdateWidgetLocation(FDMHealthBarTargetEntry& Entry) const
{
	if (Entry.WidgetComponent && Entry.TargetActor.IsValid())
	{
		Entry.WidgetComponent->SetWorldLocation(DMHealthBars::ResolveAnchorLocation(Entry.TargetActor.Get(), HealthBarSocketName, HeightOffset));
	}
}

void UDMHealthBarsRuntimeComponent::UpdateDiagnosticCounts()
{
	ActiveWidgetCount = 0;
	for (const FDMHealthBarTargetEntry& Entry : TargetEntries)
	{
		ActiveWidgetCount += Entry.bCurrentlyDisplayed && Entry.WidgetComponent ? 1 : 0;
	}
	PooledWidgetCount = WidgetPool.Num();
}

