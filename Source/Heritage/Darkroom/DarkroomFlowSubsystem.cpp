#include "DarkroomFlowSubsystem.h"
#include "DarkroomRoom.h"
#include "DarkroomDirector.h"
#include "DarkroomProp.h"
#include "DarkroomReel.h"
#include "DarkroomCharacter.h"
#include "Heritage.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"

bool UDarkroomFlowSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDarkroomFlowSubsystem::Deinitialize()
{
    // 没走到最后就退出的，也留一份记录：停在哪了本身就是答案的一部分。
    if (bStarted && !bSaved && Events.Num() > 2) { SaveReport(); }
    Super::Deinitialize();
}

double UDarkroomFlowSubsystem::Now() const { return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0; }

void UDarkroomFlowSubsystem::Register(ADarkroomRoom* InReality, ADarkroomRoom* InFilm, ADarkroomDirector* InDirector)
{
    RealityRoom = InReality; FilmRoom = InFilm; Director = InDirector;
}

void UDarkroomFlowSubsystem::Begin()
{
    bStarted = true; StartTime = Now();
    Say(DR_TEXT("门是被人从外面撞开的，可它原本是从里面插上的。", "The door was broken in from the outside. Yet it was bolted from within."));
    Say(DR_TEXT("那天夜里，屋里的人去了哪儿？", "So where did whoever was in this room go that night?"), true);
    Log(TEXT("开始"));
}

// ---- 独白和闪光 ----

void UDarkroomFlowSubsystem::Say(const FDarkroomText& Line, bool bQueue)
{
    if (Line.IsEmpty()) { return; }
    const double Time = Now();
    if (!bQueue) { Said.Reset(); }
    FSaid Entry; Entry.Line = Line;
    Entry.Start = Said.Num() > 0 ? FMath::Max(Time, Said.Last().End + 0.25) : Time;
    // 停留多久按字数算：一个汉字比一个英文字母耐读得多。两种语言取长的，中途切换也不会被掐掉。
    const double Reading = FMath::Max(0.17 * Line.Zh.ToString().Len(), 0.06 * Line.En.ToString().Len());
    Entry.End = Entry.Start + FMath::Clamp(1.6 + Reading, 2.4, 7.0);
    Said.Add(Entry);
}

FText UDarkroomFlowSubsystem::GetSubtitle() const
{
    const double Time = Now();
    for (const FSaid& Entry : Said) { if (Time >= Entry.Start && Time < Entry.End) { return Entry.Line.Get(); } }
    return FText::GetEmpty();
}

void UDarkroomFlowSubsystem::Flash(FLinearColor Color, float Seconds)
{
    FlashColor = Color; FlashStart = Now(); FlashSeconds = FMath::Max(Seconds, 0.01f);
}

FLinearColor UDarkroomFlowSubsystem::GetFlash() const
{
    const double Alpha = 1.0 - (Now() - FlashStart) / FMath::Max(FlashSeconds, 0.01);
    if (Alpha <= 0.0 || Alpha > 1.0) { return FLinearColor::Transparent; }
    FLinearColor Result = FlashColor; Result.A *= float(Alpha);
    return Result;
}

// ---- 现在 ----

void UDarkroomFlowSubsystem::CollectReel()
{
    if (bOwnsReel) { return; }
    bOwnsReel = true; ++Revision;
    Say(DR_TEXT("捡到了胶卷，可以播放试试。", "Picked up a reel of film. Try playing it."));
    Log(TEXT("拾取胶卷"));
}

void UDarkroomFlowSubsystem::NotifyBustTurned(float Yaw)
{
    // 现在这边转对了也不出声、不出字：胸像该朝哪儿，只能从那一夜里看来。有提示的话，瞎转三下就过关了。
    ++BustTurns;
    const bool bAligned = FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, DarkroomLayout::BustPortraitYaw)) < 5.f;
    Log(FString::Printf(TEXT("现在：转胸像 → %d°%s"), FMath::RoundToInt(Yaw), bAligned ? TEXT("（朝着画像）") : TEXT("")));
}

