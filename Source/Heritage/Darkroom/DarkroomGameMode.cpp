#include "DarkroomGameMode.h"
#include "DarkroomCharacter.h"
#include "DarkroomDirector.h"
#include "DarkroomFlowSubsystem.h"
#include "DarkroomHUD.h"
#include "DarkroomReel.h"
#include "DarkroomRoom.h"
#include "Heritage.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

ADarkroomGameMode::ADarkroomGameMode()
{
    DefaultPawnClass = ADarkroomCharacter::StaticClass();
    HUDClass = ADarkroomHUD::StaticClass();
}

void ADarkroomGameMode::StartPlay()
{
    UWorld* World = GetWorld();
    // 地图是空的，两间书房都在这里现盖：现在的一间，胶卷里的一间，隔开 40 米。
    const auto SpawnRoom = [World](EDarkroomEra Era, FVector Origin)
    {
        ADarkroomRoom* Room = World->SpawnActor<ADarkroomRoom>(ADarkroomRoom::StaticClass(), FTransform(Origin));
        Room->Era = Era;
        Room->Build();
        return Room;
    };
    ADarkroomRoom* Reality = SpawnRoom(EDarkroomEra::Reality, FVector::ZeroVector);
    ADarkroomRoom* Film = SpawnRoom(EDarkroomEra::Film, FVector(0, 4000, 0));
    ADarkroomDirector* Director = World->SpawnActor<ADarkroomDirector>(ADarkroomDirector::StaticClass(), FTransform::Identity);
    Director->Init(Film);
    UDarkroomFlowSubsystem* Flow = World->GetSubsystem<UDarkroomFlowSubsystem>();
    if (Flow) { Flow->Register(Reality, Film, Director); }

    Super::StartPlay();

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->GetPawn())
        {
            PC->GetPawn()->TeleportTo(Reality->ToWorld(DarkroomLayout::PlayerSpawn), FRotator::ZeroRotator, false, true);
            PC->SetControlRotation(FRotator(-4, 0, 0));
        }
    }
    if (Flow) { Flow->Begin(); }

    if (FParse::Param(FCommandLine::Get(), TEXT("DarkroomSelfTest")))
    {
        World->GetTimerManager().SetTimerForNextTick([World]()
        {
            const bool bPassed = RunDarkroomSelfTest(World);
            FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
        });
    }

    // 打包版没有编辑器可以截图：带 -DarkroomShot=文件名 启动，开场 5 秒后连界面截一张图，再过 2 秒退出。
    FString ShotFile;
    if (FParse::Value(FCommandLine::Get(), TEXT("DarkroomShot="), ShotFile))
    {
        FTimerHandle ShotTimer, QuitTimer;
        World->GetTimerManager().SetTimer(ShotTimer, [ShotFile]() { FScreenshotRequest::RequestScreenshot(ShotFile, true, false); }, 5.f, false);
        World->GetTimerManager().SetTimer(QuitTimer, []() { FPlatformMisc::RequestExit(false); }, 7.f, false);
    }
}
