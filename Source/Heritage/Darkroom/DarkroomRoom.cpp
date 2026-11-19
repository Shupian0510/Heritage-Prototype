#include "DarkroomRoom.h"
#include "DarkroomProp.h"
#include "DarkroomReel.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
    using namespace DarkroomLayout;

    // 灰盒配色。
    const FLinearColor RoomFloor(0.085f, 0.058f, 0.042f);
    const FLinearColor RoomCeiling(0.20f, 0.19f, 0.18f);
    const FLinearColor RoomWallFilm(0.30f, 0.25f, 0.20f);
    const FLinearColor RoomWallNow(0.24f, 0.25f, 0.26f);
    const FLinearColor RoomDark(0.012f, 0.012f, 0.015f);
    const FLinearColor RoomStone(0.13f, 0.12f, 0.11f);
    const FLinearColor RoomStoneLight(0.26f, 0.24f, 0.22f);
    const FLinearColor RoomIron(0.03f, 0.03f, 0.035f);
    const FLinearColor RoomWood(0.15f, 0.085f, 0.045f);
    const FLinearColor RoomBrass(0.55f, 0.38f, 0.10f);
    const FLinearColor RoomMarble(0.62f, 0.60f, 0.56f);
    const FLinearColor RoomDress(0.42f, 0.04f, 0.06f);
    const FLinearColor RoomSkin(0.70f, 0.52f, 0.42f);
    const FLinearColor RoomHair(0.05f, 0.035f, 0.03f);
    const FLinearColor RoomPaper(0.82f, 0.78f, 0.66f);
    const FLinearColor RoomChar(0.02f, 0.018f, 0.015f);
    const FLinearColor RoomTeal(0.04f, 0.22f, 0.25f);
    const FLinearColor RoomFlame(1.0f, 0.42f, 0.08f);
    const FLinearColor RoomLantern(1.0f, 0.72f, 0.35f);
    const FLinearColor RoomDried(0.20f, 0.14f, 0.08f);

    // 灯：曝光是锁死的（EV100 = 0），这些数都是按它调的。
    const float RoomFireCandelas = 34.f;
    const float RoomLanternCandelas = 9.f;
    const float RoomFilmFill = 2.2f;
    const float RoomNowFill = 5.f;
    const float RoomNowDaylight = 46.f;
    const float RoomMoonCandelas = 420.f;       // 光源在窗外 10 米，落到壁炉上只剩两三勒克斯
}

ADarkroomRoom::ADarkroomRoom()
{
    PrimaryActorTick.bCanEverTick = false;
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Movable);
    SetRootComponent(Root);
}

ADarkroomProp* ADarkroomRoom::Find(FName Id) const
{
    const TObjectPtr<ADarkroomProp>* Found = Props.Find(Id);
    return Found ? Found->Get() : nullptr;
}

UStaticMeshComponent* ADarkroomRoom::Box(FName Name, FVector Center, FVector Size, FLinearColor Color, bool bCollide, bool bVisible, bool bEmissive)
{
    UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, Name);
    AddInstanceComponent(Part);
    Part->SetMobility(EComponentMobility::Movable);
    Part->SetupAttachment(GetRootComponent());
    Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Part->SetRelativeLocation(Center);
    Part->SetRelativeScale3D(Size / 100.f);
    Part->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
    const TCHAR* Path = bEmissive ? TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")
        : TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
    if (UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr, Path), Part))
    {
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Part->SetMaterial(0, Material);
    }
    if (bEmissive) { Part->SetCastShadow(false); }
    if (!bVisible) { Part->SetVisibility(false); Part->SetCastShadow(false); }
    Part->RegisterComponent();
    return Part;
}

