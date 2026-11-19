// 原型 3「暗房」的数据类型。
// 快照这一套改自 Richard 原型的 HeritageTypes.h：留下"每帧只写变了的东西，往后顺延"，
// 去掉了事实记录、取景编辑、刺激（Stimulus）这些"亲手做动作"才用得上的类型。
#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "DarkroomTypes.generated.h"

class UTextureRenderTarget2D;

// 当前用哪种语言。控制台 Darkroom.Lang [zh|en] 切换；存在模块里，F5 重来或重开 PIE 都还在。
namespace DarkroomLanguage
{
    bool IsEnglish();
    void Set(bool bEnglish);
}

// 一句话的中英两个版本。存的时候两种都存，显示的时候再挑：
// 这样切换语言时，已经记进笔记本的、正说到一半的那句也会跟着变。
struct FDarkroomText
{
    FText Zh;
    FText En;
    FDarkroomText() = default;
    FDarkroomText(const TCHAR* InZh, const TCHAR* InEn) : Zh(FText::FromString(InZh)), En(FText::FromString(InEn)) {}
    FText Get() const { return DarkroomLanguage::IsEnglish() ? En : Zh; }
    bool IsEmpty() const { return Zh.IsEmpty(); }
    bool HasBoth() const { return !Zh.IsEmpty() && !En.IsEmpty(); }
};

#define DR_TEXT(Zh, En) FDarkroomText(TEXT(Zh), TEXT(En))

UENUM() enum class EDarkroomEra : uint8 { Reality, Film };
UENUM() enum class EDarkroomShape : uint8 { Cube, Sphere, Cylinder, Cone };
UENUM() enum class EDarkroomKind : uint8 { Plain, Fireplace, Lantern, Vase, Letter, Door, Bust, Fireback, Reel };

namespace DarkroomTags
{
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Lit);       // 壁炉烧着；窗外有灯
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Locked);    // 门闩着
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shaking);   // 有人在撞门
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Filled);    // 花瓶里有花有水
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Whole);     // 信还完好
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Burning);   // 信在火里
    UE_DECLARE_GAMEPLAY_TAG_EXTERN(Revealed);  // 火灭了，背板看得见
}

// 名词：照片拍到的是"什么"。笔记本里拼的就是两个名词。
namespace DarkroomNoun
{
    inline const FName Lady(TEXT("Lady"));
    inline const FName Letter(TEXT("Letter"));
    inline const FName Vase(TEXT("Vase"));
    inline const FName Bust(TEXT("Bust"));
    inline const FName Fire(TEXT("Fire"));
    inline const FName Door(TEXT("Door"));
    inline const FName Window(TEXT("Window"));
    inline const FName Portrait(TEXT("Portrait"));
    inline const FName Desk(TEXT("Desk"));
    inline const FName Pursuer(TEXT("Pursuer"));
}

// 一个道具在某一帧的样子（位置是房间坐标）。
struct FDarkroomSnapshot
{
    FName Id;
    FVector Location = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FGameplayTagContainer State;
    bool bHidden = false;
    bool bHasVia = false;               // 从上一帧过来时先经过这里
    FVector Via = FVector::ZeroVector;

    FDarkroomSnapshot& Tag(FGameplayTag InTag) { State.AddTag(InTag); return *this; }
    FDarkroomSnapshot& Hide() { bHidden = true; return *this; }
    FDarkroomSnapshot& Through(FVector Point) { bHasVia = true; Via = Point; return *this; }
};

struct FDarkroomFrame
{
    TArray<FDarkroomSnapshot> Keys;     // 只写这一帧变了的道具
};

// 一条线索：某个道具带着某个状态时可以拍。
struct FDarkroomClue
{
    FName Id;
    FName Noun;
    FName PropId;
    FGameplayTag Requires;              // 空 = 只要道具看得见
    bool bKey = false;                  // 黄豆
    FDarkroomText Title;
    FDarkroomText Note;                 // 记进笔记的话
    FDarkroomText Line;                 // 角色说出口的想法
};

struct FDarkroomPlace
{
    FName Noun;
    FVector2D Stand = FVector2D::ZeroVector;    // 人站哪
    float Yaw = 0.f;
    FVector Put = FVector::ZeroVector;          // 东西放哪
    float BustYaw = 0.f;                        // 胸像朝这边时的角度
};

// 夫人那一夜做过的三件事。先后只有一条约束：信得在火灭之前烧。
struct FDarkroomStory
{
    bool bLetterBurnt = false;
    bool bFireOut = false;
    bool bBustTurned = false;
    int32 Solved() const { return int32(bLetterBurnt) + int32(bFireOut) + int32(bBustTurned); }
    bool IsComplete() const { return Solved() == 3; }
};

// 一次"显影"：世界照玩家拼的演一遍。对错都有 Steps，都会演。
struct FDarkroomAttempt
{
    bool bAccept = false;
    TArray<TArray<FDarkroomSnapshot>> Steps;
    FDarkroomText Line;
    FName Fx;
    FDarkroomStory After;
};

struct FDarkroomTry
{
    FName Subject;
    FName Place;
    bool bAccept = false;
};

USTRUCT()
struct FDarkroomEntry
{
    GENERATED_BODY()
    FName ClueId;
    FName Noun;
    FDarkroomText Title;
    FDarkroomText Note;
    bool bKey = false;
    int32 Frame = 0;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Photo;
};
