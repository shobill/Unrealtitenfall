#include "DMMovePoint.h"
#include "Components/BillboardComponent.h"

ADMMovePoint::ADMMovePoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	EditorSprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("EditorSprite"));
	EditorSprite->SetupAttachment(Root);
	EditorSprite->bIsScreenSizeScaled = true;
#endif
}
