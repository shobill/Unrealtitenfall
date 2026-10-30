#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "DMHitRoutingTestTypes.h"
#include "Misc/AutomationTest.h"
#include "DeathMatch/Character/DeathMatchRobotCharacter.h"
#include "PartsCharacter/PartsDamageManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AIController.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMHitRoutingTest, "SkySeoul.DeathMatch.Combat.Parts.HitRouting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMHitRoutingTest::RunTest(const FString&)
{
    UWorld::InitializationValues IV;
    IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IV);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Victim = World->SpawnActor<ADeathMatchRobotCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    auto* Shooter = World->SpawnActor<ADeathMatchRobotCharacter>(FVector(500, 0, 0), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Victim"), Victim) || !TestNotNull(TEXT("Shooter"), Shooter))
    { World->DestroyWorld(false); return false; }
    Victim->CurHP = Victim->MaxHP = 1000.f; Victim->IsDead = false;
    auto* Parts = Victim->GetPartsComponent();
    auto* Manager = Victim->GetPartsDamageManager();
    Parts->bKillOwnerWhenCannotShoot = false;
    auto* Fire = NewObject<UDMHitRoutingTestAbility>();
    TestEqual(TEXT("Shared default trace channel"), Fire->GetTraceChannel(), Parts->WeaponTraceChannel.GetValue());

    // Synthetic hit results exercise production routing without touching PhAT body instances.
    FHitResult Hit(Victim, Victim->GetMesh(), FVector::ZeroVector, FVector::UpVector);
    auto Body = Manager->ApplyHitDamage(Hit, 20, nullptr, Shooter);
    TestEqual(TEXT("No bone maps to body"), Body.BodyDamageApplied, 20.f);
    TestFalse(TEXT("No fabricated closest part"), Body.bHitPart);
    Hit.BoneName = TEXT("unmapped_torso");
    Body = Manager->ApplyHitDamage(Hit, 20, nullptr, Shooter);
    TestEqual(TEXT("Unmapped torso uses no-part scale"), Body.BodyDamageApplied, 20.f);

    auto* NoPartScale = FindFProperty<FFloatProperty>(UPartsDamageManager::StaticClass(), TEXT("BodyDamageOnNoPartScale"));
    NoPartScale->SetPropertyValue_InContainer(Manager, .25f);
    TestEqual(TEXT("Authored no-part scale is preserved"), Manager->ApplyHitDamage(Hit, 20, nullptr, Shooter).BodyDamageApplied, 5.f);
    NoPartScale->SetPropertyValue_InContainer(Manager, 1.f);

    const auto Tag = FGameplayTag::RequestGameplayTag(TEXT("Parts.Head"));
    auto& Runtime = *FindFProperty<FMapProperty>(UPartsComponent::StaticClass(), TEXT("PartsRuntime"))->ContainerPtrToValuePtr<TMap<FGameplayTag, FPartsRuntimeState>>(Parts);
    auto& Definitions = *FindFProperty<FArrayProperty>(UPartsComponent::StaticClass(), TEXT("ActiveDefinitions"))->ContainerPtrToValuePtr<TArray<FPartsDefinition>>(Parts);
    auto& BoneMap = *FindFProperty<FMapProperty>(UPartsComponent::StaticClass(), TEXT("BoneToPartMap"))->ContainerPtrToValuePtr<TMap<FName, FGameplayTag>>(Parts);
    FPartsDefinition Def; Def.PartTag = Tag; Def.bSeparable = false; Def.MaxHP = 30.f;
    Definitions.Add(Def);
    FPartsRuntimeState State; State.CurrentHP = 30.f; Runtime.Add(Tag, State);
    BoneMap.Add(TEXT("test_part"), Tag); Hit.BoneName = TEXT("test_part");
    Parts->OnPartDestroyed.AddDynamic(Fire, &UDMHitRoutingTestAbility::ObserveDestroyed);
    FPartsHitDamageResult Published; int32 AppliedEvents = 0;
    Manager->OnAppliedDamage.AddLambda([&](const FPartsHitDamageResult& R, AController*, AActor*, const FVector&, FName)
    { Published = R; ++AppliedEvents; });
    auto Part = Manager->ApplyHitDamage(Hit, 20, nullptr, Shooter);
    TestEqual(TEXT("Live part HP loss"), Part.PartDamageApplied, 20.f);
    TestEqual(TEXT("Live part body scale"), Part.BodyDamageApplied, 10.f);
    Manager->ApplyHitDamage(Hit, 20, nullptr, Shooter);
    TestEqual(TEXT("One destruction"), Fire->DestroyedEvents, 1);
    const float BeforeRepeat = Victim->CurHP;
    auto Repeat = Manager->ApplyHitDamage(Hit, 20, nullptr, Shooter);
    TestTrue(TEXT("Retained PhAT applies damage"), Repeat.bApplied);
    TestEqual(TEXT("Destroyed part receives no HP damage"), Repeat.PartDamageApplied, 0.f);
    TestEqual(TEXT("Retained PhAT uses part body scale"), Repeat.BodyDamageApplied, 10.f);
    TestEqual(TEXT("Actual CurHP delta"), Repeat.BodyDamageApplied, BeforeRepeat - Victim->CurHP);
    TestEqual(TEXT("Destroyed HP stays zero"), Parts->GetPartHP(Tag), 0.f);
    TestEqual(TEXT("No duplicate destruction"), Fire->DestroyedEvents, 1);
    TestEqual(TEXT("One applied event per routed hit"), AppliedEvents, 3);
    TestEqual(TEXT("Awareness payload only contains actual body damage on repeat"), Published.BodyDamageApplied + Published.PartDamageApplied, Repeat.BodyDamageApplied);
    TestFalse(TEXT("Direct destroyed-tag API has no new body route"), Manager->ApplyDamageToPart(Tag, 20, nullptr, Shooter).bApplied);

    auto* Scale = FindFProperty<FFloatProperty>(UPartsDamageManager::StaticClass(), TEXT("BodyDamageOnPartHitScale"));
    Scale->SetPropertyValue_InContainer(Manager, 0.f);
    TestFalse(TEXT("Repeat with zero scale is not applied"), Manager->ApplyHitDamage(Hit, 20, nullptr, Shooter).bApplied);
    Scale->SetPropertyValue_InContainer(Manager, .5f);
    FGameplayTag OutTag;
    TestEqual(TEXT("Zero requested damage"), Fire->ApplyFireDamage(Hit, Shooter, 0, OutTag), 0.f);
    auto* ShooterAI = World->SpawnActor<AAIController>(); auto* VictimAI = World->SpawnActor<AAIController>();
    ShooterAI->SetGenericTeamId(FGenericTeamId(1)); VictimAI->SetGenericTeamId(FGenericTeamId(1));
    ShooterAI->Possess(Shooter); VictimAI->Possess(Victim);
    const float BeforeFriendly = Victim->CurHP;
    TestEqual(TEXT("Friendly hit blocked"), Fire->ApplyFireDamage(Hit, Shooter, 20, OutTag), 0.f);
    TestEqual(TEXT("Friendly preserves HP"), Victim->CurHP, BeforeFriendly);
    VictimAI->SetGenericTeamId(FGenericTeamId(2));

    auto* Diag = IConsoleManager::Get().FindConsoleVariable(TEXT("dm.CombatDiag"));
    const int32 PreviousDiag = Diag ? Diag->GetInt() : 0;
    float DamageByDiag[2] = {};
    for (int32 Enabled = 0; Enabled < 2; ++Enabled)
    {
        if (Diag) Diag->Set(Enabled, ECVF_SetByConsole);
        DamageByDiag[Enabled] = Fire->ApplyFireDamage(Hit, Shooter, 20, OutTag);
    }
    if (Diag) Diag->Set(PreviousDiag, ECVF_SetByConsole);
    TestEqual(TEXT("Diagnostics preserve applied damage"), DamageByDiag[0], DamageByDiag[1]);
    TestEqual(TEXT("Repeat fire total is body delta"), DamageByDiag[0], 10.f);

    auto* Debris = World->SpawnActor<AActor>(); Debris->SetOwner(Victim);
    Debris->Tags.Add(PartsHitCollision::DetachedDebrisTag);
    auto* DebrisMesh = NewObject<UStaticMeshComponent>(Debris);
    FHitResult DebrisHit(Debris, DebrisMesh, FVector::ZeroVector, FVector::UpVector);
    DebrisHit.BoneName = Hit.BoneName;
    const float BeforeDebris = Victim->CurHP;
    TestEqual(TEXT("Debris fire hit rejected"), Fire->ApplyFireDamage(DebrisHit, Shooter, 20, OutTag), 0.f);
    TestFalse(TEXT("Foreign debris cannot route through owner manager"), Manager->ApplyHitDamage(DebrisHit, 20, nullptr, Shooter).bApplied);
    TestEqual(TEXT("Debris preserves original HP"), Victim->CurHP, BeforeDebris);
    Victim->CurHP = 3.f;
    const auto Lethal = Manager->ApplyHitDamage(Hit, 20, nullptr, Shooter);
    TestEqual(TEXT("Overkill reports only actual HP loss"), Lethal.BodyDamageApplied, 3.f);
    TestFalse(TEXT("Dead target damage rejected"), Manager->ApplyHitDamage(Hit, 20, nullptr, Shooter).bApplied);
    Manager->OnAppliedDamage.Clear();
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMHitCollisionPolicyTest, "SkySeoul.DeathMatch.Combat.Parts.HitCollisionPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMHitCollisionPolicyTest::RunTest(const FString&)
{
    UWorld::InitializationValues IV;
    IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IV);
    auto* Pawn = World->SpawnActor<ADeathMatchRobotCharacter>();
    auto* Parts = NewObject<UDMHitRoutingTestParts>(Pawn);
    for (bool Legacy : {false, true})
        for (auto Mode : {ECollisionEnabled::NoCollision, ECollisionEnabled::PhysicsOnly, ECollisionEnabled::QueryOnly})
        {
            Parts->bConfigureOwnerMeshForWeaponTrace = Legacy;
            Parts->bAttachedPartsBlockTraces = Legacy;
            Pawn->GetMesh()->SetCollisionEnabled(Mode);
            Pawn->GetMesh()->SetCollisionResponseToChannel(Parts->WeaponTraceChannel, ECR_Ignore);
            const auto PawnResponse = Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);
            Parts->ConfigureOwnerMeshForWeaponTrace();
            TestTrue(TEXT("Query restored independently of legacy flags"), Pawn->GetMesh()->IsQueryCollisionEnabled());
            TestEqual(TEXT("PhAT component blocks"), Pawn->GetMesh()->GetCollisionResponseToChannel(Parts->WeaponTraceChannel), ECR_Block);
            TestEqual(TEXT("Capsule ignores weapon channel"), Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(Parts->WeaponTraceChannel), ECR_Ignore);
            TestEqual(TEXT("Capsule Pawn response preserved"), Pawn->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), PawnResponse);
        }
    World->DestroyWorld(false);
    return true;
}
#endif