void UDarkroomFlowSubsystem::NotifyBustReturned()
{
    // 只在第一次说一句：玩家这时多半已经转身走开了，得让他知道胸像不会停在那儿。之后就不再出字。
    if (++BustReturns == 1) { Say(DR_TEXT("胸像自己转回去了。", "The bust has turned back on its own.")); }
    Log(TEXT("现在：胸像自己转回原位"));
}

void UDarkroomFlowSubsystem::TryFireback(ADarkroomProp* Fireback)
{
    if (!IsReady() || !Fireback || bFirebackOpen) { return; }
    const ADarkroomProp* Bust = RealityRoom->Find(DarkroomLayout::PropBust);
    const bool bAligned = Bust && FMath::Abs(FMath::FindDeltaAngleDegrees(Bust->GetFacingYaw(), DarkroomLayout::BustPortraitYaw)) < 5.f;
    if (!bAligned)
    {
        ++FirebackFails;
        Say(DR_TEXT("铸铁的背板，推上去纹丝不动。", "A cast-iron fireback. It will not budge."));
        Log(TEXT("现在：推背板，没动"));
        return;
    }
    bFirebackOpen = true;
    // 暗道尽头的光这时才亮：背板关着的时候它是灭的，缝里什么也看不出来。
    TInlineComponentArray<UPointLightComponent*> Lamps(RealityRoom);
    for (UPointLightComponent* Lamp : Lamps)
    {
        if (Lamp && Lamp->GetFName() == TEXT("TunnelGlow")) { Lamp->SetIntensity(3.f); }
    }
    const FTransform Target = FTransform(DarkroomLayout::FirebackOpenRot, DarkroomLayout::FirebackOpen) * RealityRoom->GetActorTransform();
    Fireback->TweenTo(Target.GetLocation(), Target.Rotator(), 1.8f);
    Say(DR_TEXT("背板朝里转开了，后面是空的。", "The fireback swings inward. It is hollow behind."));
    Log(FString::Printf(TEXT("现在：背板开了（胶卷%s补全）"), Director->IsReelComplete() ? TEXT("已") : TEXT("没")));
}

void UDarkroomFlowSubsystem::CheckPassage(const FVector& PawnLocation)
{
    if (!IsReady() || bSliceComplete || !bFirebackOpen || Era != EDarkroomEra::Reality) { return; }
    if (RealityRoom->ToLocal(PawnLocation).X < DarkroomLayout::PassageX) { return; }
    bSliceComplete = true; CompleteTime = Now(); bNotebookOpen = false;
    Say(DR_TEXT("原来是条暗道。她就是从这儿走的。", "A hidden passage. So this is how she left."));
    Log(TEXT("走进暗道，结束"));
    SaveReport();
}

float UDarkroomFlowSubsystem::SecondsSinceComplete() const { return bSliceComplete ? float(Now() - CompleteTime) : 0.f; }

// ---- 进出过去 ----

bool UDarkroomFlowSubsystem::CanToggleEra() const
{
    return IsReady() && bOwnsReel && !bSliceComplete && Director->IsIdle();
}

bool UDarkroomFlowSubsystem::ToggleEra()
{
    if (!CanToggleEra()) { return false; }
    SetNotebookOpen(false);
    ADarkroomRoom* FromRoom = CurrentRoom();
    Era = Era == EDarkroomEra::Reality ? EDarkroomEra::Film : EDarkroomEra::Reality;
    ADarkroomRoom* ToRoom = CurrentRoom();
    Director->SetTransport(0);
    if (ACharacter* Pawn = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0))
    {
        // 两间房摆设一样：站在哪儿进去，就站在哪儿出来。
        Pawn->TeleportTo(ToRoom->ToWorld(FromRoom->ToLocal(Pawn->GetActorLocation())), Pawn->GetActorRotation(), false, true);
        if (ADarkroomCharacter* Player = Cast<ADarkroomCharacter>(Pawn)) { Player->ApplyEra(Era); }
    }
    Flash(FLinearColor(1.f, 0.96f, 0.86f, 0.9f), 0.7f);
    ++EraTrips; ++Revision;
    if (Era == EDarkroomEra::Film && EraTrips == 1) { Say(DR_TEXT("还是这间书房，只是回到了那一夜。", "The same study, as it was that night.")); }
    Log(Era == EDarkroomEra::Film ? FString::Printf(TEXT("进入过去（第 %d 帧）"), Director->GetFrame() + 1) : FString(TEXT("回到现在")));
    return true;
}

