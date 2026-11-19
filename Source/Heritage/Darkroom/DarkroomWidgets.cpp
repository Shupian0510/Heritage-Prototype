#include "DarkroomWidgets.h"
#include "DarkroomHUD.h"
#include "DarkroomFlowSubsystem.h"
#include "DarkroomDirector.h"
#include "DarkroomCharacter.h"
#include "DarkroomReel.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FLinearColor UiPaper(0.78f, 0.73f, 0.60f);
    const FLinearColor UiPaperLight(0.87f, 0.83f, 0.71f);
    const FLinearColor UiInk(0.07f, 0.055f, 0.045f);
    const FLinearColor UiInkSoft(0.32f, 0.28f, 0.22f);
    const FLinearColor UiRed(0.55f, 0.08f, 0.06f);
    const FLinearColor UiKey(0.93f, 0.76f, 0.25f);
    const FLinearColor UiShadow(0.f, 0.f, 0.f, 0.85f);

    FSlateFontInfo UiFont(int32 Size, bool bBold = false)
    {
        return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
    }
    TSharedRef<STextBlock> UiText(const FText& Text, int32 Size, FLinearColor Color, bool bBold = false, float WrapAt = 0.f)
    {
        return SNew(STextBlock).Text(Text).Font(UiFont(Size, bBold)).ColorAndOpacity(Color).WrapTextAt(WrapAt);
    }
    // 屏幕上那一层只搭一次，会随语言变的字每帧现取；笔记本的页面切语言时整页重排，用 UiText 就行。
    TSharedRef<STextBlock> UiLive(const FDarkroomText& Text, int32 Size, FLinearColor Color, bool bBold = false)
    {
        return SNew(STextBlock).Font(UiFont(Size, bBold)).ColorAndOpacity(Color).Text_Lambda([Text]() { return Text.Get(); });
    }
    // 英文单词比汉字占地方：同一个格子里，英文用小一号的字。
    int32 UiSize(int32 Zh, int32 En) { return DarkroomLanguage::IsEnglish() ? En : Zh; }
    TSharedRef<SWidget> UiBox(const FSlateBrush* Brush, FLinearColor Color)
    {
        return SNew(SImage).Image(Brush).ColorAndOpacity(Color);
    }
}

// ---------- 照片 ----------

void SDarkroomPhoto::Construct(const FArguments& Args)
{
    Size = Args._Size;
    if (Args._Photo)
    {
        Brush.SetResourceObject(Args._Photo);
        Brush.SetImageSize(FVector2f(float(Size.X), float(Size.Y)));
        Brush.DrawAs = ESlateBrushDrawType::Image;
        bHasPhoto = true;
    }
}

int32 SDarkroomPhoto::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const FSlateBrush* Paint = bHasPhoto ? &Brush : static_cast<const FSlateBrush*>(&Blank);
    FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Paint,
        bHasPhoto ? ESlateDrawEffect::IgnoreTextureAlpha : ESlateDrawEffect::None, Paint->GetTint(InWidgetStyle));
    return LayerId;
}

// ---------- 笔记本 ----------

void SDarkroomNotebook::Construct(const FArguments& Args)
{
    Flow = Args._Flow;
    const FVector2D Page = PageSize();
    ChildSlot
    [
        SNew(SBox).WidthOverride(float(Page.X)).HeightOverride(float(Page.Y))
        [
            SAssignNew(Canvas, SCanvas)
        ]
    ];
    Rebuild();
}

void SDarkroomNotebook::Sync()
{
    const UDarkroomFlowSubsystem* Now = Flow.Get();
    if (Now && Now->Revision != SeenRevision) { Rebuild(); }
}

void SDarkroomNotebook::Put(FVector2D Position, FVector2D Size, const TSharedRef<SWidget>& Widget)
{
    Canvas->AddSlot().Position(Position).Size(Size)[ Widget ];
}