UPointLightComponent* ADarkroomRoom::Lamp(FName Name, FVector Position, FLinearColor Color, float Candelas, float Radius, bool bShadows)
{
    UPointLightComponent* NewLight = NewObject<UPointLightComponent>(this, Name);
    AddInstanceComponent(NewLight);
    NewLight->SetMobility(EComponentMobility::Movable);
    NewLight->SetupAttachment(GetRootComponent());
    NewLight->SetRelativeLocation(Position);
    NewLight->SetIntensityUnits(ELightUnits::Candelas);
    NewLight->SetIntensity(Candelas);
    NewLight->SetAttenuationRadius(Radius);
    NewLight->SetLightColor(Color);
    NewLight->SetCastShadows(bShadows);
    NewLight->RegisterComponent();
    return NewLight;
}

ADarkroomProp* ADarkroomRoom::Spawn(FName Id, EDarkroomKind Kind, FName Noun, EDarkroomShape Shape, FVector Size, FLinearColor Color)
{
    ADarkroomProp* Prop = GetWorld()->SpawnActor<ADarkroomProp>(ADarkroomProp::StaticClass(), FTransform(GetActorLocation()));
    Prop->Setup(Id, Kind, Era, Noun, Shape, Size, Color);
    Props.Add(Id, Prop);
    return Prop;
}

void ADarkroomRoom::Build()
{
    BuildShell();
    BuildProps();
    if (Era == EDarkroomEra::Reality)
    {
        for (const FDarkroomSnapshot& Key : DarkroomReel::RealityKeys())
        {
            if (ADarkroomProp* Prop = Find(Key.Id)) { Prop->Restore(Key, GetActorTransform()); }
        }
    }
}

