#include "DarkroomDirector.h"
#include "DarkroomRoom.h"
#include "DarkroomProp.h"
#include "DarkroomReel.h"
#include "DarkroomFlowSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"

namespace
{
    const float DirScrubSeconds = 1.5f;     // 走过一帧要多久
    const float DirStepSeconds = 1.25f;     // 演一步要多久
    const float DirHoldAccept = 1.2f;
    const float DirHoldReject = 2.0f;
    const float DirBurnSeconds = 0.9f;
    const float DirTailSeconds = 2.2f;
    const float DirTailHold = 1.3f;
    const float DirFxAt = 0.6f;             // 状态在过渡的这个位置切换，效果也在这时响

    const FLinearColor DirFillWarm(1.f, 0.72f, 0.45f);
    const FLinearColor DirFillCold(0.42f, 0.55f, 0.95f);
    const float DirFillWarmCandelas = 2.2f;
    const float DirFillColdCandelas = 11.f;     // 火灭了也得看得清：接下来转胸像、开背板都发生在这片冷光里
}

ADarkroomDirector::ADarkroomDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADarkroomDirector::Init(ADarkroomRoom* InRoom)
{
    Room = InRoom;
    Frames = DarkroomReel::BuildKnown();
    Story = FDarkroomStory();
    Current = 0; Phase = EDarkroomPhase::Idle; bReelComplete = false;
    ApplyState(Effective(0));
    UpdateLight(0.f, true);
}

void ADarkroomDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Advance(DeltaSeconds);
    UpdateLight(DeltaSeconds, false);
}

// 每帧只写了变过的道具；某一帧的完整样子 = 从头到这一帧依次盖上去。
ADarkroomDirector::FStateMap ADarkroomDirector::Effective(int32 Index) const
{
    FStateMap State;
    for (int32 FrameIndex = 0; FrameIndex <= Index && Frames.IsValidIndex(FrameIndex); ++FrameIndex)
    {
        for (const FDarkroomSnapshot& Key : Frames[FrameIndex].Keys)
        {
            FDarkroomSnapshot& Slot = State.FindOrAdd(Key.Id);
            Slot = Key;
            if (FrameIndex != Index) { Slot.bHasVia = false; }     // 绕路只属于进入那一帧的过渡
        }
    }
    return State;
}

void ADarkroomDirector::ApplyState(const FStateMap& State)
{
    if (!Room) { return; }
    const FTransform RoomTransform = Room->GetActorTransform();
    for (const auto& Pair : State)
    {
        if (ADarkroomProp* Prop = Room->Find(Pair.Key)) { Prop->Restore(Pair.Value, RoomTransform); }
    }
}

void ADarkroomDirector::ApplyBlend(const FStateMap& A, const FStateMap& B, float InU)
{
    if (!Room) { return; }
    const FTransform RoomTransform = Room->GetActorTransform();
    const float Alpha = FMath::Clamp(InU, 0.f, 1.f);
    const float Eased = FMath::SmoothStep(0.f, 1.f, Alpha);
    for (const auto& Pair : B)
    {
        ADarkroomProp* Prop = Room->Find(Pair.Key);
        if (!Prop) { continue; }
        const FDarkroomSnapshot& End = Pair.Value;
        const FDarkroomSnapshot* Found = A.Find(Pair.Key);
        const FDarkroomSnapshot& Start = Found ? *Found : End;

        FDarkroomSnapshot Mix = End;
        if (End.bHasVia)
        {
            Mix.Location = Eased < 0.5f ? FMath::Lerp(Start.Location, End.Via, Eased * 2.f) : FMath::Lerp(End.Via, End.Location, (Eased - 0.5f) * 2.f);
        }
        else { Mix.Location = FMath::Lerp(Start.Location, End.Location, Eased); }
        Mix.Rotation = FQuat::Slerp(Start.Rotation.Quaternion(), End.Rotation.Quaternion(), Eased).Rotator();
        // 出现的东西早一点出来，消失的东西晚一点走，这样过渡里看得见它在动。
        if (Start.bHidden != End.bHidden) { Mix.bHidden = End.bHidden ? Alpha >= 0.85f : Alpha < 0.15f; }
        Mix.State = Alpha >= DirFxAt ? End.State : Start.State;
        Prop->Restore(Mix, RoomTransform);
    }
}

