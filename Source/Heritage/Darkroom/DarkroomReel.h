// 胶卷「书房，夜」：已知的三帧、线索、位置，以及"拼出来的东西世界怎么演"的规则。
// 对应 Richard 原型的 HeritageReelData，内容全部重写。
#pragma once
#include "DarkroomTypes.h"

// 房间坐标（原点在地板正中，X 东 Y 北 Z 上，单位厘米）。房间和胶卷共用。
namespace DarkroomLayout
{
    inline const FVector RoomSize(800, 600, 350);
    inline constexpr float Wall = 20.f;
    inline const FVector PlayerSpawn(-300, 0, 100);

    // 壁炉在东墙正中，大得能走进一个人；背板后面是暗道。
    inline const FVector FireSlab(355, 0, 5);
    inline const FVector FirebackClosed(384, 0, 104);
    inline const FVector FirebackAjar(397, -1, 104);
    inline const FRotator FirebackAjarRot(0, -12, 0);
    inline const FVector FirebackOpen(445, -51, 104);
    inline const FRotator FirebackOpenRot(0, -80, 0);
    inline constexpr float PassageX = 420.f;            // 走过这条线就算进了暗道

    // 门在南墙正中。
    inline const FVector DoorClosed(0, -296, 106);
    inline const FVector DoorOpen(-65, -242, 106);
    inline const FRotator DoorOpenRot(0, 100, 0);
    inline const FVector BoltOn(42, -289, 115);
    inline const FVector BoltBroken(30, -250, 3);

    // 窗在西墙正中。
    inline const FVector WindowPos(-396, 0, 170);
    inline const FVector LanternOutside(-470, 40, 150);

    // 书桌靠北墙。
    inline const FVector DeskTop(-120, 240, 78);
    inline const FVector LetterOnDesk(-135, 222, 81.5);
    inline const FVector VaseOnDesk(-62, 248, 101);
    inline const FVector ReelOnDesk(-172, 232, 84);

    // 胸像在东南，画像在它正北的墙上。
    inline const FVector BustPos(250, -130, 111);
    inline const FVector PortraitPos(250, 287, 175);
    inline constexpr float BustRestYaw = 180.f;
    inline constexpr float BustPortraitYaw = 90.f;

    // 道具的名字：快照、线索、房间都用它找道具。
    inline const FName PropLady(TEXT("Lady"));
    inline const FName PropLetter(TEXT("Letter"));
    inline const FName PropDoor(TEXT("Door"));
    inline const FName PropBolt(TEXT("Bolt"));
    inline const FName PropWindow(TEXT("Window"));
    inline const FName PropLantern(TEXT("Lantern"));
    inline const FName PropFireplace(TEXT("Fireplace"));
    inline const FName PropFireback(TEXT("Fireback"));
    inline const FName PropVase(TEXT("Vase"));
    inline const FName PropFlowers(TEXT("Flowers"));
    inline const FName PropBust(TEXT("Bust"));
    inline const FName PropPortrait(TEXT("Portrait"));
    inline const FName PropDesk(TEXT("Desk"));
    inline const FName PropPursuer(TEXT("Pursuer"));
    inline const FName PropReel(TEXT("Reel"));

    inline const FVector HearthLetter(345, 12, 16);
    inline const FVector HearthVase(345, -30, 22);
    inline const FVector FloorFlowers(285, 55, 5);
}

namespace DarkroomReel
{
    inline constexpr int32 KnownFrames = 3;
    inline constexpr int32 GapFrames = 3;
    inline constexpr int32 TailFrames = 2;
    inline constexpr int32 TotalFrames = KnownFrames + GapFrames + TailFrames;

    TArray<FDarkroomFrame> BuildKnown();
    TArray<FDarkroomFrame> BuildTail();
    TArray<FDarkroomSnapshot> RealityKeys();
    const TArray<FDarkroomClue>& Clues();
    const FDarkroomPlace* FindPlace(FName Noun);
    FDarkroomText NounName(FName Noun);
    FDarkroomText FinalLine();

    // 核心规则：在现有的故事状态下，把【什么】拼到【哪里】，世界怎么演、算不算数。
    FDarkroomAttempt Resolve(const FDarkroomStory& Story, FName Subject, FName Place);
}
