// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/A_GrenadeBase.h"
#include "PhysicsEngine/RadialForceComponent.h"


AA_GrenadeBase::AA_GrenadeBase()
{
    RadialForceComp = CreateDefaultSubobject<URadialForceComponent>(TEXT("RadialForceComp"));
    RadialForceComp->SetupAttachment(ActorMesh);
    RadialForceComp->Radius; // 폭발 반경 설정
    RadialForceComp->bImpulseVelChange = false;
    RadialForceComp->bAutoActivate = false;
    // RadialForceComp->bIgnoreOwningActor = true; // 자신에게는 힘을 가하지 않음

    ActorMesh->SetSimulatePhysics(true);
}

void AA_GrenadeBase::GetInRadiusActors(TArray<FHitResult>& HitResults)
{
    FVector StartLocation = ActorMesh->GetRelativeLocation();
    FVector EndLocation = StartLocation;
    FCollisionShape CollisionShape = FCollisionShape::MakeSphere(500.f);
    FCollisionQueryParams CollisionParams;
    CollisionParams.AddIgnoredActor(this);
    GetWorld()->SweepMultiByChannel(HitResults, StartLocation, EndLocation, FQuat::Identity, ECollisionChannel::ECC_GameTraceChannel1, CollisionShape, CollisionParams);

}