void ADarkroomDirector::BeginScrub(int32 Target)
{
    From = Current; To = Target; U = 0.f; Travel = 1.f;
    StateLo = Effective(FMath::Min(From, To));
    StateHi = Effective(FMath::Max(From, To));
    Phase = EDarkroomPhase::Scrub;
}

float ADarkroomDirector::GetPlayhead() const
{
    if (Phase == EDarkroomPhase::Scrub || Phase == EDarkroomPhase::Tail) { return FMath::Lerp(float(From), float(To), FMath::Clamp(U, 0.f, 1.f)); }
    return float(Current);
}

void ADarkroomDirector::JumpTo(int32 Frame)
{
    if (Phase != EDarkroomPhase::Idle || Frames.Num() == 0) { return; }
    Current = FMath::Clamp(Frame, 0, Frames.Num() - 1);
    ApplyState(Effective(Current));
    UpdateLight(0.f, true);
}

void ADarkroomDirector::Flush()
{
    const int32 KeepDir = Dir; Dir = 0;
    for (int32 Guard = 0; Guard < 4000 && Phase != EDarkroomPhase::Idle; ++Guard) { Advance(0.1f); }
    Dir = KeepDir;
    UpdateLight(0.f, true);
}

void ADarkroomDirector::Advance(float DeltaSeconds)
{
    UDarkroomFlowSubsystem* Flow = GetWorld() ? GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>() : nullptr;
    switch (Phase)
    {
    case EDarkroomPhase::Idle:
        if (Dir != 0 && Frames.IsValidIndex(Current + Dir))
        {
            if (Dir < 0) { ++Rewinds; }
            BeginScrub(Current + Dir);
        }
        break;

    case EDarkroomPhase::Scrub:
    {
        // 按着哪边就往哪边走，中途可以反向；松手会把这一帧走完。
        const int32 Along = To > From ? 1 : -1;
        if (Dir != 0) { Travel = Dir == Along ? 1.f : -1.f; }
        U += Travel * DeltaSeconds / DirScrubSeconds;
        if (U >= 1.f || U <= 0.f)
        {
            Current = U >= 1.f ? To : From;
            ApplyState(Effective(Current));
            Phase = EDarkroomPhase::Idle;
        }
        else { ApplyBlend(StateLo, StateHi, To > From ? U : 1.f - U); }
        break;
    }

    case EDarkroomPhase::Attempt:
    {
        const bool bLastStep = StepIndex >= StepStates.Num() - 2;
        U += DeltaSeconds / DirStepSeconds;
        if (bLastStep && !bFxPlayed && U >= DirFxAt) { bFxPlayed = true; PlayFx(Pending.Fx); }
        if (U < 1.f) { ApplyBlend(StepStates[StepIndex], StepStates[StepIndex + 1], U); break; }
        ApplyState(StepStates[StepIndex + 1]);
        ++StepIndex; U = 0.f;
        if (StepIndex >= StepStates.Num() - 1)
        {
            Phase = EDarkroomPhase::Hold;
            Timer = Pending.bAccept ? DirHoldAccept : DirHoldReject;
            if (Flow) { Flow->Say(Pending.Line); }
        }
        break;
    }

    case EDarkroomPhase::Hold:
        Timer -= DeltaSeconds;
        if (Timer <= 0.f)
        {
            if (Pending.bAccept) { Commit(); }
            else { Phase = EDarkroomPhase::Burn; Timer = 0.f; bBurnRestored = false; }
        }
        break;

    case EDarkroomPhase::Burn:
    {
        // 烧到最亮的那一下把世界换回去。
        Timer += DeltaSeconds;
        const float Alpha = Timer / DirBurnSeconds;
        BurnAlpha = Alpha < 0.45f ? Alpha / 0.45f : FMath::Max(0.f, 1.f - (Alpha - 0.45f) / 0.55f);
        if (!bBurnRestored && Alpha >= 0.45f) { bBurnRestored = true; ApplyState(Effective(Current)); }
        if (Alpha >= 1.f)
        {
            BurnAlpha = 0.f; Phase = EDarkroomPhase::Idle;
            if (Flow) { Flow->OnAttemptResolved(false, Pending.Line); }
        }
        break;
    }

    case EDarkroomPhase::Tail:
        if (Timer > 0.f) { Timer -= DeltaSeconds; break; }
        U += DeltaSeconds / DirTailSeconds;
        if (U < 1.f) { ApplyBlend(StateLo, StateHi, U); break; }
        Current = To;
        ApplyState(Effective(Current));
        if (Current >= Frames.Num() - 1)
        {
            Phase = EDarkroomPhase::Idle; bReelComplete = true;
            if (Flow) { Flow->OnReelComplete(); }
        }
        else
        {
            From = Current; To = Current + 1; U = 0.f; Timer = DirTailHold;
            StateLo = Effective(From); StateHi = Effective(To);
        }
        break;
    }
}

