// =====================================================================================
// 原型 3「暗房」 —— 一次性的玩具原型，只回答一个问题。地图：/Game/Developers/Chiip03/Darkroom/Lvl_Darkroom
//
// 问题（是/否）：把"找线索"和"回答接下来发生了什么"分开，并且无论对错世界都照着演一遍，
//               玩家是不是先有猜测再提交，而不是把照片挨个试？
// 动词：走、看、正放/倒放（按住 E / Q）、拍照（左键）、在笔记本里拼（Tab）、显影；回到现在：使用（E）。
// 体验：站在那一夜的书房里，时间在身边流过；攒够线索时心里已经有答案；
//       显影以后世界照你说的演一遍，错了也看得懂为什么错。
// 为什么用 UE：答案取决于第一人称的在场感、火灭掉时整间屋的光、手里那本笔记本。
// 看什么：Saved/Darkroom/playtest_*.txt —— 第一次显影前拍了几张、成立/烧掉各几次、烧掉之后有没有回去倒放和翻笔记。
//
// 结论：是/否还没下。
// 2026-11-18 第一次试玩（Saved/Darkroom/playtest_20261118_204317.txt）：8 分 49 秒走完；第一次显影前拍了 9 张；
//   显影 7 次，成立 3、烧掉 4，烧掉的 4 次全是"夫人 → 某处"；倒放 2 次，翻笔记本 9 次。
//   试玩后的原话："玩法没问题，文本质量和交互需要打磨。"
//   同一次里，现在的胸像在拿到胶卷之前就被转到过朝着画像的位置，并且出了"咔"的提示 —— 这个提示已经去掉
//   （UDarkroomFlowSubsystem::NotifyBustTurned）。光去掉提示还不够（胸像停在那个朝向时去推背板照样推得开），
//   所以又改成：现在的胸像转过去后只停 2.5 秒就自己转回原位，得趁它还朝着画像时去推背板（ADarkroomProp::Tick）。
// 之后的打磨：文本重写并加了英文（控制台 Darkroom.Lang [zh|en]），左上角只留目标，笔记本笔尖上下方向修正。
//
// 打包（2026-11-18，Win64 Development，Builds/Darkroom/Windows/Heritage.exe）：打包版默认英文，编辑器里默认中文。
//   房间是代码按路径加载引擎资源搭出来的（/Engine/BasicShapes 下的 Cube、Sphere、Cylinder、Cone、BasicShapeMaterial，
//   以及 /Engine/EngineMaterials/EmissiveMeshMaterial），烘焙器看不到这层引用，打包时要在 -MapsToCook 里点名，否则屋子是空的。
//   启动地图靠 DefaultEngine.ini 的 GameDefaultMap。-DarkroomShot=文件名 用来给打包版截图（见 DarkroomGameMode.cpp）。
//
// 底子是 Richard 的原型（heritage-kingslayerrq，Source/Blank/Heritage）：代码里搭灰盒、两间房之间传送、
// 快照往后顺延的时间轴、代码里建输入、无界面自测。删掉了：扫描胶卷的门槛、固定机位取景、亲手表演和判定、
// 抓/扔/冲刺/跳/蹲、物理刺激、档案馆剪辑台、地图。各文件开头写了它对应 Richard 的哪一部分。
// 所有类都叫 Darkroom*，放在这个文件夹里，和模板自带的 Heritage* 类互不相干；原型之间不共用代码。
// =====================================================================================
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DarkroomGameMode.generated.h"

UCLASS()
class ADarkroomGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ADarkroomGameMode();
    virtual void StartPlay() override;
};

// 无界面自测：命令行带 -DarkroomSelfTest 时跑一遍整条流程，用退出码报结果。
bool RunDarkroomSelfTest(UWorld* World);