// 壳：地板、天花板、四面墙。东墙的壁炉后面有暗道，南墙有门洞，西墙有窗洞。
void ADarkroomRoom::BuildShell()
{
    const bool bFilm = Era == EDarkroomEra::Film;
    const FLinearColor WallColor = bFilm ? RoomWallFilm : RoomWallNow;

    Box(TEXT("Floor"), FVector(0, 0, -10), FVector(840, 640, 20), RoomFloor);
    Box(TEXT("Ceiling"), FVector(0, 0, 360), FVector(840, 640, 20), RoomCeiling);
    Box(TEXT("WallN"), FVector(0, 310, 175), FVector(840, 20, 350), WallColor);

    // 东墙：正中留出炉膛那么大的洞。
    Box(TEXT("WallE_S"), FVector(410, -181, 175), FVector(20, 238, 350), WallColor);
    Box(TEXT("WallE_N"), FVector(410, 181, 175), FVector(20, 238, 350), WallColor);
    Box(TEXT("WallE_Top"), FVector(410, 0, 275), FVector(20, 124, 150), WallColor);
    Box(TEXT("TunnelFloor"), FVector(540, 0, -10), FVector(240, 180, 20), RoomDark);
    Box(TEXT("TunnelS"), FVector(540, -80, 105), FVector(240, 20, 230), RoomDark);
    Box(TEXT("TunnelN"), FVector(540, 80, 105), FVector(240, 20, 230), RoomDark);
    Box(TEXT("TunnelTop"), FVector(540, 0, 215), FVector(240, 180, 20), RoomDark);
    Box(TEXT("TunnelEnd"), FVector(670, 0, 105), FVector(20, 180, 230), RoomDark);
    if (bFilm)
    {
        // 胶卷里只能看，不能跟着她走进去。
        Box(TEXT("PassageBlock"), FVector(404, 0, 100), FVector(4, 124, 200), RoomDark, true, false);
    }
    else
    {
        // 暗道尽头的光先关着：亮着会从背板的缝里漏出来，等于把答案写在墙上。背板开了才由流程点亮。
        Lamp(TEXT("TunnelGlow"), FVector(610, 0, 120), FLinearColor(0.55f, 0.7f, 1.f), 0.f, 500.f, false);
    }

    // 南墙：门洞，外面是走廊，走不出去。
    Box(TEXT("WallS_W"), FVector(-230, -310, 175), FVector(340, 20, 350), WallColor);
    Box(TEXT("WallS_E"), FVector(230, -310, 175), FVector(340, 20, 350), WallColor);
    Box(TEXT("WallS_Top"), FVector(0, -310, 285), FVector(120, 20, 130), WallColor);
    Box(TEXT("HallFloor"), FVector(0, -390, -10), FVector(300, 140, 20), RoomFloor);
    Box(TEXT("HallBack"), FVector(0, -470, 130), FVector(300, 20, 280), RoomDark, false);
    Box(TEXT("HallW"), FVector(-160, -390, 130), FVector(20, 140, 280), RoomDark, false);
    Box(TEXT("HallE"), FVector(160, -390, 130), FVector(20, 140, 280), RoomDark, false);
    Box(TEXT("HallTop"), FVector(0, -390, 280), FVector(340, 180, 20), RoomDark, false);
    Box(TEXT("DoorBlock"), FVector(0, -328, 110), FVector(120, 6, 220), RoomDark, true, false);

    // 西墙：窗洞。夜里外面是黑的，现在外面是白天。
    Box(TEXT("WallW_S"), FVector(-410, -180, 175), FVector(20, 240, 350), WallColor);
    Box(TEXT("WallW_N"), FVector(-410, 180, 175), FVector(20, 240, 350), WallColor);
    Box(TEXT("WallW_Low"), FVector(-410, 0, 50), FVector(20, 120, 100), WallColor);
    Box(TEXT("WallW_High"), FVector(-410, 0, 295), FVector(20, 120, 110), WallColor);
    if (bFilm)
    {
        Box(TEXT("Night"), FVector(-640, 0, 170), FVector(20, 700, 500), FLinearColor(0.004f, 0.006f, 0.012f), false)->SetCastShadow(false);
        Box(TEXT("NightGround"), FVector(-530, 0, 60), FVector(240, 700, 20), RoomDark, false)->SetCastShadow(false);

        // 月光：从窗外很远的地方平着照进来，穿过铁栏，正好落在壁炉上。
        // 火烧着的时候几乎看不出来；火一灭，屋里最亮的就是壁炉——答案在的地方。
        USpotLightComponent* Moon = NewObject<USpotLightComponent>(this, TEXT("Moon"));
        AddInstanceComponent(Moon);
        Moon->SetMobility(EComponentMobility::Movable);
        Moon->SetupAttachment(GetRootComponent());
        Moon->SetRelativeLocation(FVector(-1400, 0, 185));
        Moon->SetIntensityUnits(ELightUnits::Candelas);
        Moon->SetIntensity(RoomMoonCandelas);
        Moon->SetAttenuationRadius(2600.f);
        Moon->SetInnerConeAngle(7.f);
        Moon->SetOuterConeAngle(9.f);
        Moon->SetLightColor(FLinearColor(0.55f, 0.68f, 1.f));
        Moon->SetCastShadows(true);
        Moon->RegisterComponent();
    }
    else
    {
        Box(TEXT("Day"), FVector(-520, 0, 170), FVector(10, 500, 400), FLinearColor(0.55f, 0.66f, 0.80f), false, true, true);
        Box(TEXT("BoardA"), FVector(-394, 0, 138), FVector(3, 134, 22), RoomDried, false);
        Box(TEXT("BoardB"), FVector(-394, 0, 204), FVector(3, 134, 18), RoomDried, false);
        Lamp(TEXT("Daylight"), FVector(-330, 0, 190), FLinearColor(0.72f, 0.84f, 1.f), RoomNowDaylight, 1600.f, true);
    }

    // 胸像的底座和铭牌。
    Box(TEXT("Pedestal"), FVector(BustPos.X, BustPos.Y, 50), FVector(40, 40, 100), RoomStoneLight);
    Box(TEXT("Turntable"), FVector(BustPos.X, BustPos.Y, 100.4f), FVector(34, 34, 0.8f), RoomIron, false);
    Box(TEXT("Plaque"), FVector(BustPos.X - 20.6f, BustPos.Y, 72), FVector(1.2f, 24, 10), RoomBrass, false);

    MainLight = bFilm
        ? Lamp(TEXT("Fill"), FVector(0, 0, 240), FLinearColor(1.f, 0.72f, 0.45f), RoomFilmFill, 1500.f, false)
        : Lamp(TEXT("Fill"), FVector(60, 0, 300), FLinearColor(0.80f, 0.88f, 1.f), RoomNowFill, 1500.f, false);
}

