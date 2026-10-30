#include "TitanDefence/Grapple/TDGrappleComponent.h"

#include "CableComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

UTDGrappleComponent::UTDGrappleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTDGrappleComponent::BeginPlay()
{
	Super::BeginPlay();
	//  CableComponent는 줄의 외형만 그립니다. 캐릭터를 당기는 힘은 만들지 않습니다.
	Cable = NewObject<UCableComponent>(GetOwner(), TEXT("GrappleCable"));
	Cable->SetupAttachment(GetOwner()->GetRootComponent());
	Cable->SetRelativeLocation(CableStartOffset);
	Cable->CableWidth = 6.f;
	Cable->NumSegments = 16;
	Cable->bAttachStart = true;
	Cable->bAttachEnd = true;
	Cable->bEnableCollision = false;
	Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cable->RegisterComponent();
	Cable->AddTickPrerequisiteComponent(this);
	Cable->SetVisibility(false);
	Cable->SetComponentTickEnabled(false);
}

bool UTDGrappleComponent::IsTerrain(const UPrimitiveComponent* Component) const
{
	//  정적 지형/건물만 허용.
	return IsValid(Component) && !Cast<APawn>(Component->GetOwner())
		&& Component->GetCollisionObjectType() == ECC_WorldStatic
		&& Component->Mobility == EComponentMobility::Static;
}

bool UTDGrappleComponent::FindTerrainTarget(const FVector& Start, const FVector& Direction, FHitResult& OutHit) const
{
	OutHit = FHitResult();
	if (!GetWorld() || Direction.IsNearlyZero() || MaxRange <= 0.f) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TDGrappleTrace), true, GetOwner());
	const FVector End = Start + Direction.GetSafeNormal() * MaxRange;
	FHitResult Hit;
	// [각주 7] 먼저 시야를 가로막는 물체를 검사한 다음 지형인지 판별합니다.
	// 타이탄 등 Visibility를 차단하는 물체를 관통해 뒤쪽 벽에 연결하지 않습니다.
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	const bool bAccepted = bHit && IsTerrain(Hit.GetComponent());
	if (bDrawDebugTrace)
	{
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End,
			bAccepted ? FColor::Green : FColor::Red, false, 2.f);
	}
	if (bAccepted) OutHit = Hit;
	return bAccepted;
}

bool UTDGrappleComponent::AttachToTerrain(const FHitResult& Hit)
{
	if (!IsTerrain(Hit.GetComponent()) || !Hit.bBlockingHit || bIsGrappling) return false;

	// [각주 8] 월드 좌표 대신 컴포넌트 기준 상대 좌표를 저장해 연결 위치를 명확하게 유지합니다.
	AnchorComponent = Hit.GetComponent();
	AnchorLocalPosition = Hit.GetComponent()->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
	bIsGrappling = true;
	SetComponentTickEnabled(true);
	if (Cable)
	{
		Cable->SetVisibility(true);
		Cable->SetComponentTickEnabled(true);
	}
	UpdateCable();
	return true;
}

FVector UTDGrappleComponent::GetAnchorWorldLocation() const
{
	return AnchorComponent.IsValid()
		? AnchorComponent->GetComponentTransform().TransformPosition(AnchorLocalPosition)
		: FVector::ZeroVector;
}

void UTDGrappleComponent::UpdateCable()
{
	if (!Cable || !bIsGrappling) return;
	Cable->SetRelativeLocation(CableStartOffset);
	const FVector Anchor = GetAnchorWorldLocation();
	// [각주 9] EndLocation은 케이블의 로컬 좌표입니다. 월드 연결점을 변환해서 넣어야 합니다.
	Cable->EndLocation = Cable->GetComponentTransform().InverseTransformPosition(Anchor);
	Cable->CableLength = FVector::Distance(Cable->GetComponentLocation(), Anchor);
}

void UTDGrappleComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	// [각주 10] 대상 삭제, 조종 해제, 캐릭터 제거 시 남아 있는 줄을 정리합니다.
	if (!IsTerrain(AnchorComponent.Get()) || (Pawn && !Pawn->IsLocallyControlled())
		|| (Character && Character->IsActorBeingDestroyed()))
	{
		ReleaseGrapple();
		return;
	}
	UpdateCable();
}

void UTDGrappleComponent::ReleaseGrapple()
{
	const bool bWasGrappling = bIsGrappling;
	bIsGrappling = false;
	AnchorComponent.Reset();
	AnchorLocalPosition = FVector::ZeroVector;
	SetComponentTickEnabled(false);
	if (Cable)
	{
		Cable->SetVisibility(false);
		Cable->SetComponentTickEnabled(false);
	}
	// [각주 11] 속도, 중력, 이동 모드는 변경하지 않습니다. 이번 범위는 연결과 표시까지입니다.
	// [GAS 각주 8] 외부 해제도 어빌리티에 알립니다. 중복 해제는 알리지 않습니다.
	if (bWasGrappling) OnGrappleReleased.Broadcast();
}

void UTDGrappleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseGrapple();
	if (Cable) Cable->DestroyComponent();
	Super::EndPlay(EndPlayReason);
}