bool SDarkroomNotebook::IsHovered(const FHot& Hot) const
{
    const UDarkroomFlowSubsystem* Now = Flow.Get();
    return Now && Hot.Contains(Now->Cursor);
}

EDarkroomHot SDarkroomNotebook::HitTest(FVector2D Pixel, int32& OutIndex) const
{
    for (int32 Index = Hots.Num() - 1; Index >= 0; --Index)
    {
        if (Hots[Index].Contains(Pixel)) { OutIndex = Hots[Index].Index; return Hots[Index].Type; }
    }
    OutIndex = INDEX_NONE;
    return EDarkroomHot::None;
}

// 页面是固定坐标排的：点到了什么，直接拿笔尖的位置和这些格子比，不经过 Slate 的命中测试。
void SDarkroomNotebook::Rebuild()
{
    Canvas->ClearChildren();
    Hots.Reset();
    UDarkroomFlowSubsystem* Now = Flow.Get();
    if (!Now) { return; }
    SeenRevision = Now->Revision;
    const FVector2D Page = PageSize();

    Put(FVector2D::ZeroVector, Page, UiBox(&White, UiPaper));
    Put(FVector2D(638, 0), FVector2D(4, Page.Y), UiBox(&White, FLinearColor(0.45f, 0.40f, 0.32f)));

    // 左页：拍到的东西。
    Put(FVector2D(44, 32), FVector2D(560, 40), UiText(FText::Format(DR_TEXT("照片　{0} 张", "Photos   {0}").Get(), FText::AsNumber(Now->Entries.Num())), 24, UiInk, true));
    const FVector2D GridOrigin(44, 88), Cell(138, 132), CardSize(134, 126);
    for (int32 Index = 0; Index < Now->Entries.Num(); ++Index)
    {
        const FDarkroomEntry& Entry = Now->Entries[Index];
        FHot Hot;
        Hot.Min = GridOrigin + FVector2D((Index % 4) * Cell.X, (Index / 4) * Cell.Y);
        Hot.Max = Hot.Min + CardSize; Hot.Type = EDarkroomHot::Entry; Hot.Index = Index;
        Hots.Add(Hot);
        Put(Hot.Min, CardSize,
            SNew(SBorder).BorderImage(&White).Padding(3.f)
            .BorderBackgroundColor_Lambda([this, Hot]() -> FSlateColor
            {
                const UDarkroomFlowSubsystem* Live = Flow.Get();
                if (Live && Live->Selected == Hot.Index) { return UiRed; }
                return IsHovered(Hot) ? UiInk : UiInkSoft;
            })
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SDarkroomPhoto).Photo(Entry.Photo.Get()).Size(FVector2D(128, 96))
                ]
                + SVerticalBox::Slot().FillHeight(1.f)
                [
                    SNew(SBorder).BorderImage(&White).BorderBackgroundColor(Entry.bKey ? UiKey : UiPaperLight)
                    .Padding(FMargin(5.f, 1.f)).VAlign(VAlign_Center)
                    [
                        UiText(Entry.Title.Get(), UiSize(13, 11), UiInk)
                    ]
                ]
            ]);
    }
    if (Now->Entries.IsValidIndex(Now->Selected))
    {
        const FDarkroomEntry& Entry = Now->Entries[Now->Selected];
        Put(FVector2D(44, 630), FVector2D(560, 34),
            UiText(FText::Format(DR_TEXT("{0}　（第 {1} 帧）", "{0}   (frame {1})").Get(), Entry.Title.Get(), FText::AsNumber(Entry.Frame + 1)), 22, UiRed, true));
        Put(FVector2D(44, 670), FVector2D(560, 110), UiText(Entry.Note.Get(), 20, UiInk, false, 550.f));
    }
    else
    {
        Put(FVector2D(44, 640), FVector2D(560, 30),
            UiText((Now->Entries.Num() > 0
                ? DR_TEXT("先选一张照片，再点右页的空格。", "Pick a photo, then click a blank on the right page.")
                : DR_TEXT("还一张都没拍。", "No photos yet.")).Get(), 18, UiInkSoft));
    }

    // 右页：接下来发生了什么？【什么】→【哪里】
    Put(FVector2D(684, 32), FVector2D(560, 50), UiText(DR_TEXT("接下来发生了什么？", "What happened next?").Get(), 30, UiInk, true));
    const FVector2D SlotSize(220, 196);
    const FVector2D SlotPos[2] = { FVector2D(700, 150), FVector2D(1000, 150) };
    const FText SlotName[2] = { DR_TEXT("什么", "WHAT").Get(), DR_TEXT("哪里", "WHERE").Get() };
    Put(FVector2D(938, 220), FVector2D(60, 50), UiText(FText::FromString(TEXT("→")), 34, UiInk, true));
    for (int32 Slot = 0; Slot < 2; ++Slot)
    {
        FHot Hot;
        Hot.Min = SlotPos[Slot]; Hot.Max = Hot.Min + SlotSize; Hot.Type = EDarkroomHot::Slot; Hot.Index = Slot;
        Hots.Add(Hot);
        Put(SlotPos[Slot] - FVector2D(0, 34), FVector2D(220, 30), UiText(SlotName[Slot], 20, UiInkSoft, true));

        TSharedPtr<SWidget> Content;
        if (Now->Entries.IsValidIndex(Now->Slots[Slot]))
        {
            const FDarkroomEntry& Entry = Now->Entries[Now->Slots[Slot]];
            Content = SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SNew(SDarkroomPhoto).Photo(Entry.Photo.Get()).Size(FVector2D(184, 138))
                ]
                + SVerticalBox::Slot().FillHeight(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    UiText(DarkroomReel::NounName(Entry.Noun).Get(), UiSize(22, 18), UiInk, true)
                ];
        }
        else
        {
            Content = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    UiText(DR_TEXT("？", "?").Get(), 54, UiInkSoft, true)
                ];
        }
        Put(SlotPos[Slot], SlotSize,
            SNew(SBorder).BorderImage(&White).Padding(4.f)
            .BorderBackgroundColor_Lambda([this, Hot]() -> FSlateColor
            {
                const UDarkroomFlowSubsystem* Live = Flow.Get();
                if (IsHovered(Hot)) { return UiInk; }
                return Live && Live->Selected != INDEX_NONE ? UiRed : UiInkSoft;
            })
            [
                SNew(SBorder).BorderImage(&White).BorderBackgroundColor(UiPaperLight).Padding(6.f)
                [
                    Content.ToSharedRef()
                ]
            ]);
    }

    FHot Go;
    Go.Min = FVector2D(840, 380); Go.Max = Go.Min + FVector2D(240, 62); Go.Type = EDarkroomHot::Develop;
    Hots.Add(Go);
    Put(Go.Min, FVector2D(240, 62),
        SNew(SBorder).BorderImage(&Rounded).HAlign(HAlign_Center).VAlign(VAlign_Center)
        .BorderBackgroundColor_Lambda([this, Go]() -> FSlateColor
        {
            const UDarkroomFlowSubsystem* Live = Flow.Get();
            if (!Live || !Live->CanDevelop()) { return FLinearColor(0.50f, 0.46f, 0.38f); }
            return IsHovered(Go) ? FLinearColor(0.78f, 0.13f, 0.08f) : UiRed;
        })
        [
            UiText(DR_TEXT("显　影", "DEVELOP").Get(), 26, FLinearColor(0.95f, 0.90f, 0.80f), true)
        ]);

    if (!Now->LastResult.IsEmpty())
    {
        Put(FVector2D(700, 462), FVector2D(520, 70), UiText(Now->LastResult.Get(), UiSize(20, 18), UiRed, false, 520.f));
    }
    if (Now->Tries.Num() > 0)
    {
        Put(FVector2D(700, 548), FVector2D(520, 28), UiText(DR_TEXT("试过的拼法", "Tried so far").Get(), 18, UiInkSoft, true));
        const int32 First = FMath::Max(0, Now->Tries.Num() - 6);
        for (int32 Index = First; Index < Now->Tries.Num(); ++Index)
        {
            const FDarkroomTry& Try = Now->Tries[Index];
            const FText Line = FText::Format(DR_TEXT("{0} → {1}　{2}", "{0} → {1}    {2}").Get(),
                DarkroomReel::NounName(Try.Subject).Get(), DarkroomReel::NounName(Try.Place).Get(),
                (Try.bAccept ? DR_TEXT("成立", "holds") : DR_TEXT("烧掉", "burnt")).Get());
            Put(FVector2D(700, 580 + (Index - First) * 26), FVector2D(520, 26), UiText(Line, 18, Try.bAccept ? UiInk : UiInkSoft));
        }
    }
    if (Now->IsReady())
    {
        Put(FVector2D(700, 752), FVector2D(520, 28), UiText(FText::Format(DR_TEXT("胶卷　{0} / {1} 帧", "Reel   {0} / {1} frames").Get(),
            FText::AsNumber(Now->Director->NumFrames()), FText::AsNumber(DarkroomReel::TotalFrames)), 18, UiInkSoft));
    }

    // 笔尖。
    Canvas->AddSlot()
        .Position(TAttribute<FVector2D>::CreateLambda([this]()
        {
            const UDarkroomFlowSubsystem* Live = Flow.Get();
            return (Live ? Live->Cursor : FVector2D(640, 400)) - FVector2D(8, 8);
        }))
        .Size(FVector2D(16, 16))
        [
            SNew(SImage).Image(&Nib)
        ];
}

