#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TDGrappleComponent.generated.h"

class UCableComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTDGrappleReleased);

/** 조준 검사 → 지형 연결점 저장 → 줄 표시까지만 담당합니다. 이동 기능은 없습니다. */
UCLASS(ClassGroup = (TitanDefense), meta = (BlueprintSpawnableComponent))
class SKYSEOUL_API UTDGrappleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTDGrappleComponent();

	UFUNCTION(BlueprintCallable, Category = "Grapple")
	void ReleaseGrapple();

	// [GAS 각주 7] 입력과 능력 발동은 GA가 담당합니다. 이 함수들은 검사/표시 전용입니다.
	bool FindTerrainTarget(const FVector& Start, const FVector& Direction, FHitResult& OutHit) const;
	bool AttachToTerrain(const FHitResult& Hit);

	UPROPERTY(BlueprintAssignable, Category = "Grapple")
	FTDGrappleReleased OnGrappleReleased;

	UFUNCTION(BlueprintPure, Category = "Grapple")
	bool IsGrappling() const { return bIsGrappling; }

	UFUNCTION(BlueprintPure, Category = "Grapple")
	FVector GetAnchorWorldLocation() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple", meta = (ClampMin = "1", Units = "cm"))
	float MaxRange = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple")
	bool bDrawDebugTrace = false;

	// 캐릭터 기준 줄 시작 위치입니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grapple")
	FVector CableStartOffset = FVector(25.f, 20.f, 45.f);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Grapple")
	TObjectPtr<UCableComponent> Cable;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool IsTerrain(const UPrimitiveComponent* Component) const;
	void UpdateCable();
	TWeakObjectPtr<UPrimitiveComponent> AnchorComponent;
	FVector AnchorLocalPosition = FVector::ZeroVector;
	bool bIsGrappling = false;
};
