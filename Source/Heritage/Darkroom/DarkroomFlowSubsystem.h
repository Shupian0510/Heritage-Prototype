// 流程：现在/过去两边跑、拿没拿到胶卷、笔记本里记了什么、拼了什么、试玩记录。
// 对应 Richard 原型的 HeritageGameFlowSubsystem + HeritageArchiveSubsystem，合成了一个，
// 去掉了档案馆的剪辑台、固定机位、地图。
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DarkroomTypes.h"
#include "DarkroomFlowSubsystem.generated.h"

class ADarkroomRoom;
class ADarkroomDirector;
class ADarkroomProp;
class UTextureRenderTarget2D;

UCLASS()
class UDarkroomFlowSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Deinitialize() override;

    void Register(ADarkroomRoom* InReality, ADarkroomRoom* InFilm, ADarkroomDirector* InDirector);
    void Begin();
    bool IsReady() const { return RealityRoom && FilmRoom && Director; }
    ADarkroomRoom* CurrentRoom() const { return Era == EDarkroomEra::Film ? FilmRoom : RealityRoom; }

    // 独白和闪光
    void Say(const FDarkroomText& Line, bool bQueue = false);
    FText GetSubtitle() const;
    void Flash(FLinearColor Color, float Seconds);
    FLinearColor GetFlash() const;

    // 现在
    void CollectReel();
    void NotifyBustTurned(float Yaw);
    void NotifyBustReturned();
    void TryFireback(ADarkroomProp* Fireback);
    void CheckPassage(const FVector& PawnLocation);

    // 进出过去
    bool CanToggleEra() const;
    bool ToggleEra();

    // 拍照
    bool HasScanned(FName ClueId) const;
    bool Scan(int32 ClueIndex, UTextureRenderTarget2D* Photo);
    void NoteEmptyShot();

    // 笔记本：点一张照片选中，再点一个空格放进去。
    bool SetNotebookOpen(bool bOpen);
    void ClickEntry(int32 Index);
    void ClickSlot(int32 Slot);
    bool CanDevelop() const;
    bool Develop();
    bool ComposeDirect(FName Subject, FName Place);
    void OnAttemptResolved(bool bAccept, const FDarkroomText& Line);
    void OnLanguageChanged();
    void OnReelComplete();

    bool CanMove() const { return !bNotebookOpen && !bSliceComplete; }
    float SecondsSinceComplete() const;

    void Log(const FString& What);
    FString BuildReport(bool bWithEvents) const;
    void SaveReport();

    EDarkroomEra Era = EDarkroomEra::Reality;
    bool bOwnsReel = false;
    bool bNotebookOpen = false;
    bool bFirebackOpen = false;
    bool bSliceComplete = false;
    bool bQuiet = false;                        // 自测时不写试玩记录

    UPROPERTY() TObjectPtr<ADarkroomRoom> RealityRoom;
    UPROPERTY() TObjectPtr<ADarkroomRoom> FilmRoom;
    UPROPERTY() TObjectPtr<ADarkroomDirector> Director;
    UPROPERTY() TArray<FDarkroomEntry> Entries;

    int32 Selected = INDEX_NONE;                // 选中的照片
    int32 Slots[2] = { INDEX_NONE, INDEX_NONE };    // 【什么】【哪里】
    FVector2D Cursor = FVector2D(640, 400);     // 笔记本上的笔尖（页面像素）
    FDarkroomText LastResult;
    TArray<FDarkroomTry> Tries;
    int32 Revision = 0;                         // 笔记本内容变了就加一，界面据此重排

    // 试玩记录
    int32 EmptyShots = 0;
    int32 NotebookOpens = 0;
    int32 BustTurns = 0;
    int32 BustReturns = 0;                      // 胸像自己转回去了几次（= 有几次没赶上）
    int32 FirebackFails = 0;
    int32 EraTrips = 0;
    int32 PhotosBeforeFirstTry = INDEX_NONE;
private:
    struct FSaid { FDarkroomText Line; double Start = 0; double End = 0; };
    double Now() const;
    TArray<FSaid> Said;
    FLinearColor FlashColor = FLinearColor::Transparent;
    double FlashStart = 0;
    double FlashSeconds = 0;
    double StartTime = 0;
    double CompleteTime = 0;
    FDarkroomTry PendingTry;
    TArray<FString> Events;
    bool bSaved = false;
    bool bStarted = false;
};