// 显影：世界照玩家拼的演一遍。对的留下成为新的一帧，不对的烧掉。
bool ADarkroomDirector::Compose(FName Subject, FName Place)
{
    if (Phase != EDarkroomPhase::Idle || bReelComplete || Frames.Num() == 0) { return false; }
    UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();

    // 总是从最后一帧接着往下演。
    Current = Frames.Num() - 1;
    ApplyState(Effective(Current));
    Pending = DarkroomReel::Resolve(Story, Subject, Place);

    if (Pending.Steps.Num() == 0)
    {
        // 没有动作可演：胶片直接卡住烧掉。
        if (Flow) { Flow->Say(Pending.Line); }
        Phase = EDarkroomPhase::Burn; Timer = 0.f; bBurnRestored = true;
        return true;
    }

    StepStates.Reset();
    StepStates.Add(Effective(Current));
    for (const TArray<FDarkroomSnapshot>& Step : Pending.Steps)
    {
        FStateMap Next = StepStates.Last();
        for (auto& Pair : Next) { Pair.Value.bHasVia = false; }
        for (const FDarkroomSnapshot& Key : Step) { Next.Add(Key.Id, Key); }
        StepStates.Add(MoveTemp(Next));
    }
    StepIndex = 0; U = 0.f; bFxPlayed = false;
    Phase = EDarkroomPhase::Attempt;
    return true;
}

void ADarkroomDirector::Commit()
{
    UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();

    // 几步合成一帧。回放时要走的还是同一条路：
    // 两步都动的记下中途站过的地方，后一步才动的前半段原地等着，前一步就到位的后半段不再动。
    FDarkroomFrame NewFrame;
    TMap<FName, int32> LastStep;
    const int32 NumSteps = Pending.Steps.Num();
    for (int32 Step = 0; Step < NumSteps; ++Step)
    {
        for (const FDarkroomSnapshot& Key : Pending.Steps[Step])
        {
            FDarkroomSnapshot* Existing = NewFrame.Keys.FindByPredicate([&Key](const FDarkroomSnapshot& Other) { return Other.Id == Key.Id; });
            LastStep.Add(Key.Id, Step);
            if (!Existing)
            {
                FDarkroomSnapshot Added = Key;
                const FDarkroomSnapshot* Before = StepStates.Num() > 0 ? StepStates[0].Find(Key.Id) : nullptr;
                if (Step > 0 && Before && !Before->Location.Equals(Key.Location, 1.f)) { Added.Through(Before->Location); }
                NewFrame.Keys.Add(Added);
                continue;
            }
            const FVector Earlier = Existing->Location;
            *Existing = Key;
            if (!Earlier.Equals(Key.Location, 1.f)) { Existing->Through(Earlier); }
        }
    }
    if (NumSteps > 1)
    {
        for (FDarkroomSnapshot& Key : NewFrame.Keys)
        {
            if (!Key.bHasVia && LastStep.FindRef(Key.Id) == 0) { Key.Through(Key.Location); }
        }
    }
    Frames.Add(NewFrame);
    Current = Frames.Num() - 1;
    Story = Pending.After;
    ApplyState(Effective(Current));
    if (Flow) { Flow->OnAttemptResolved(true, Pending.Line); }

    if (Story.IsComplete())
    {
        Frames.Append(DarkroomReel::BuildTail());
        From = Current; To = Current + 1; U = 0.f; Timer = DirTailHold;
        StateLo = Effective(From); StateHi = Effective(To);
        Phase = EDarkroomPhase::Tail;
    }
    else { Phase = EDarkroomPhase::Idle; }
}