void ADarkroomRoom::BuildProps()
{
    using EKind = EDarkroomKind;
    using EShape = EDarkroomShape;
    const bool bFilm = Era == EDarkroomEra::Film;
    const FLinearColor WallColor = bFilm ? RoomWallFilm : RoomWallNow;
    const FRotator Flat = FRotator::ZeroRotator;
    const FRotator PointForward(-90, 0, 0);    // 锥尖朝 +X

    // 壁炉：炉膛大得能走进一个人。
    ADarkroomProp* Fire = Spawn(PropFireplace, EKind::Fireplace, DarkroomNoun::Fire, EShape::Cube, FVector(70, 172, 10), RoomStone);
    Fire->AddShape(TEXT("JambS"), EShape::Cube, FVector(5, -80, 93), FVector(80, 36, 196), RoomStone, Flat, false, true);
    Fire->AddShape(TEXT("JambN"), EShape::Cube, FVector(5, 80, 93), FVector(80, 36, 196), RoomStone, Flat, false, true);
    Fire->AddShape(TEXT("Mantel"), EShape::Cube, FVector(0, 0, 202), FVector(96, 214, 22), RoomStoneLight, Flat, false, true);
    Fire->AddShape(TEXT("Breast"), EShape::Cube, FVector(17, 0, 279), FVector(56, 196, 132), WallColor, Flat, false, true);
    Fire->AddShape(TEXT("Ash"), EShape::Sphere, FVector(0, 0, 7), FVector(52, 96, 8), FLinearColor(0.09f, 0.09f, 0.09f));
    Fire->AddShape(TEXT("LogA"), EShape::Cylinder, FVector(-4, 0, 13), FVector(11, 11, 62), RoomWood, FRotator(0, 0, 90));
    Fire->AddShape(TEXT("LogB"), EShape::Cylinder, FVector(8, 6, 20), FVector(10, 10, 54), RoomWood, FRotator(0, 18, 90));
    Fire->Flame = Fire->AddShape(TEXT("Flame"), EShape::Cone, FVector(2, 0, 50), FVector(46, 78, 74), RoomFlame, Flat, true);
    // 光源放在炉膛里面：放在炉口外的话，夫人走到火前时会把它整个罩在身体里，光从模型的接缝漏出来。
    Fire->AddLight(FVector(-20, 0, 58), FLinearColor(1.f, 0.55f, 0.22f), RoomFireCandelas, 1500.f);
    Fire->RealityLine = DR_TEXT("灰都结块了。这火是被水浇灭的。", "The ash has caked hard. This fire was put out with water.");

    // 背板：一整块铸铁。
    ADarkroomProp* Fireback = Spawn(PropFireback, EKind::Fireback, DarkroomNoun::Fire, EShape::Cube, FVector(8, 124, 196), RoomIron);
    Fireback->AddShape(TEXT("Crest"), EShape::Cube, FVector(-4.5f, 0, 20), FVector(2, 56, 70), FLinearColor(0.05f, 0.05f, 0.055f));
    Fireback->UsePrompt = DR_TEXT("E 推", "E  Push");

    // 门和闩。
    ADarkroomProp* Door = Spawn(PropDoor, EKind::Door, DarkroomNoun::Door, EShape::Cube, FVector(110, 8, 212), RoomWood);
    Door->AddShape(TEXT("Knob"), EShape::Sphere, FVector(42, 8, -4), FVector(8, 8, 8), RoomBrass);
    Door->RealityLine = DR_TEXT("门是被人从外面撞开的。", "The door was broken in from the outside.");
    ADarkroomProp* Bolt = Spawn(PropBolt, EKind::Plain, DarkroomNoun::Door, EShape::Cube, FVector(36, 5, 7), RoomIron);
    Bolt->SetGhost();
    Bolt->RealityLine = DR_TEXT("插销是从里面插上的，被撞断了。", "The bolt was shot from the inside. It snapped when the door gave.");

    // 窗：瞄准用一块看不见的板，画出来的是铁栏。
    ADarkroomProp* Window = Spawn(PropWindow, EKind::Plain, DarkroomNoun::Window, EShape::Cube, FVector(6, 120, 140), RoomIron);
    Window->HideBody();
    for (int32 Index = 0; Index < 5; ++Index)
    {
        Window->AddShape(*FString::Printf(TEXT("Bar%d"), Index), EShape::Cube, FVector(0, -48.f + 24.f * Index, 0), FVector(4, 4, 140), RoomIron);
    }
    Window->AddShape(TEXT("Rail"), EShape::Cube, FVector(0, 0, 0), FVector(4, 120, 4), RoomIron);
    Window->SetGhost();
    Window->RealityLine = DR_TEXT("窗上装着铁栏，后来又钉了木板。", "Iron bars, and boards nailed over them later.");

    // 书桌。
    ADarkroomProp* Desk = Spawn(PropDesk, EKind::Plain, DarkroomNoun::Desk, EShape::Cube, FVector(160, 80, 6), RoomWood);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const float SX = (Index & 1) ? 72.f : -72.f, SY = (Index & 2) ? 32.f : -32.f;
        Desk->AddShape(*FString::Printf(TEXT("Leg%d"), Index), EShape::Cube, FVector(SX, SY, -40.5f), FVector(7, 7, 75), RoomWood, Flat, false, true);
    }
    Desk->AddShape(TEXT("Ink"), EShape::Cylinder, FVector(34, -6, 7), FVector(8, 8, 8), RoomIron);
    Desk->AddShape(TEXT("Pen"), EShape::Cube, FVector(48, -18, 3.8f), FVector(15, 1.2f, 1.2f), RoomPaper, FRotator(0, 25, 0));
    Desk->RealityLine = DR_TEXT("墨水早就干了，笔还扔在原处。", "The ink dried up long ago. The pen still lies where it was dropped.");

    // 信：完好 / 正在烧 / 烧剩半张。
    ADarkroomProp* Letter = Spawn(PropLetter, EKind::Letter, DarkroomNoun::Letter, EShape::Cube, FVector(18, 12, 1), RoomPaper);
    Letter->AddShape(TEXT("Seal"), EShape::Cylinder, FVector(-5, 0, 0.6f), FVector(3, 3, 0.5f), RoomDress);
    Letter->Extras.Add(Letter->AddShape(TEXT("Char"), EShape::Cube, FVector(4.6f, 0, 0.1f), FVector(9.4f, 12.6f, 1.3f), RoomChar));
    Letter->Flame = Letter->AddShape(TEXT("Ember"), EShape::Cube, FVector(0, 0, 0.9f), FVector(19, 13, 0.6f), RoomFlame, Flat, true);
    Letter->SetGhost();
    Letter->RealityLine = DR_TEXT("烧剩的半张信，字迹已经认不出了。", "Half a letter, left over from the fire. The writing is past reading.");

    // 花瓶，和倒出来的花。
    ADarkroomProp* Vase = Spawn(PropVase, EKind::Vase, DarkroomNoun::Vase, EShape::Cylinder, FVector(22, 22, 40), RoomTeal);
    Vase->AddShape(TEXT("Neck"), EShape::Cylinder, FVector(0, 0, 22), FVector(14, 14, 6), RoomTeal);
    Vase->Extras.Add(Vase->AddShape(TEXT("BloomA"), EShape::Sphere, FVector(0, 0, 35), FVector(12, 12, 11), FLinearColor(0.85f, 0.35f, 0.45f)));
    Vase->Extras.Add(Vase->AddShape(TEXT("BloomB"), EShape::Sphere, FVector(7, 5, 31), FVector(10, 10, 9), FLinearColor(0.9f, 0.85f, 0.7f)));
    Vase->Extras.Add(Vase->AddShape(TEXT("BloomC"), EShape::Sphere, FVector(-7, -4, 32), FVector(10, 10, 9), FLinearColor(0.9f, 0.7f, 0.2f)));
    Vase->SetGhost();
    Vase->RealityLine = DR_TEXT("花瓶怎么会倒在壁炉里？", "What is a vase doing in the fireplace?");

    const FLinearColor Bloom = bFilm ? FLinearColor(0.85f, 0.35f, 0.45f) : RoomDried;
    ADarkroomProp* Flowers = Spawn(PropFlowers, EKind::Plain, DarkroomNoun::Vase, EShape::Sphere, FVector(12, 12, 8), Bloom);
    Flowers->AddShape(TEXT("BloomB"), EShape::Sphere, FVector(14, 6, 0), FVector(10, 10, 7), bFilm ? FLinearColor(0.9f, 0.85f, 0.7f) : RoomDried);
    Flowers->AddShape(TEXT("BloomC"), EShape::Sphere, FVector(-6, 15, 0), FVector(10, 10, 7), bFilm ? FLinearColor(0.9f, 0.7f, 0.2f) : RoomDried);
    Flowers->AddShape(TEXT("StemA"), EShape::Cube, FVector(-15, -6, -2.5f), FVector(30, 1.5f, 1.5f), bFilm ? FLinearColor(0.1f, 0.3f, 0.1f) : RoomDried, FRotator(0, 20, 0));
    Flowers->AddShape(TEXT("StemB"), EShape::Cube, FVector(-10, 12, -2.5f), FVector(28, 1.5f, 1.5f), bFilm ? FLinearColor(0.1f, 0.3f, 0.1f) : RoomDried, FRotator(0, -35, 0));
    Flowers->SetGhost();
    Flowers->RealityLine = DR_TEXT("干透了的花。当年就是倒在这儿的。", "Flowers, dried to husks. They have lain here since that night.");

    // 胸像：鼻子和胸前的徽章让人看得出它朝哪边。
    ADarkroomProp* Bust = Spawn(PropBust, EKind::Bust, DarkroomNoun::Bust, EShape::Cube, FVector(26, 38, 22), RoomMarble);
    Bust->AddShape(TEXT("Head"), EShape::Sphere, FVector(0, 0, 26), FVector(26, 24, 30), RoomMarble);
    Bust->AddShape(TEXT("Nose"), EShape::Cone, FVector(14, 0, 26), FVector(7, 7, 12), RoomMarble, PointForward);
    Bust->AddShape(TEXT("Medal"), EShape::Sphere, FVector(13, 0, 2), FVector(4, 9, 9), RoomBrass);
    Bust->UsePrompt = DR_TEXT("E 转动", "E  Turn");

    // 画像：年轻时的夫人。
    ADarkroomProp* Portrait = Spawn(PropPortrait, EKind::Plain, DarkroomNoun::Portrait, EShape::Cube, FVector(96, 24, 128), FLinearColor(0.30f, 0.20f, 0.07f));
    Portrait->AddShape(TEXT("Canvas"), EShape::Cube, FVector(0, -12, 0), FVector(80, 2, 112), FLinearColor(0.10f, 0.11f, 0.09f));
    Portrait->AddShape(TEXT("Dress"), EShape::Cube, FVector(0, -13.2f, -27), FVector(40, 2, 56), RoomDress);
    Portrait->AddShape(TEXT("Face"), EShape::Sphere, FVector(0, -13.6f, 16), FVector(22, 3, 27), RoomSkin);
    Portrait->AddShape(TEXT("Hair"), EShape::Sphere, FVector(0, -13.0f, 25), FVector(29, 2.4f, 22), RoomHair);
    Portrait->SetGhost();
    Portrait->RealityLine = DR_TEXT("画上是年轻时的夫人。", "The lady, painted when she was young.");

    if (bFilm)
    {
        ADarkroomProp* Lady = Spawn(PropLady, EKind::Plain, DarkroomNoun::Lady, EShape::Cylinder, FVector(46, 46, 150), RoomDress);
        Lady->AddShape(TEXT("Head"), EShape::Sphere, FVector(0, 0, 87), FVector(24, 24, 26), RoomSkin);
        Lady->AddShape(TEXT("Nose"), EShape::Cone, FVector(13, 0, 86), FVector(6, 6, 10), RoomSkin, PointForward);
        Lady->AddShape(TEXT("Hair"), EShape::Sphere, FVector(-8, 0, 94), FVector(18, 21, 16), RoomHair);
        Lady->AddShape(TEXT("Arm"), EShape::Cube, FVector(20, 0, 27), FVector(22, 8, 7), RoomDress);
        Lady->SetGhost();

        ADarkroomProp* Pursuer = Spawn(PropPursuer, EKind::Plain, DarkroomNoun::Pursuer, EShape::Cylinder, FVector(54, 54, 150), FLinearColor(0.025f, 0.025f, 0.03f));
        Pursuer->AddShape(TEXT("Head"), EShape::Sphere, FVector(0, 0, 88), FVector(25, 25, 26), FLinearColor(0.35f, 0.28f, 0.24f));
        Pursuer->AddShape(TEXT("Brim"), EShape::Cylinder, FVector(0, 0, 100), FVector(38, 38, 3), RoomChar);
        Pursuer->AddShape(TEXT("Crown"), EShape::Cylinder, FVector(0, 0, 107), FVector(23, 23, 12), RoomChar);
        Pursuer->AddShape(TEXT("Arm"), EShape::Cube, FVector(20, 14, 44), FVector(26, 8, 8), FLinearColor(0.025f, 0.025f, 0.03f));
        Pursuer->SetGhost();

        ADarkroomProp* Lantern = Spawn(PropLantern, EKind::Lantern, NAME_None, EShape::Sphere, FVector(12, 12, 16), RoomLantern);
        Lantern->SetEmissive(RoomLantern);
        Lantern->AddShape(TEXT("Cap"), EShape::Cylinder, FVector(0, 0, 10), FVector(13, 13, 3), RoomIron);
        Lantern->AddLight(FVector::ZeroVector, RoomLantern, RoomLanternCandelas, 900.f);
        Lantern->SetGhost();
    }
    else
    {
        ADarkroomProp* Reel = Spawn(PropReel, EKind::Reel, NAME_None, EShape::Cylinder, FVector(24, 24, 6), FLinearColor(0.42f, 0.40f, 0.36f));
        Reel->AddShape(TEXT("Hub"), EShape::Cylinder, FVector(0, 0, 0.6f), FVector(8, 8, 6.4f), RoomIron);
        Reel->AddShape(TEXT("Label"), EShape::Cube, FVector(6, 0, 3.1f), FVector(8, 12, 0.3f), RoomPaper);
        Reel->SetGhost();
        Reel->UsePrompt = DR_TEXT("E 拾取胶卷", "E  Take the reel");
    }

    for (const auto& Pair : Props)
    {
        if (Pair.Value && Pair.Value->UsePrompt.IsEmpty() && !Pair.Value->RealityLine.IsEmpty()) { Pair.Value->UsePrompt = DR_TEXT("E 查看", "E  Examine"); }
    }
}
