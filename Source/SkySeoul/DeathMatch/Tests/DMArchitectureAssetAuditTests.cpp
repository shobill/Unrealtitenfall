#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/LinkerLoad.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/UObjectHash.h"
#include "UObject/SoftObjectPath.h"

class FDMSoftReferenceAudit : public FArchiveUObject
{
public:
    TSet<FString> Paths;
    FDMSoftReferenceAudit() { ArIsObjectReferenceCollector = true; }
    using FArchiveUObject::operator<<;
    FArchive& operator<<(FSoftObjectPath& Path) override
    {
        if (!Path.IsNull()) Paths.Add(Path.ToString());
        return *this;
    }
    FArchive& operator<<(FSoftObjectPtr& Ptr) override
    {
        if (!Ptr.IsNull()) Paths.Add(Ptr.ToSoftObjectPath().ToString());
        return *this;
    }
};

// Registry supplies all packages depending on the native module, including outside DeathMatch.
// The engine linker resolves the actual import table; this is not a binary string search.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMArchitectureAssetAudit, "SkySeoul.DeathMatch.Architecture.AssetImports",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDMArchitectureAssetAudit::RunTest(const FString& Parameters)
{
    const FString Root = FPaths::ProjectSavedDir() / TEXT("DMArchitectureAudit");
    TArray<FString> Packages;
    if (!FFileHelper::LoadFileToStringArray(Packages, *(Root / TEXT("packages.txt"))))
    {
        AddError(TEXT("Run inspect_architecture_registry.py before the import audit."));
        return false;
    }
    TArray<FString> Imports;
    TArray<FString> Topology;
    TArray<FString> SoftReferences;
    for (const FString& Name : Packages)
    {
        UPackage* Package = LoadPackage(nullptr, *Name, LOAD_None);
        if (!TestNotNull(*Name, Package)) continue;
        FLinkerLoad* Linker = Package->GetLinker();
        if (!TestNotNull(*(Name + TEXT(" linker")), Linker)) continue;
        TArray<UObject*> Objects;
        GetObjectsWithOuter(Package, Objects, true);
        FDMSoftReferenceAudit SoftArchive;
        for (UObject* Object : Objects) Object->Serialize(SoftArchive);
        for (const FString& Path : SoftArchive.Paths) SoftReferences.Add(Name + TEXT("\t") + Path);
        for (int32 Index = 0; Index < Linker->ImportMap.Num(); ++Index)
        {
            const FString Path = Linker->GetImportPathName(Index);
            if (Path.StartsWith(TEXT("/Script/SkySeoul.")) || Name.StartsWith(TEXT("/Game/DeathMatch/")))
                Imports.Add(Name + TEXT("\t") + Path);
        }
        if (Name.StartsWith(TEXT("/Game/DeathMatch/AI/Controller/")))
        {
            UBlueprint* BP = FindObject<UBlueprint>(Package, *FPaths::GetBaseFilename(Name));
            if (BP && BP->SimpleConstructionScript)
            {
                Topology.Add(Name + TEXT(" parent=") + GetPathNameSafe(BP->ParentClass));
                for (const USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
                    Topology.Add(FString::Printf(TEXT("  SCS %s class=%s guid=%s"),
                        *Node->GetVariableName().ToString(), *GetPathNameSafe(Node->ComponentClass), *Node->VariableGuid.ToString()));
            }
        }
        if (Name == TEXT("/Game/DeathMatch/AI/BT/BT_Combat"))
        {
            const UBehaviorTree* Tree = FindObject<UBehaviorTree>(Package, TEXT("BT_Combat"));
            TFunction<void(const UBTCompositeNode*, int32)> Visit = [&](const UBTCompositeNode* Node, int32 Depth)
            {
                if (!Node) return;
                Topology.Add(FString::ChrN(Depth, ' ') + Node->GetClass()->GetName());
                for (const FBTCompositeChild& Child : Node->Children)
                {
                    if (Child.ChildTask) Topology.Add(FString::ChrN(Depth + 1, ' ') + Child.ChildTask->GetClass()->GetName());
                    Visit(Child.ChildComposite, Depth + 1);
                }
            };
            if (Tree) Visit(Tree->RootNode, 0);
        }
    }
    TestTrue(TEXT("Write resolved package imports"), FFileHelper::SaveStringArrayToFile(Imports, *(Root / TEXT("imports.tsv"))));
    TestTrue(TEXT("Write BP and BT topology"), FFileHelper::SaveStringArrayToFile(Topology, *(Root / TEXT("topology.txt"))));
    TestTrue(TEXT("Write serialized soft references"), FFileHelper::SaveStringArrayToFile(SoftReferences, *(Root / TEXT("soft-references.tsv"))));
    AddInfo(FString::Printf(TEXT("Audited %d packages; saved %d imports"), Packages.Num(), Imports.Num()));
    return true;
}
#endif