void ADarkroomDirector::PlayFx(FName Fx)
{
    UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (Fx == TEXT("Flare"))
    {
        if (ADarkroomProp* Fire = Room ? Room->Find(DarkroomLayout::PropFireplace) : nullptr) { Fire->Flare(1.4f); }
        if (Flow) { Flow->Flash(FLinearColor(1.f, 0.45f, 0.1f, 0.30f), 0.5f); }
    }
    else if (Fx == TEXT("Steam")) { if (Flow) { Flow->Flash(FLinearColor(0.85f, 0.9f, 1.f, 0.55f), 0.9f); } }
    else if (Fx == TEXT("Click")) { if (Flow) { Flow->Flash(FLinearColor(1.f, 1.f, 1.f, 0.12f), 0.2f); } }
    else if (Fx == TEXT("Rush")) { if (Flow) { Flow->Flash(FLinearColor(0.5f, 0.05f, 0.02f, 0.45f), 0.7f); } }
}

TArray<int32> ADarkroomDirector::CluesHere() const
{
    TArray<int32> Result;
    if (!Room || (Phase != EDarkroomPhase::Idle && Phase != EDarkroomPhase::Scrub)) { return Result; }
    const TArray<FDarkroomClue>& Table = DarkroomReel::Clues();
    for (int32 Index = 0; Index < Table.Num(); ++Index)
    {
        const ADarkroomProp* Prop = Room->Find(Table[Index].PropId);
        if (Prop && !Prop->IsHidden() && (!Table[Index].Requires.IsValid() || Prop->HasState(Table[Index].Requires))) { Result.Add(Index); }
    }
    return Result;
}

int32 ADarkroomDirector::ClueFor(const ADarkroomProp* Prop) const
{
    if (Phase != EDarkroomPhase::Idle || !Prop || Prop->IsHidden() || Prop->Era != EDarkroomEra::Film) { return INDEX_NONE; }
    const TArray<FDarkroomClue>& Table = DarkroomReel::Clues();
    for (int32 Index = 0; Index < Table.Num(); ++Index)
    {
        if (Table[Index].PropId == Prop->FilmId && (!Table[Index].Requires.IsValid() || Prop->HasState(Table[Index].Requires))) { return Index; }
    }
    return INDEX_NONE;
}

// 屋里的底光跟着火走：火烧着是暖的，灭了只剩窗外透进来的冷光。
void ADarkroomDirector::UpdateLight(float DeltaSeconds, bool bSnap)
{
    if (!Room || !Room->MainLight) { return; }
    const ADarkroomProp* Fire = Room->Find(DarkroomLayout::PropFireplace);
    const bool bLit = Fire && Fire->HasState(DarkroomTags::Lit);
    const FLinearColor TargetColor = bLit ? DirFillWarm : DirFillCold;
    const float TargetCandelas = bLit ? DirFillWarmCandelas : DirFillColdCandelas;
    const float Blend = bSnap ? 1.f : FMath::Clamp(DeltaSeconds * 4.f, 0.f, 1.f);
    Room->MainLight->SetIntensity(FMath::Lerp(Room->MainLight->Intensity, TargetCandelas, Blend));
    Room->MainLight->SetLightColor(FMath::Lerp(Room->MainLight->GetLightColor(), TargetColor, Blend));
}
