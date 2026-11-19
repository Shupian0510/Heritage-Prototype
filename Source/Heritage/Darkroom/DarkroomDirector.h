// 时间轴：管胶卷里那间房在第几帧、帧和帧之间怎么过渡、玩家拼出来的东西怎么演。
// 改自 Richard 原型的 HeritageFilmDirector：留下"快照往后顺延"，
// 去掉了固定机位、亲手表演、判定窗口；新增按住正放/倒放的连续过渡，和"显影"的演出。
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarkroomTypes.h"
#include "DarkroomDirector.generated.h"

class ADarkroomRoom;
class ADarkroomProp;

UENUM()
enum class EDarkroomPhase : uint8
{
    Idle,       // 停在某一帧上，可以拍照
    Scrub,      // 两帧之间
    Attempt,    // 正在演玩家拼的那一步
    Hold,       // 演完停一下，让人看清
    Burn,       // 不对：胶片烧掉，退回去
    Tail        // 三件事都对了：片尾自己接上
};

UCLASS()
class ADarkroomDirector : public AActor
{
    GENERATED_BODY()
public:
    ADarkroomDirector();
    virtual void Tick(float DeltaSeconds) override;
    void Init(ADarkroomRoom* InRoom);

    void SetTransport(int32 InDir) { Dir = FMath::Clamp(InDir, -1, 1); }   // +1 正放，-1 倒放，0 松手
    bool Compose(FName Subject, FName Place);
    void JumpTo(int32 Frame);
    void Advance(float DeltaSeconds);
    void Flush();                                   // 把正在进行的过渡直接走完（测试、调试用）

    EDarkroomPhase GetPhase() const { return Phase; }
    bool IsIdle() const { return Phase == EDarkroomPhase::Idle; }
    int32 NumFrames() const { return Frames.Num(); }
    int32 GetFrame() const { return Current; }
    float GetPlayhead() const;
    bool IsReelComplete() const { return bReelComplete; }
    const FDarkroomStory& GetStory() const { return Story; }
    float GetBurnAlpha() const { return BurnAlpha; }
    TArray<int32> CluesHere() const;                // 这一帧上能拍的线索
    int32 ClueFor(const ADarkroomProp* Prop) const; // 对着这个道具能拍到哪条

    int32 Rewinds = 0;
private:
    typedef TMap<FName, FDarkroomSnapshot> FStateMap;
    FStateMap Effective(int32 Index) const;
    void ApplyState(const FStateMap& State);
    void ApplyBlend(const FStateMap& A, const FStateMap& B, float U);
    void BeginScrub(int32 Target);
    void Commit();
    void PlayFx(FName Fx);
    void UpdateLight(float DeltaSeconds, bool bSnap);

    UPROPERTY() TObjectPtr<ADarkroomRoom> Room;
    TArray<FDarkroomFrame> Frames;
    FDarkroomStory Story;
    EDarkroomPhase Phase = EDarkroomPhase::Idle;
    int32 Dir = 0;
    int32 Current = 0;
    int32 From = 0;
    int32 To = 0;
    float U = 0.f;
    float Travel = 1.f;
    float Timer = 0.f;
    float BurnAlpha = 0.f;
    bool bBurnRestored = false;
    bool bFxPlayed = false;
    bool bReelComplete = false;
    FStateMap StateLo;
    FStateMap StateHi;

    FDarkroomAttempt Pending;
    TArray<FStateMap> StepStates;
    int32 StepIndex = 0;
};
