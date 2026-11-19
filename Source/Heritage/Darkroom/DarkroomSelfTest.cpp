// 无界面自测（-DarkroomSelfTest）和调试用的控制台命令（Darkroom.*）。
// 自测的做法照搬 Richard 原型的 HeritageSelfTest：不渲染，把整条流程按规则走一遍，用退出码报结果。
#include "DarkroomGameMode.h"
#include "DarkroomCharacter.h"
#include "DarkroomDirector.h"
#include "DarkroomFlowSubsystem.h"
#include "DarkroomProp.h"
#include "DarkroomReel.h"
#include "DarkroomRoom.h"
#include "Heritage.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

bool RunDarkroomSelfTest(UWorld* World)
{
    using namespace DarkroomLayout;
    int32 Passed = 0, Total = 0;
    const auto Check = [&Passed, &Total](bool bOk, const TCHAR* What)
    {
        ++Total; Passed += bOk ? 1 : 0;
        if (bOk) { UE_LOG(LogHeritage, Display, TEXT("DARKROOM SELFTEST ok   %s"), What); }
        else { UE_LOG(LogHeritage, Error, TEXT("DARKROOM SELFTEST FAIL %s"), What); }
    };
    const auto Finish = [&Passed, &Total]()
    {
        UE_LOG(LogHeritage, Display, TEXT("DARKROOM SELFTEST: %s (%d/%d)"), Passed == Total ? TEXT("PASS") : TEXT("FAIL"), Passed, Total);
        return Passed == Total;
    };

    UDarkroomFlowSubsystem* Flow = World ? World->GetSubsystem<UDarkroomFlowSubsystem>() : nullptr;
    Check(Flow && Flow->IsReady(), TEXT("流程、两间房、时间轴都在"));
    if (!Flow || !Flow->IsReady()) { return Finish(); }
    Flow->bQuiet = true;
    ADarkroomDirector* Director = Flow->Director;
    ADarkroomRoom* Reality = Flow->RealityRoom;
    ADarkroomRoom* Film = Flow->FilmRoom;
    ADarkroomCharacter* Player = Cast<ADarkroomCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
    Check(Player != nullptr, TEXT("玩家是暗房的角色"));
    // 开发版里引擎把 F5 绑在调试视图上，和"F5 重来"撞了；角色装好输入以后应该已经把这一条拿掉。
    APlayerController* PlayerPC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
    Check(PlayerPC && PlayerPC->PlayerInput && PlayerPC->PlayerInput->GetBind(EKeys::F5).IsEmpty(), TEXT("F5 只管重来，上面没有引擎的调试命令"));
    const auto Step = [Director](int32 Dir)
    {
        Director->SetTransport(Dir); Director->Advance(0.01f); Director->SetTransport(0); Director->Flush();
    };

    // 现在：E 拾取胶卷，然后才能进入过去。
    Check(!Flow->ToggleEra(), TEXT("没有胶卷进不了过去"));
    ADarkroomProp* Reel = Reality->Find(PropReel);
    Check(Reel && Reel->OnUse() && Flow->bOwnsReel, TEXT("按 E 拾取胶卷"));
    Check(Flow->ToggleEra() && Flow->Era == EDarkroomEra::Film, TEXT("拿到胶卷后进入过去"));
    Check(Player && Film->ToLocal(Player->GetActorLocation()).Size2D() < 600.f, TEXT("人被送到了胶卷里的那间房"));

    // 已知的三帧，正放倒放。
    Check(Director->NumFrames() == 3 && Director->CluesHere().Num() == 7, TEXT("第 1 帧有 7 条线索"));
    Step(1); Check(Director->GetFrame() == 1 && Director->CluesHere().Num() == 8, TEXT("第 2 帧：闩门，多一条"));
    Step(1); Check(Director->GetFrame() == 2 && Director->CluesHere().Num() == 9, TEXT("第 3 帧：窗外来人，再多一条"));
    Step(1); Check(Director->GetFrame() == 2, TEXT("放到头就停住"));
    Step(-1); Check(Director->GetFrame() == 1 && Director->Rewinds == 1, TEXT("倒放回到上一帧"));
    Step(1);

    // 拍照。
    const int32 LetterClue = Director->ClueFor(Film->Find(PropLetter));
    Check(LetterClue == 0, TEXT("对着完好的信，拍到的是『信』"));
    Check(Flow->Scan(LetterClue, nullptr) && Flow->Entries.Num() == 1 && !Flow->Scan(LetterClue, nullptr), TEXT("拍一张记一条，重复不记"));
    Check(Director->ClueFor(Film->Find(PropFireback)) == INDEX_NONE, TEXT("火还烧着时拍不到背板"));

    // 规则表：对错都要有反应。
    const FName Nouns[] = { DarkroomNoun::Lady, DarkroomNoun::Letter, DarkroomNoun::Vase, DarkroomNoun::Bust, DarkroomNoun::Fire,
        DarkroomNoun::Door, DarkroomNoun::Window, DarkroomNoun::Portrait, DarkroomNoun::Desk, DarkroomNoun::Pursuer };
    int32 Accepts = 0, Acted = 0, Silent = 0;
    for (const FName Subject : Nouns)
    {
        for (const FName Place : Nouns)
        {
            const FDarkroomAttempt Attempt = DarkroomReel::Resolve(FDarkroomStory(), Subject, Place);
            Accepts += Attempt.bAccept ? 1 : 0;
            Acted += Attempt.Steps.Num() > 0 ? 1 : 0;
            Silent += Attempt.Line.IsEmpty() ? 1 : 0;
        }
    }
    Check(Accepts == 2, TEXT("一开始只有两种拼法成立"));
    Check(Acted - Accepts >= 20, TEXT("至少二十种错的拼法世界也会演出来"));
    Check(Silent == 0, TEXT("每一种拼法都有一句回应"));

    // 文本：每一句都得有中英两个版本，切换语言后取到的是另一种。
    int32 Missing = 0;
    for (const FDarkroomClue& Clue : DarkroomReel::Clues())
    {
        Missing += Clue.Title.HasBoth() && Clue.Note.HasBoth() && Clue.Line.HasBoth() ? 0 : 1;
    }
    const bool StoryBits[] = { false, true };
    for (const FName Subject : Nouns)
    {
        Missing += DarkroomReel::NounName(Subject).HasBoth() ? 0 : 1;
        for (const FName Place : Nouns)
        {
            for (const bool bBurnt : StoryBits) { for (const bool bOut : StoryBits) { for (const bool bTurned : StoryBits)
            {
                FDarkroomStory Probe; Probe.bLetterBurnt = bBurnt; Probe.bFireOut = bOut; Probe.bBustTurned = bTurned;
                Missing += DarkroomReel::Resolve(Probe, Subject, Place).Line.HasBoth() ? 0 : 1;
            } } }
        }
    }
    for (const auto& Pair : Reality->Props)
    {
        const ADarkroomProp* Prop = Pair.Value;
        if (Prop && ((!Prop->RealityLine.IsEmpty() && !Prop->RealityLine.HasBoth()) || (!Prop->UsePrompt.IsEmpty() && !Prop->UsePrompt.HasBoth()))) { ++Missing; }
    }
    Missing += DarkroomReel::FinalLine().HasBoth() ? 0 : 1;
    Check(Missing == 0, TEXT("每一句话都有中英两个版本"));
    const bool bWasEnglish = DarkroomLanguage::IsEnglish();
    DarkroomLanguage::Set(false); const FString LadyZh = DarkroomReel::NounName(DarkroomNoun::Lady).Get().ToString();
    DarkroomLanguage::Set(true); const FString LadyEn = DarkroomReel::NounName(DarkroomNoun::Lady).Get().ToString();
    DarkroomLanguage::Set(bWasEnglish);
    Check(LadyZh == TEXT("夫人") && LadyEn == TEXT("The Lady"), TEXT("切换语言后取到的是另一种文字"));

    // 顺序不对：先浇火。照演，然后烧掉，世界退回去。
    ADarkroomProp* Fire = Film->Find(PropFireplace);
    Check(Flow->ComposeDirect(DarkroomNoun::Vase, DarkroomNoun::Fire) && !Director->IsIdle(), TEXT("显影：世界开始演"));
    Director->Flush();
    Check(Director->NumFrames() == 3 && Fire && Fire->HasState(DarkroomTags::Lit) && Flow->Tries.Num() == 1 && !Flow->Tries[0].bAccept,
        TEXT("花瓶 → 壁炉（信还在手上）：烧掉，世界退回去"));

    // 对的三步。
    Flow->ComposeDirect(DarkroomNoun::Letter, DarkroomNoun::Fire); Director->Flush();
    Check(Director->NumFrames() == 4 && Director->GetStory().bLetterBurnt, TEXT("信 → 壁炉：成立，多出一帧"));
    Check(Director->ClueFor(Film->Find(PropLetter)) == 1, TEXT("火里的信是一条新线索"));
    Flow->ComposeDirect(DarkroomNoun::Vase, DarkroomNoun::Fire); Director->Flush();
    Check(Director->NumFrames() == 5 && !Fire->HasState(DarkroomTags::Lit) && Film->Find(PropFireback)->HasState(DarkroomTags::Revealed),
        TEXT("花瓶 → 壁炉：火灭了，背板露出来"));
    Flow->ComposeDirect(DarkroomNoun::Bust, DarkroomNoun::Portrait); Director->Flush();
    Check(Director->IsReelComplete() && Director->NumFrames() == DarkroomReel::TotalFrames && Director->GetFrame() == DarkroomReel::TotalFrames - 1,
        TEXT("胸像 → 画像：片尾自己接上，八帧齐了"));
    Check(Film->Find(PropLady)->IsHidden() && !Film->Find(PropPursuer)->IsHidden(), TEXT("最后一帧：屋里没有她，只有撞门进来的人"));
    Check(!Flow->ComposeDirect(DarkroomNoun::Lady, DarkroomNoun::Door), TEXT("补全以后不能再显影"));
    Step(-1); Check(Director->GetFrame() == DarkroomReel::TotalFrames - 2, TEXT("补全以后还能倒回去看"));

    // 笔记本。
    Check(Flow->SetNotebookOpen(true) && !Flow->CanMove(), TEXT("打开笔记本时人站住"));
    Flow->ClickEntry(0); Flow->ClickSlot(0);
    Check(Flow->Slots[0] == 0 && Flow->Selected == INDEX_NONE, TEXT("点照片，再点空格，照片放进去"));
    Flow->SetNotebookOpen(false);

    // 回到现在：用那一夜学到的事打开壁炉。
    Check(Flow->ToggleEra() && Flow->Era == EDarkroomEra::Reality, TEXT("回到现在"));
    ADarkroomProp* Fireback = Reality->Find(PropFireback);
    ADarkroomProp* Bust = Reality->Find(PropBust);
    Fireback->OnUse();
    Check(!Flow->bFirebackOpen && Flow->FirebackFails == 1, TEXT("胸像没转对，背板推不动"));
    const FString SaidBeforeTurning = Flow->GetSubtitle().ToString();
    for (int32 Turn = 0; Turn < 3; ++Turn) { Bust->OnUse(); Bust->Tick(1.f); }
    Check(FMath::Abs(FMath::FindDeltaAngleDegrees(Bust->GetFacingYaw(), BustPortraitYaw)) < 1.f, TEXT("转三下，胸像朝着画像"));
    Check(Flow->GetSubtitle().ToString() == SaidBeforeTurning, TEXT("现在转对了胸像也没有任何提示"));
    // 放着不管：过一会儿它自己转回去，这时再推背板就推不动了。
    Bust->Tick(10.f); Bust->Tick(10.f);
    Check(FMath::Abs(FMath::FindDeltaAngleDegrees(Bust->GetFacingYaw(), BustRestYaw)) < 1.f && Flow->BustReturns == 1, TEXT("胸像停一会儿就自己转回原位"));
    Fireback->OnUse();
    Check(!Flow->bFirebackOpen && Flow->FirebackFails == 2, TEXT("胸像转回去以后，背板又推不动了"));
    for (int32 Turn = 0; Turn < 3; ++Turn) { Bust->OnUse(); Bust->Tick(1.f); }
    Bust->Tick(1.f);
    Check(FMath::Abs(FMath::FindDeltaAngleDegrees(Bust->GetFacingYaw(), BustPortraitYaw)) < 1.f, TEXT("转完一秒，胸像还朝着画像"));
    Fireback->OnUse(); Fireback->Tick(5.f);
    Check(Flow->bFirebackOpen, TEXT("趁胸像还朝着画像去推，背板开了"));
    Flow->CheckPassage(Reality->ToWorld(FVector(PassageX - 40.f, 0, 90)));
    Check(!Flow->bSliceComplete, TEXT("没走进去不算完"));
    Flow->CheckPassage(Reality->ToWorld(FVector(PassageX + 40.f, 0, 90)));
    Check(Flow->bSliceComplete, TEXT("走进暗道，原型结束"));

    UE_LOG(LogHeritage, Display, TEXT("%s"), *Flow->BuildReport(true));
    return Finish();
}