// ---------- 屏幕上的那一层 ----------

UDarkroomFlowSubsystem* SDarkroomOverlay::GetFlow() const
{
    return Hud.IsValid() ? Hud->GetFlow() : nullptr;
}

const FSlateBrush* SDarkroomOverlay::BeanBrush(int32 ClueIndex) const
{
    // 绿豆是这一帧能拍的线索，黄豆是关键线索；拍过的灭掉，只留一个框。
    const TArray<FDarkroomClue>& Table = DarkroomReel::Clues();
    const UDarkroomFlowSubsystem* Flow = GetFlow();
    const bool bKey = Table.IsValidIndex(ClueIndex) && Table[ClueIndex].bKey;
    const bool bDone = Flow && Table.IsValidIndex(ClueIndex) && Flow->HasScanned(Table[ClueIndex].Id);
    if (bDone) { return bKey ? &BeanKeyHollow : &BeanHollow; }
    return bKey ? &BeanKey : &Bean;
}

FSlateColor SDarkroomOverlay::CellColor(int32 FrameIndex) const
{
    const UDarkroomFlowSubsystem* Flow = GetFlow();
    const int32 Known = Flow && Flow->IsReady() ? Flow->Director->NumFrames() : 0;
    if (FrameIndex >= Known) { return FLinearColor(0.10f, 0.09f, 0.08f, 0.75f); }
    return FrameIndex < DarkroomReel::KnownFrames ? FLinearColor(0.85f, 0.80f, 0.66f, 0.95f) : FLinearColor(1.f, 0.72f, 0.28f, 0.95f);
}