// ---- 拍照 ----

bool UDarkroomFlowSubsystem::HasScanned(FName ClueId) const
{
    return Entries.ContainsByPredicate([ClueId](const FDarkroomEntry& Entry) { return Entry.ClueId == ClueId; });
}

bool UDarkroomFlowSubsystem::Scan(int32 ClueIndex, UTextureRenderTarget2D* Photo)
{
    const TArray<FDarkroomClue>& Table = DarkroomReel::Clues();
    if (!IsReady() || !Table.IsValidIndex(ClueIndex) || HasScanned(Table[ClueIndex].Id)) { return false; }
    const FDarkroomClue& Clue = Table[ClueIndex];
    FDarkroomEntry Entry;
    Entry.ClueId = Clue.Id; Entry.Noun = Clue.Noun; Entry.Title = Clue.Title; Entry.Note = Clue.Note;
    Entry.bKey = Clue.bKey; Entry.Frame = Director->GetFrame(); Entry.Photo = Photo;
    Entries.Add(Entry);
    ++Revision;
    Say(Clue.Line);
    Log(FString::Printf(TEXT("拍照：%s%s（第 %d 帧）"), *Clue.Title.Zh.ToString(), Clue.bKey ? TEXT("【关键】") : TEXT(""), Entry.Frame + 1));
    return true;
}

void UDarkroomFlowSubsystem::NoteEmptyShot() { ++EmptyShots; }

// ---- 笔记本 ----

bool UDarkroomFlowSubsystem::SetNotebookOpen(bool bOpen)
{
    if (bOpen == bNotebookOpen) { return true; }
    if (bOpen && (!IsReady() || bSliceComplete || (Era == EDarkroomEra::Film && !Director->IsIdle()))) { return false; }
    bNotebookOpen = bOpen; ++Revision;
    if (bOpen)
    {
        ++NotebookOpens; Selected = INDEX_NONE;
        Director->SetTransport(0);
        Log(FString::Printf(TEXT("打开笔记本（%d 张照片）"), Entries.Num()));
    }
    return true;
}

void UDarkroomFlowSubsystem::ClickEntry(int32 Index)
{
    if (!Entries.IsValidIndex(Index)) { return; }
    Selected = Selected == Index ? INDEX_NONE : Index;
    ++Revision;
}

void UDarkroomFlowSubsystem::ClickSlot(int32 Slot)
{
    if (Slot < 0 || Slot > 1) { return; }
    if (Selected != INDEX_NONE) { Slots[Slot] = Selected; Selected = INDEX_NONE; }
    else { Slots[Slot] = INDEX_NONE; }
    ++Revision;
}

bool UDarkroomFlowSubsystem::CanDevelop() const
{
    return IsReady() && Era == EDarkroomEra::Film && Entries.IsValidIndex(Slots[0]) && Entries.IsValidIndex(Slots[1])
        && Director->IsIdle() && !Director->IsReelComplete();
}

bool UDarkroomFlowSubsystem::Develop()
{
    if (!CanDevelop())
    {
        if (IsReady() && Director->IsReelComplete()) { LastResult = DR_TEXT("胶卷已经补全了。", "The reel is already complete."); }
        else if (Era != EDarkroomEra::Film) { LastResult = DR_TEXT("要先回到过去，才能显影。", "Go back into the past to develop."); }
        else { LastResult = DR_TEXT("两个空格都要放上照片。", "Both blanks need a photo."); }
        ++Revision;
        return false;
    }
    const FName Subject = Entries[Slots[0]].Noun, Place = Entries[Slots[1]].Noun;
    SetNotebookOpen(false);         // 合上本子，抬头看
    return ComposeDirect(Subject, Place);
}

