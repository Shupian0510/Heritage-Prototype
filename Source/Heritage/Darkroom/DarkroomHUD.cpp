#include "DarkroomHUD.h"
#include "DarkroomCharacter.h"
#include "DarkroomDirector.h"
#include "DarkroomFlowSubsystem.h"
#include "DarkroomWidgets.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"

void ADarkroomHUD::BeginPlay()
{
    Super::BeginPlay();
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (Viewport && FApp::CanEverRender() && FSlateApplication::IsInitialized())
    {
        Overlay = SNew(SDarkroomOverlay).Hud(this);
        Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 5);
    }
}

void ADarkroomHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Overlay.IsValid())
    {
        if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr) { Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef()); }
        Overlay.Reset();
    }
    Super::EndPlay(Reason);
}

void ADarkroomHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const UDarkroomFlowSubsystem* Flow = GetFlow();
    if (Flow && Flow->IsReady() && Flow->Era == EDarkroomEra::Film) { CluesNow = Flow->Director->CluesHere(); }
    else { CluesNow.Reset(); }
}

UDarkroomFlowSubsystem* ADarkroomHUD::GetFlow() const
{
    return GetWorld() ? GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>() : nullptr;
}

ADarkroomCharacter* ADarkroomHUD::GetPlayer() const
{
    return Cast<ADarkroomCharacter>(GetOwningPawn());
}