void SDarkroomOverlay::Construct(const FArguments& Args)
{
    Hud = Args._Hud;
    SetVisibility(EVisibility::HitTestInvisible);
    const float CellPitch = 52.f;

    // 翻本子的时候豆子收起来：这时拍不了照，独白又挪到了最上面，窄窗口里两者会叠在一起。
    TSharedRef<SHorizontalBox> Beans = SNew(SHorizontalBox)
        .Visibility_Lambda([this]()
        {
            const UDarkroomFlowSubsystem* Flow = GetFlow();
            return Flow && Flow->bNotebookOpen ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
        });
    for (int32 Index = 0; Index < DarkroomReel::Clues().Num(); ++Index)
    {
        Beans->AddSlot().AutoWidth().Padding(4.f, 0.f)
        [
            SNew(SBox).WidthOverride(30.f).HeightOverride(16.f)
            .Visibility_Lambda([this, Index]() { return Hud.IsValid() && Hud->CluesNow.Contains(Index) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            [
                SNew(SImage).Image_Lambda([this, Index]() { return BeanBrush(Index); })
            ]
        ];
    }

    TSharedRef<SHorizontalBox> Strip = SNew(SHorizontalBox);
    for (int32 Index = 0; Index < DarkroomReel::TotalFrames; ++Index)
    {
        Strip->AddSlot().AutoWidth().Padding(3.f, 4.f)
        [
            SNew(SBox).WidthOverride(CellPitch - 6.f).HeightOverride(32.f)
            [
                SNew(SBorder).BorderImage(&Rounded).HAlign(HAlign_Center).VAlign(VAlign_Center)
                .BorderBackgroundColor_Lambda([this, Index]() { return CellColor(Index); })
                [
                    SNew(STextBlock).Font(UiFont(14, true)).ColorAndOpacity(FLinearColor(0.05f, 0.04f, 0.03f))
                    .Text_Lambda([this, Index]()
                    {
                        const UDarkroomFlowSubsystem* Flow = GetFlow();
                        return Flow && Flow->IsReady() && Index < Flow->Director->NumFrames() ? FText::AsNumber(Index + 1) : DR_TEXT("？", "?").Get();
                    })
                ]
            ]
        ];
    }

    const auto Subtitle = [this](bool bWithNotebook) -> TSharedRef<SWidget>
    {
        return SNew(SBorder).BorderImage(&Rounded).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.6f)).Padding(FMargin(24.f, 10.f))
            .Visibility_Lambda([this, bWithNotebook]()
            {
                const UDarkroomFlowSubsystem* Flow = GetFlow();
                return Flow && Flow->bNotebookOpen == bWithNotebook && !Flow->GetSubtitle().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
            })
            [
                SNew(STextBlock).Font(UiFont(26)).ColorAndOpacity(FLinearColor(1.f, 0.97f, 0.90f))
                .WrapTextAt(980.f).Justification(ETextJustify::Center)
                .Text_Lambda([this]()
                {
                    const UDarkroomFlowSubsystem* Flow = GetFlow();
                    return Flow ? Flow->GetSubtitle() : FText::GetEmpty();
                })
            ];
    };

    ChildSlot
    [
        SNew(SDPIScaler)
        .DPIScale_Lambda([this]() -> float
        {
            // 按 1080 的高度排版，窗口多大都等比缩放。
            // 用这一层自己量到的高度，不用视口的像素数：系统缩放（比如 175%）已经算在外面了，再乘一遍界面就大了一圈。
            const float Height = float(GetTickSpaceGeometry().GetLocalSize().Y);
            return Height > 1.f ? FMath::Max(0.3f, Height / 1080.f) : 1.f;
        })
        [
            SNew(SOverlay)

            // 准星和提示。
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                SNew(SOverlay)
                .Visibility_Lambda([this]()
                {
                    const UDarkroomFlowSubsystem* Flow = GetFlow();
                    return Flow && !Flow->bNotebookOpen && !Flow->bSliceComplete ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                })
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    SNew(SBox).WidthOverride(6.f).HeightOverride(6.f)[ SNew(SImage).Image(&Dot) ]
                ]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    SNew(SBox).WidthOverride(34.f).HeightOverride(34.f)
                    .Visibility_Lambda([this]()
                    {
                        const ADarkroomCharacter* Player = Hud.IsValid() ? Hud->GetPlayer() : nullptr;
                        return Player && Player->IsFocusHot() ? EVisibility::HitTestInvisible : EVisibility::Hidden;
                    })
                    [
                        SNew(SImage).Image(&Ring)
                    ]
                ]
            ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 64.f, 0.f, 0.f)
            [
                SNew(STextBlock).Font(UiFont(20, true)).ColorAndOpacity(FLinearColor::White)
                .ShadowOffset(FVector2D(1.5, 1.5)).ShadowColorAndOpacity(UiShadow)
                .Text_Lambda([this]()
                {
                    const ADarkroomCharacter* Player = Hud.IsValid() ? Hud->GetPlayer() : nullptr;
                    return Player ? Player->GetPrompt() : FText::GetEmpty();
                })
            ]

            // 左上：只写要弄明白什么。在现在还是过去、放到第几帧，画面和下面的胶片条已经说了，不再用字重复。
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(36.f, 28.f)
            [
                SNew(STextBlock).Font(UiFont(20)).ColorAndOpacity(FLinearColor(1.f, 0.95f, 0.85f, 0.9f))
                .ShadowOffset(FVector2D(1.5, 1.5)).ShadowColorAndOpacity(UiShadow)
                .Visibility_Lambda([this]()
                {
                    const UDarkroomFlowSubsystem* Flow = GetFlow();
                    return Flow && Flow->bNotebookOpen ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
                })
                .Text_Lambda([]() { return DR_TEXT("那一夜，夫人去了哪里？", "Where did the lady go that night?").Get(); })
            ]

            // 右上：这一帧的线索豆。
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(32.f, 34.f)
            [
                Beans
            ]

            // 独白：平时在下面；本子举起来的时候挪到最上面，不压着页面。
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 160.f)
            [
                Subtitle(false)
            ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 6.f, 0.f, 0.f)
            [
                Subtitle(true)
            ]

            // 胶片条：只在过去里出现；翻本子的时候收起来（页脚上写着同样的数）。
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 100.f)
            [
                SNew(SOverlay)
                .Visibility_Lambda([this]()
                {
                    const UDarkroomFlowSubsystem* Flow = GetFlow();
                    return Flow && Flow->IsReady() && Flow->Era == EDarkroomEra::Film && !Flow->bNotebookOpen ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                })
                + SOverlay::Slot()
                [
                    Strip
                ]
                + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .Padding(TAttribute<FMargin>::CreateLambda([this, CellPitch]()
                    {
                        const UDarkroomFlowSubsystem* Flow = GetFlow();
                        return FMargin(Flow && Flow->IsReady() ? Flow->Director->GetPlayhead() * CellPitch : 0.f, 0.f, 0.f, 0.f);
                    }))
                    [
                        SNew(SBox).WidthOverride(CellPitch).HeightOverride(40.f)[ SNew(SImage).Image(&Playhead) ]
                    ]
                ]
            ]

            // 左下：胶卷。拿到之前是暗的，拿到以后亮起来。
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(36.f, 0.f, 0.f, 40.f)
            [
                SNew(SBorder).BorderImage(&Rounded).Padding(FMargin(18.f, 10.f))
                .BorderBackgroundColor_Lambda([this]() -> FSlateColor
                {
                    const UDarkroomFlowSubsystem* Flow = GetFlow();
                    return Flow && Flow->bOwnsReel ? FLinearColor(0.96f, 0.72f, 0.24f, 0.96f) : FLinearColor(0.08f, 0.08f, 0.08f, 0.6f);
                })
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
                    [
                        SNew(SBox).WidthOverride(26.f).HeightOverride(26.f)
                        [
                            SNew(SImage).Image(&ReelIcon)
                            .ColorAndOpacity_Lambda([this]() -> FSlateColor
                            {
                                const UDarkroomFlowSubsystem* Flow = GetFlow();
                                return Flow && Flow->bOwnsReel ? FLinearColor(0.08f, 0.05f, 0.02f) : FLinearColor(0.5f, 0.5f, 0.5f, 0.7f);
                            })
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Font(UiFont(20, true))
                        .ColorAndOpacity_Lambda([this]() -> FSlateColor
                        {
                            const UDarkroomFlowSubsystem* Flow = GetFlow();
                            return Flow && Flow->bOwnsReel ? FLinearColor(0.08f, 0.05f, 0.02f) : FLinearColor(0.5f, 0.5f, 0.5f, 0.7f);
                        })
                        .Text_Lambda([this]()
                        {
                            const UDarkroomFlowSubsystem* Flow = GetFlow();
                            if (!Flow || !Flow->bOwnsReel) { return DR_TEXT("胶卷", "Reel").Get(); }
                            return (Flow->Era == EDarkroomEra::Film
                                ? DR_TEXT("F　回到现在", "F   Back to the present")
                                : DR_TEXT("F　进入过去", "F   Enter the past")).Get();
                        })
                    ]
                ]
            ]

            // 右下：这会儿能按什么。
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 36.f, 46.f)
            [
                SNew(STextBlock).Font(UiFont(16)).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.8f))
                .ShadowOffset(FVector2D(1.5, 1.5)).ShadowColorAndOpacity(UiShadow)
                .Text_Lambda([this]()
                {
                    const UDarkroomFlowSubsystem* Flow = GetFlow();
                    if (!Flow || Flow->bSliceComplete) { return FText::GetEmpty(); }
                    if (Flow->bNotebookOpen)
                    {
                        return DR_TEXT("鼠标 移动笔尖　左键 选取 / 放入　Tab 合上", "Mouse  move the nib     LMB  pick / place     Tab  close").Get();
                    }
                    if (Flow->Era == EDarkroomEra::Film)
                    {
                        return DR_TEXT("按住 E 正放　按住 Q 倒放　左键 拍照　Tab 笔记本", "Hold E  play     Hold Q  rewind     LMB  photograph     Tab  notebook").Get();
                    }
                    return DR_TEXT("WASD 移动　E 互动　Tab 笔记本", "WASD  move     E  interact     Tab  notebook").Get();
                })
            ]

            // 闪光：快门、进出过去、显影的效果；烧片是另一层。
            + SOverlay::Slot()
            [
                SNew(SImage).Image(&White)
                .ColorAndOpacity_Lambda([this]() -> FSlateColor
                {
                    const UDarkroomFlowSubsystem* Flow = GetFlow();
                    return Flow ? Flow->GetFlash() : FLinearColor::Transparent;
                })
            ]
            + SOverlay::Slot()
            [
                SNew(SImage).Image(&White)
                .ColorAndOpacity_Lambda([this]() -> FSlateColor
                {
                    const UDarkroomFlowSubsystem* Flow = GetFlow();
                    const float Burn = Flow && Flow->IsReady() ? Flow->Director->GetBurnAlpha() : 0.f;
                    return FLinearColor(0.95f, 0.38f, 0.04f, Burn * 0.9f);
                })
            ]

            // 结束。
            + SOverlay::Slot()
            [
                SNew(SBorder).BorderImage(&White).BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.86f))
                .HAlign(HAlign_Center).VAlign(VAlign_Center)
                .Visibility_Lambda([this]()
                {
                    const UDarkroomFlowSubsystem* Flow = GetFlow();
                    return Flow && Flow->bSliceComplete && Flow->SecondsSinceComplete() > 3.5f ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                })
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [
                        UiLive(DR_TEXT("原型到此结束", "End of the prototype"), 40, FLinearColor(1.f, 0.95f, 0.85f), true)
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 28.f)
                    [
                        SNew(STextBlock).Font(UiFont(20)).ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f)).Justification(ETextJustify::Left)
                        .WrapTextAt(1100.f)
                        .Text_Lambda([this]()
                        {
                            const UDarkroomFlowSubsystem* Flow = GetFlow();
                            return Flow && Flow->bSliceComplete ? FText::FromString(Flow->BuildReport(false)) : FText::GetEmpty();
                        })
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [
                        UiLive(DR_TEXT("按 F5 重来　试玩记录已保存到 Saved/Darkroom", "F5 to restart     Playtest log saved to Saved/Darkroom"), 16, FLinearColor(0.7f, 0.7f, 0.7f))
                    ]
                ]
            ]
        ]
    ];
}