bool UDarkroomFlowSubsystem::ComposeDirect(FName Subject, FName Place)
{
    if (!IsReady() || Era != EDarkroomEra::Film) { return false; }
    if (PhotosBeforeFirstTry == INDEX_NONE) { PhotosBeforeFirstTry = Entries.Num(); }
    PendingTry.Subject = Subject; PendingTry.Place = Place; PendingTry.bAccept = false;
    Flash(FLinearColor(1.f, 0.96f, 0.86f, 0.6f), 0.4f);
    return Director->Compose(Subject, Place);
}

void UDarkroomFlowSubsystem::OnLanguageChanged()
{
    ++Revision;     // 笔记本的页面是按 Revision 重排的
    Log(DarkroomLanguage::IsEnglish() ? TEXT("切换语言 → English") : TEXT("切换语言 → 中文"));
}

void UDarkroomFlowSubsystem::OnAttemptResolved(bool bAccept, const FDarkroomText& Line)
{
    PendingTry.bAccept = bAccept;
    Tries.Add(PendingTry);
    LastResult = Line;
    Slots[0] = Slots[1] = INDEX_NONE; Selected = INDEX_NONE;
    ++Revision;
    Log(FString::Printf(TEXT("显影：%s → %s　%s　「%s」"), *DarkroomReel::NounName(PendingTry.Subject).Zh.ToString(),
        *DarkroomReel::NounName(PendingTry.Place).Zh.ToString(), bAccept ? TEXT("成立") : TEXT("烧掉"), *Line.Zh.ToString()));
}

void UDarkroomFlowSubsystem::OnReelComplete()
{
    ++Revision;
    Say(DarkroomReel::FinalLine());
    Log(TEXT("胶卷补全"));
}

// ---- 试玩记录 ----

void UDarkroomFlowSubsystem::Log(const FString& What)
{
    const int32 Seconds = FMath::FloorToInt32(Now() - StartTime);
    Events.Add(FString::Printf(TEXT("[%02d:%02d] %s"), Seconds / 60, Seconds % 60, *What));
    UE_LOG(LogHeritage, Log, TEXT("Darkroom %s"), *Events.Last());
}

