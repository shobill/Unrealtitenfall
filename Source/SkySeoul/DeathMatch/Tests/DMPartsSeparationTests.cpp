#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "DeathMatch/Character/DeathMatchRobotCharacter.h"
#include "DeathMatch/Character/DMAimTargetComponent.h"
#include "PartsCharacter/PartsComponent.h"
#include "PartsCharacter/PartsSetupData.h"
#include "PartsCharacter/PartsDamageManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "HAL/IConsoleManager.h"

namespace
{
    UWorld* PartsTestWorld()
    {
        UWorld::InitializationValues IV;
        IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        return UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);
    }
    TArray<FPartsDefinition>& Definitions(UPartsComponent* P)
    {
        return *FindFProperty<FArrayProperty>(UPartsComponent::StaticClass(),TEXT("ActiveDefinitions"))->ContainerPtrToValuePtr<TArray<FPartsDefinition>>(P);
    }
    TMap<FGameplayTag,FPartsRuntimeState>& Runtime(UPartsComponent* P)
    {
        return *FindFProperty<FMapProperty>(UPartsComponent::StaticClass(),TEXT("PartsRuntime"))->ContainerPtrToValuePtr<TMap<FGameplayTag,FPartsRuntimeState>>(P);
    }
    int32 DebrisCount(UWorld* World)
    {
        int32 N = 0;
        for (TActorIterator<AActor> It(World); It; ++It)
            if (It->FindComponentByClass<UStaticMeshComponent>() && It->GetRootComponent()->GetFName() == TEXT("DebrisPhys")) ++N;
        return N;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMPartsAssetAuditTest,"SkySeoul.DeathMatch.Combat.Parts.AssetAudit",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMPartsAssetAuditTest::RunTest(const FString&)
{
    auto* Diag = IConsoleManager::Get().FindConsoleVariable(TEXT("dm.PartsDiag"));
    const int32 Previous = Diag->GetInt(); Diag->Set(1,ECVF_SetByConsole);
    UClass* Class = LoadClass<ADeathMatchRobotCharacter>(nullptr,TEXT("/Game/DeathMatch/AI/Character/BP_DeathMatchAIRobot.BP_DeathMatchAIRobot_C"));
    if (!TestNotNull(TEXT("Robot BP"), Class)) { Diag->Set(Previous,ECVF_SetByConsole); return false; }
    UWorld* World = PartsTestWorld();
    for (bool Death : {false,true})
    {
        auto* Pawn = World->SpawnActor<ADeathMatchRobotCharacter>(Class);
        if (const auto* Aim = Pawn->FindComponentByClass<UDMAimTargetComponent>())
            for (const auto* Sensor : {Aim->GetHeadSensor(), Aim->GetBodySensor()})
                TestTrue(TEXT("BP sensor attaches to runtime parent"),Sensor && Sensor->GetAttachParent() && !Sensor->GetAttachParent()->IsTemplate());
        auto* P = Pawn->FindComponentByClass<UPartsComponent>();
        if (!TestNotNull(TEXT("Spawned component"),P)) continue;
        // Match APartsCharacterBase::BeginPlay's authoritative asset assignment without
        // starting unrelated UI, inventory or multiplayer systems in this isolated world.
        auto* Property = FindFProperty<FObjectPropertyBase>(Pawn->GetClass(),TEXT("PartsSetupData"));
        if (auto* Data = Cast<UPartsSetupData>(Property->GetObjectPropertyValue_InContainer(Pawn))) P->SetupData = Data;
        // Stored legacy values cannot opt into a second trace owner.
        P->bAttachedPartsBlockTraces = true;
        P->bConfigureOwnerMeshForWeaponTrace = false;
        P->InitializeParts();
        TArray<UPrimitiveComponent*> Visuals; P->CollectAttachedPartComponents(Visuals);
        TestTrue(TEXT("Authored fixture creates attached visuals"), !Visuals.IsEmpty());
        for (const auto* Visual : Visuals)
            TestEqual(TEXT("Legacy true cannot enable attached collision"), Visual->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        TestEqual(TEXT("Actual setup uses authored DA"),GetPathNameSafe(P->SetupData),FString(TEXT("/Game/DeathMatch/AI/Character/DA_Parts_DM_Robot.DA_Parts_DM_Robot")));
        const auto Head = FGameplayTag::RequestGameplayTag(TEXT("Parts.Head"));
        int32 HeadCount=0;
        for (const auto& D : Definitions(P)) if (D.PartTag == Head)
        {
            ++HeadCount;
            TestFalse(TEXT("Authored head forbids separation"),D.bSeparable);
            TestTrue(TEXT("Head impairment retained"),D.bImpairHeadOnDestroy);
            AddInfo(FString::Printf(TEXT("ASSET_RUNTIME Setup=%s Separable=%d Mesh=%s HP=%.1f Impair=%d Cause=%s"),
                *GetPathNameSafe(P->SetupData),D.bSeparable,*D.PartMesh.ToSoftObjectPath().ToString(),D.MaxHP,D.bImpairHeadOnDestroy,Death?TEXT("Death"):TEXT("Damage")));
        }
        TestEqual(TEXT("Single authoritative head row"),HeadCount,1);
        // Definition resolution happens even if attachment is deferred. Populate only missing
        // HP runtime slots so the actual native destruction gate can be audited headlessly.
        for (const auto& D : Definitions(P))
            if (!Runtime(P).Contains(D.PartTag)) { FPartsRuntimeState S; S.CurrentHP=D.MaxHP; Runtime(P).Add(D.PartTag,S); }
        const int32 Before = DebrisCount(World);
        if (Death) P->HandleOwnerDeathPartsExplosion(); else P->ApplyPartDamage(Head,10000);
        TestEqual(TEXT("Authored nonseparable configuration spawns no debris"),DebrisCount(World)-Before,0);
        AddInfo(FString::Printf(TEXT("ASSET_RESULT Cause=%s Destroyed=%d DebrisDelta=%d HeadIntact=%d"),
            Death?TEXT("Death"):TEXT("Damage"),P->IsPartDestroyed(Head),DebrisCount(World)-Before,P->IsHeadIntact()));
        Pawn->Destroy();
    }
    World->DestroyWorld(false); Diag->Set(Previous,ECVF_SetByConsole);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMPartsSeparationTest,"SkySeoul.DeathMatch.Combat.Parts.Separation",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMPartsSeparationTest::RunTest(const FString&)
{
    UWorld* World = PartsTestWorld();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Mesh fixture"),Cube)) { World->DestroyWorld(false); return false; }
    for (const TCHAR* TagName : {TEXT("Parts.Head"),TEXT("Parts.Arm.L.Upper")})
        for (bool Separable : {false,true})
            for (bool Death : {false,true})
            {
                AActor* Owner = World->SpawnActor<AActor>();
                auto* Root = NewObject<USceneComponent>(Owner); Owner->SetRootComponent(Root); Root->RegisterComponent();
                auto* P = NewObject<UPartsComponent>(Owner); Owner->AddInstanceComponent(P); P->RegisterComponent();
                P->bKillOwnerWhenCannotShoot = false;
                FPartsDefinition D; D.PartTag = FGameplayTag::RequestGameplayTag(TagName); D.bSeparable = Separable;
                D.bImpairHeadOnDestroy = D.PartTag.GetTagName() == TEXT("Parts.Head");
                Definitions(P).Add(D);
                auto* Mesh = NewObject<UStaticMeshComponent>(Owner); Owner->AddInstanceComponent(Mesh);
                Mesh->SetStaticMesh(Cube); Mesh->SetupAttachment(Root); Mesh->RegisterComponent();
                FPartsRuntimeState S; S.CurrentHP = D.MaxHP; S.MeshComponent = Mesh; Runtime(P).Add(D.PartTag,S);
                const int32 Before = DebrisCount(World);
                if (Death) P->HandleOwnerDeathPartsExplosion(); else TestTrue(TEXT("Damage applied"),P->ApplyPartDamage(D.PartTag,1000));
                TestEqual(TEXT("Debris path follows flag in both causes for head and arm"),DebrisCount(World)-Before,Separable?1:0);
                TestEqual(TEXT("Functional destruction preserved; nonseparable death skips"),P->IsPartDestroyed(D.PartTag),!Death||Separable);
                if (!Separable)
                {
                    TestEqual(TEXT("Attached mesh retained"),Mesh->GetAttachParent(),Root);
                    TestFalse(TEXT("No physics on retained mesh"),Mesh->IsSimulatingPhysics());
                    TestTrue(TEXT("Nonseparable mesh remains visible"), Mesh->IsVisible());
                    if (!Death)
                    {
                        TestEqual(TEXT("Destroyed retained mesh cannot collide"), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
                        TestFalse(TEXT("Repeat damage ignored"), P->ApplyPartDamage(D.PartTag, 1000));
                        TestEqual(TEXT("No repeat debris"), DebrisCount(World)-Before, 0);
                    }
                }
                if (D.bImpairHeadOnDestroy && !Death) TestFalse(TEXT("Head impairment independent of separation"),P->IsHeadIntact());
                Owner->Destroy();
            }
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMPartsVisualPolicyTest,"SkySeoul.DeathMatch.Combat.Parts.VisualPolicies",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMPartsVisualPolicyTest::RunTest(const FString&)
{
    UWorld* World = PartsTestWorld();
    for (auto Policy : {EPartsDestroyedVisualPolicy::DetachAsDebris, EPartsDestroyedVisualPolicy::HideInPlace, EPartsDestroyedVisualPolicy::KeepAttachedDisabled})
    {
        AActor* Owner = World->SpawnActor<AActor>();
        auto* Root = NewObject<USceneComponent>(Owner); Owner->SetRootComponent(Root); Root->RegisterComponent();
        auto* P = NewObject<UPartsComponent>(Owner); Owner->AddInstanceComponent(P); P->RegisterComponent();
        P->bKillOwnerWhenCannotShoot = false;
        auto* Damage = NewObject<UPartsDamageManager>(Owner); Owner->AddInstanceComponent(Damage); Damage->RegisterComponent();
        FPartsDefinition D; D.PartTag = FGameplayTag::RequestGameplayTag(TEXT("Parts.Head"));
        D.DestroyedVisualPolicy = Policy; D.bImpairHeadOnDestroy = true;
        Definitions(P).Add(D);
        auto* Mesh = NewObject<UStaticMeshComponent>(Owner); Owner->AddInstanceComponent(Mesh);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetupAttachment(Root); Mesh->RegisterComponent();
        FPartsRuntimeState S; S.CurrentHP=D.MaxHP; S.MeshComponent=Mesh; S.bFollowBoneAnim=true;
        Runtime(P).Add(D.PartTag,S);
        const int32 Before = DebrisCount(World);
        P->ApplyPartDamage(D.PartTag,1000);
        const bool Keep = Policy == EPartsDestroyedVisualPolicy::KeepAttachedDisabled;
        TestEqual(TEXT("Only debris policy spawns physics"),DebrisCount(World)-Before,Policy == EPartsDestroyedVisualPolicy::DetachAsDebris ? 1 : 0);
        TestEqual(TEXT("Only kept geometry remains visible"),Mesh->IsVisible(),Keep);
        TestEqual(TEXT("Keep retains animation following"),Runtime(P)[D.PartTag].bFollowBoneAnim,Keep);
        TestFalse(TEXT("Head function destroyed for every visual policy"),P->IsHeadIntact());
        const auto Repeat = Damage->ApplyDamageToPart(D.PartTag,100,nullptr,nullptr);
        TestFalse(TEXT("Direct destroyed-tag damage is ignored"),Repeat.bApplied);
        TestEqual(TEXT("Direct tag call without owner mesh hit cannot apply body damage"),Repeat.BodyDamageApplied,0.f);
        Owner->Destroy();
    }
    World->DestroyWorld(false);
    return true;
}
#endif
