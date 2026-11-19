// 把 SDarkroomOverlay 挂到视口上，并替它每帧算好"这一帧有哪些线索"。
// Richard 原型的 HUD 是用 Canvas 画的；这里换成 Slate，因为界面上的字是中文。
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DarkroomHUD.generated.h"

class UDarkroomFlowSubsystem;
class ADarkroomCharacter;
class SWidget;

UCLASS()
class ADarkroomHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    UDarkroomFlowSubsystem* GetFlow() const;
    ADarkroomCharacter* GetPlayer() const;

    TArray<int32> CluesNow;         // 这一帧上能拍的线索（线索表的下标）
private:
    TSharedPtr<SWidget> Overlay;
};