// ---------------- 调试命令 ----------------
namespace
{
    UDarkroomFlowSubsystem* DebugFlow(UWorld* World)
    {
        UDarkroomFlowSubsystem* Flow = World ? World->GetSubsystem<UDarkroomFlowSubsystem>() : nullptr;
        if (Flow && Flow->IsReady()) { return Flow; }
        // 从编辑器的控制台敲命令时给的是编辑器世界：自己去找正在玩的那个。
        if (GEngine)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* Candidate = Context.World();
                if (Candidate && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game))
                {
                    Flow = Candidate->GetSubsystem<UDarkroomFlowSubsystem>();
                    if (Flow && Flow->IsReady()) { return Flow; }
                }
            }
        }
        return nullptr;
    }
    ADarkroomCharacter* DebugPlayer(UDarkroomFlowSubsystem* Flow)
    {
        return Flow ? Cast<ADarkroomCharacter>(UGameplayStatics::GetPlayerCharacter(Flow->GetWorld(), 0)) : nullptr;
    }
    float DebugArg(const TArray<FString>& Args, int32 Index, float Default)
    {
        return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : Default;
    }

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdEra(TEXT("Darkroom.Era"), TEXT("拿到胶卷并进出过去"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (UDarkroomFlowSubsystem* Flow = DebugFlow(World)) { Flow->CollectReel(); Flow->Director->Flush(); Flow->ToggleEra(); }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdFrame(TEXT("Darkroom.Frame"), TEXT("Darkroom.Frame N：跳到第 N 帧（从 0 数）"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            if (UDarkroomFlowSubsystem* Flow = DebugFlow(World)) { Flow->Director->Flush(); Flow->Director->JumpTo(int32(DebugArg(Args, 0, 0))); }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdHold(TEXT("Darkroom.Hold"), TEXT("Darkroom.Hold 1 / -1 / 0：按住正放 / 倒放 / 松手"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            if (ADarkroomCharacter* Player = DebugPlayer(DebugFlow(World)))
            {
                const int32 Dir = int32(DebugArg(Args, 0, 0));
                Player->SetHeld(1, Dir > 0); Player->SetHeld(-1, Dir < 0);
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdFlush(TEXT("Darkroom.Flush"), TEXT("把正在进行的过渡 / 演出直接走完"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (UDarkroomFlowSubsystem* Flow = DebugFlow(World)) { Flow->Director->Flush(); }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdTp(TEXT("Darkroom.Tp"), TEXT("Darkroom.Tp X Y [Yaw] [Pitch]：站到房间坐标的某处"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UDarkroomFlowSubsystem* Flow = DebugFlow(World);
            if (ADarkroomCharacter* Player = DebugPlayer(Flow))
            {
                Player->TeleportTo(Flow->CurrentRoom()->ToWorld(FVector(DebugArg(Args, 0, 0), DebugArg(Args, 1, 0), 90)), Player->GetActorRotation(), false, true);
                if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
                {
                    PC->SetControlRotation(FRotator(DebugArg(Args, 3, 0), DebugArg(Args, 2, PC->GetControlRotation().Yaw), 0));
                }
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdLookAt(TEXT("Darkroom.LookAt"), TEXT("Darkroom.LookAt 道具名：把视线对准一个道具"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UDarkroomFlowSubsystem* Flow = DebugFlow(World);
            ADarkroomCharacter* Player = DebugPlayer(Flow);
            ADarkroomProp* Prop = Player && Args.Num() > 0 ? Flow->CurrentRoom()->Find(FName(*Args[0])) : nullptr;
            if (APlayerController* PC = Prop ? Cast<APlayerController>(Player->GetController()) : nullptr)
            {
                FVector Origin, Extent;
                Prop->GetActorBounds(true, Origin, Extent);
                PC->SetControlRotation((Origin - Player->PhotoCapture->GetComponentLocation()).Rotation());
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdShoot(TEXT("Darkroom.Shoot"), TEXT("左键：拍照"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (ADarkroomCharacter* Player = DebugPlayer(DebugFlow(World))) { Player->Shoot(); }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdUse(TEXT("Darkroom.Use"), TEXT("E：使用准星对着的东西"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (ADarkroomCharacter* Player = DebugPlayer(DebugFlow(World))) { Player->Use(); }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdScanAll(TEXT("Darkroom.ScanAll"), TEXT("把这一帧能拍的都拍了（镜头自己对过去）"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            UDarkroomFlowSubsystem* Flow = DebugFlow(World);
            ADarkroomCharacter* Player = DebugPlayer(Flow);
            if (!Player || Flow->Era != EDarkroomEra::Film) { return; }
            Flow->Director->Flush();
            for (const int32 ClueIndex : Flow->Director->CluesHere())
            {
                const FDarkroomClue& Clue = DarkroomReel::Clues()[ClueIndex];
                ADarkroomProp* Prop = Flow->FilmRoom->Find(Clue.PropId);
                if (!Prop || Flow->HasScanned(Clue.Id)) { continue; }
                FVector Origin, Extent;
                Prop->GetActorBounds(true, Origin, Extent);
                Player->PhotoCapture->SetWorldRotation((Origin - Player->PhotoCapture->GetComponentLocation()).Rotation());
                Flow->Scan(ClueIndex, Player->TakePhoto());
            }
            Player->PhotoCapture->SetRelativeRotation(FRotator::ZeroRotator);
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdNotebook(TEXT("Darkroom.Notebook"), TEXT("Tab：打开 / 合上笔记本"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (UDarkroomFlowSubsystem* Flow = DebugFlow(World)) { Flow->Director->Flush(); Flow->SetNotebookOpen(!Flow->bNotebookOpen); }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdClick(TEXT("Darkroom.Click"), TEXT("Darkroom.Click X Y：把笔尖挪到页面上的某处并点一下"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UDarkroomFlowSubsystem* Flow = DebugFlow(World);
            if (ADarkroomCharacter* Player = DebugPlayer(Flow))
            {
                if (Args.Num() >= 2) { Flow->Cursor = FVector2D(DebugArg(Args, 0, 640), DebugArg(Args, 1, 400)); }
                Player->ClickNotebook();
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdCompose(TEXT("Darkroom.Compose"), TEXT("Darkroom.Compose 什么 哪里：直接显影（Letter Fire / Vase Fire / Bust Portrait / Lady Door …）"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            UDarkroomFlowSubsystem* Flow = DebugFlow(World);
            if (Flow && Args.Num() >= 2) { Flow->SetNotebookOpen(false); Flow->Director->Flush(); Flow->ComposeDirect(FName(*Args[0]), FName(*Args[1])); }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdSolve(TEXT("Darkroom.Solve"), TEXT("把胶卷直接补全"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            UDarkroomFlowSubsystem* Flow = DebugFlow(World);
            if (!Flow || Flow->Era != EDarkroomEra::Film) { return; }
            Flow->SetNotebookOpen(false); Flow->Director->Flush();
            if (!Flow->Director->GetStory().bLetterBurnt) { Flow->ComposeDirect(DarkroomNoun::Letter, DarkroomNoun::Fire); Flow->Director->Flush(); }
            if (!Flow->Director->GetStory().bFireOut) { Flow->ComposeDirect(DarkroomNoun::Vase, DarkroomNoun::Fire); Flow->Director->Flush(); }
            if (!Flow->Director->GetStory().bBustTurned) { Flow->ComposeDirect(DarkroomNoun::Bust, DarkroomNoun::Portrait); Flow->Director->Flush(); }
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdLang(TEXT("Darkroom.Lang"), TEXT("Darkroom.Lang [zh|en]：切换界面和文本的语言；不带参数就在中英之间来回切"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
        {
            bool bEnglish = !DarkroomLanguage::IsEnglish();
            if (Args.Num() > 0) { bEnglish = Args[0].StartsWith(TEXT("en"), ESearchCase::IgnoreCase); }
            DarkroomLanguage::Set(bEnglish);
            if (UDarkroomFlowSubsystem* Flow = DebugFlow(World)) { Flow->OnLanguageChanged(); }
            UE_LOG(LogHeritage, Display, TEXT("Darkroom language: %s"), bEnglish ? TEXT("English") : TEXT("中文"));
        }));

    FAutoConsoleCommandWithWorldAndArgs DarkroomCmdReport(TEXT("Darkroom.Report"), TEXT("把试玩记录打到日志里"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
        {
            if (UDarkroomFlowSubsystem* Flow = DebugFlow(World)) { UE_LOG(LogHeritage, Display, TEXT("\n%s"), *Flow->BuildReport(true)); }
        }));
}