FString UDarkroomFlowSubsystem::BuildReport(bool bWithEvents) const
{
    const TArray<FDarkroomClue>& Table = DarkroomReel::Clues();
    int32 KeyTotal = 0, KeyFound = 0, Right = 0, Wrong = 0;
    for (const FDarkroomClue& Clue : Table) { KeyTotal += Clue.bKey ? 1 : 0; }
    for (const FDarkroomEntry& Entry : Entries) { KeyFound += Entry.bKey ? 1 : 0; }
    for (const FDarkroomTry& Try : Tries) { (Try.bAccept ? Right : Wrong) += 1; }
    const int32 Seconds = FMath::FloorToInt32((bSliceComplete ? CompleteTime : Now()) - StartTime);

    const bool bReelDone = Director && Director->IsReelComplete();
    const int32 RewindCount = Director ? Director->Rewinds : 0;
    FString Text;
    // 带过程的那份是写给设计者看的（存盘、打日志），一律中文；结束画面上那份跟着玩家当前的语言。
    if (!bWithEvents && DarkroomLanguage::IsEnglish())
    {
        Text += FString::Printf(TEXT("Time %d:%02d    Entered the passage: %s    Reel completed: %s\n"), Seconds / 60, Seconds % 60,
            bSliceComplete ? TEXT("yes") : TEXT("no"), bReelDone ? TEXT("yes") : TEXT("no"));
        Text += FString::Printf(TEXT("Clues photographed: %d / %d (key %d / %d), empty shots: %d\n"), Entries.Num(), Table.Num(), KeyFound, KeyTotal, EmptyShots);
        Text += FString::Printf(TEXT("Developed %d times: %d held, %d burnt; %d photos before the first try\n"), Tries.Num(), Right, Wrong, FMath::Max(PhotosBeforeFirstTry, 0));
        Text += FString::Printf(TEXT("Rewinds: %d, notebook opened: %d, trips between past and present: %d\n"), RewindCount, NotebookOpens, EraTrips);
        Text += FString::Printf(TEXT("In the present: bust turned %d times and sprang back %d times, fireback pushed in vain %d times\n"), BustTurns, BustReturns, FirebackFails);
        if (Tries.Num() > 0)
        {
            Text += TEXT("Order: ");
            for (int32 Index = 0; Index < Tries.Num(); ++Index)
            {
                Text += FString::Printf(TEXT("%s%s → %s (%s)"), Index > 0 ? TEXT(", ") : TEXT(""), *DarkroomReel::NounName(Tries[Index].Subject).En.ToString(),
                    *DarkroomReel::NounName(Tries[Index].Place).En.ToString(), Tries[Index].bAccept ? TEXT("held") : TEXT("burnt"));
            }
            Text += TEXT("\n");
        }
        return Text;
    }
    Text += FString::Printf(TEXT("用时 %d 分 %02d 秒　走进暗道：%s　胶卷补全：%s\n"), Seconds / 60, Seconds % 60,
        bSliceComplete ? TEXT("是") : TEXT("否"), bReelDone ? TEXT("是") : TEXT("否"));
    Text += FString::Printf(TEXT("拍到 %d / %d 条线索（关键 %d / %d），空拍 %d 次\n"), Entries.Num(), Table.Num(), KeyFound, KeyTotal, EmptyShots);
    Text += FString::Printf(TEXT("显影 %d 次：成立 %d，烧掉 %d；第一次显影前拍了 %d 张\n"), Tries.Num(), Right, Wrong, FMath::Max(PhotosBeforeFirstTry, 0));
    Text += FString::Printf(TEXT("倒放 %d 次，翻笔记本 %d 次，进出过去 %d 次\n"), RewindCount, NotebookOpens, EraTrips);
    Text += FString::Printf(TEXT("现在：转胸像 %d 次，胸像自己转回去 %d 次，推背板没动 %d 次\n"), BustTurns, BustReturns, FirebackFails);
    if (Tries.Num() > 0)
    {
        Text += TEXT("显影顺序：");
        for (int32 Index = 0; Index < Tries.Num(); ++Index)
        {
            Text += FString::Printf(TEXT("%s%s→%s %s"), Index > 0 ? TEXT("，") : TEXT(""), *DarkroomReel::NounName(Tries[Index].Subject).Zh.ToString(),
                *DarkroomReel::NounName(Tries[Index].Place).Zh.ToString(), Tries[Index].bAccept ? TEXT("成立") : TEXT("烧掉"));
        }
        Text += TEXT("\n");
    }
    if (bWithEvents)
    {
        Text += TEXT("\n--- 过程 ---\n");
        for (const FString& Event : Events) { Text += Event + TEXT("\n"); }
    }
    return Text;
}

void UDarkroomFlowSubsystem::SaveReport()
{
    if (bQuiet) { return; }
    bSaved = true;
    const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
    const FString Path = FPaths::ProjectSavedDir() / TEXT("Darkroom") / FString::Printf(TEXT("playtest_%s.txt"), *Stamp);
    const FString Text = FString::Printf(TEXT("暗房（原型 3）试玩记录　%s　结束时的语言：%s\n"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")),
        DarkroomLanguage::IsEnglish() ? TEXT("English") : TEXT("中文")) + BuildReport(true);
    FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8);
    UE_LOG(LogHeritage, Log, TEXT("Darkroom 试玩记录写到 %s"), *Path);
}
