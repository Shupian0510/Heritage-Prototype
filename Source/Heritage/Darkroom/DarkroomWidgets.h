// 界面，全部用 Slate 在代码里搭（中文要走 Slate 的字体回退，Canvas 的默认字体没有中文）。
// SDarkroomOverlay：贴在屏幕上的那一层 —— 线索豆、准星、独白、胶片条、胶卷按钮。
// SDarkroomNotebook：画在手里那本笔记本上的页面，由角色身上的 WidgetComponent 渲染到世界里。
#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"

class UDarkroomFlowSubsystem;
class ADarkroomHUD;
class UTextureRenderTarget2D;
class SCanvas;

// 一张照片。场景捕获出来的贴图透明度是 0，所以画的时候不看透明度。
class SDarkroomPhoto : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDarkroomPhoto) : _Photo(nullptr), _Size(FVector2D(128, 96)) {}
        SLATE_ARGUMENT(UTextureRenderTarget2D*, Photo)
        SLATE_ARGUMENT(FVector2D, Size)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual FVector2D ComputeDesiredSize(float) const override { return Size; }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
private:
    FSlateBrush Brush;
    FSlateColorBrush Blank = FSlateColorBrush(FLinearColor(0.05f, 0.045f, 0.04f));
    FVector2D Size = FVector2D(128, 96);
    bool bHasPhoto = false;
};

enum class EDarkroomHot : uint8 { None, Entry, Slot, Develop };

class SDarkroomNotebook : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDarkroomNotebook) {}
        SLATE_ARGUMENT(TWeakObjectPtr<UDarkroomFlowSubsystem>, Flow)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void Sync();                                                    // 内容变了就重排；每帧由角色调用
    EDarkroomHot HitTest(FVector2D Pixel, int32& OutIndex) const;   // 笔尖点在了什么上
    static FVector2D PageSize() { return FVector2D(1280, 800); }
private:
    struct FHot
    {
        FVector2D Min = FVector2D::ZeroVector;
        FVector2D Max = FVector2D::ZeroVector;
        EDarkroomHot Type = EDarkroomHot::None;
        int32 Index = INDEX_NONE;
        bool Contains(FVector2D Point) const { return Point.X >= Min.X && Point.X <= Max.X && Point.Y >= Min.Y && Point.Y <= Max.Y; }
    };
    void Rebuild();
    void Put(FVector2D Position, FVector2D Size, const TSharedRef<SWidget>& Widget);
    bool IsHovered(const FHot& Hot) const;

    TWeakObjectPtr<UDarkroomFlowSubsystem> Flow;
    TSharedPtr<SCanvas> Canvas;
    TArray<FHot> Hots;
    int32 SeenRevision = -1;
    FSlateColorBrush White = FSlateColorBrush(FLinearColor::White);
    FSlateRoundedBoxBrush Rounded = FSlateRoundedBoxBrush(FLinearColor::White, 8.f);
    FSlateRoundedBoxBrush Nib = FSlateRoundedBoxBrush(FLinearColor(0.55f, 0.08f, 0.06f), 8.f, FLinearColor(1.f, 0.95f, 0.85f), 2.f);
};

class SDarkroomOverlay : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDarkroomOverlay) {}
        SLATE_ARGUMENT(TWeakObjectPtr<ADarkroomHUD>, Hud)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
private:
    UDarkroomFlowSubsystem* GetFlow() const;
    const FSlateBrush* BeanBrush(int32 ClueIndex) const;
    FSlateColor CellColor(int32 FrameIndex) const;

    TWeakObjectPtr<ADarkroomHUD> Hud;
    FSlateColorBrush White = FSlateColorBrush(FLinearColor::White);
    FSlateRoundedBoxBrush Rounded = FSlateRoundedBoxBrush(FLinearColor::White, 8.f);
    FSlateRoundedBoxBrush Bean = FSlateRoundedBoxBrush(FLinearColor(0.22f, 0.86f, 0.36f), 8.f);
    FSlateRoundedBoxBrush BeanKey = FSlateRoundedBoxBrush(FLinearColor(1.f, 0.80f, 0.12f), 8.f);
    FSlateRoundedBoxBrush BeanHollow = FSlateRoundedBoxBrush(FLinearColor(0, 0, 0, 0.25f), 8.f, FLinearColor(0.22f, 0.86f, 0.36f, 0.75f), 2.f);
    FSlateRoundedBoxBrush BeanKeyHollow = FSlateRoundedBoxBrush(FLinearColor(0, 0, 0, 0.25f), 8.f, FLinearColor(1.f, 0.80f, 0.12f, 0.75f), 2.f);
    FSlateRoundedBoxBrush Dot = FSlateRoundedBoxBrush(FLinearColor(1, 1, 1, 0.85f), 3.f);
    FSlateRoundedBoxBrush Ring = FSlateRoundedBoxBrush(FLinearColor::Transparent, 17.f, FLinearColor(1, 1, 1, 0.9f), 2.f);
    FSlateRoundedBoxBrush ReelIcon = FSlateRoundedBoxBrush(FLinearColor::Transparent, 13.f, FLinearColor::White, 5.f);
    FSlateRoundedBoxBrush Playhead = FSlateRoundedBoxBrush(FLinearColor::Transparent, 5.f, FLinearColor(1, 1, 1, 0.95f), 3.f);
};
