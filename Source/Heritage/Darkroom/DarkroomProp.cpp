#include "DarkroomProp.h"
#include "DarkroomFlowSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
    // 现在的胸像：转过去以后只停这么久，然后自己转回原位。要用它，就得趁这一会儿。
    const float PropBustHoldSeconds = 2.5f;
    const float PropBustReturnSeconds = 0.9f;

    // 看不见的零件也不该挡住瞄准。
    void DarkroomShowPart(UStaticMeshComponent* Part, bool bShow)
    {
        if (!Part) { return; }
        Part->SetVisibility(bShow);
        Part->SetCollisionEnabled(bShow ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }
}

ADarkroomProp::ADarkroomProp()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
}

UStaticMesh* ADarkroomProp::LoadShape(EDarkroomShape Shape)
{
    const TCHAR* Path = TEXT("/Engine/BasicShapes/Cube.Cube");
    switch (Shape)
    {
    case EDarkroomShape::Sphere: Path = TEXT("/Engine/BasicShapes/Sphere.Sphere"); break;
    case EDarkroomShape::Cylinder: Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder"); break;
    case EDarkroomShape::Cone: Path = TEXT("/Engine/BasicShapes/Cone.Cone"); break;
    default: break;
    }
    return LoadObject<UStaticMesh>(nullptr, Path);
}

UMaterialInstanceDynamic* ADarkroomProp::MakeMaterial(UObject* Outer, FLinearColor Color, bool bEmissive)
{
    // 发光的用引擎自带的不受光叠加材质（火苗、提灯），其余用基础形状材质。
    const TCHAR* Path = bEmissive ? TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")
        : TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr, Path), Outer);
    if (Material) { Material->SetVectorParameterValue(TEXT("Color"), Color); }
    return Material;
}

void ADarkroomProp::Setup(FName InId, EDarkroomKind InKind, EDarkroomEra InEra, FName InNoun, EDarkroomShape Shape, FVector SizeCm, FLinearColor Color)
{
    FilmId = InId; Kind = InKind; Era = InEra; Noun = InNoun;
    Mesh->SetStaticMesh(LoadShape(Shape));
    SetActorScale3D(SizeCm / 100.f);
    Mesh->SetMaterial(0, MakeMaterial(this, Color, false));
}

UStaticMeshComponent* ADarkroomProp::AddShape(FName Name, EDarkroomShape Shape, FVector Position, FVector SizeCm, FLinearColor Color,
    FRotator Rotation, bool bEmissive, bool bSolid)
{
    UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, Name);
    AddInstanceComponent(Part);
    Part->SetMobility(EComponentMobility::Movable);
    Part->SetupAttachment(Mesh);
    Part->SetStaticMesh(LoadShape(Shape));
    // 根形状是缩放过的，零件的尺寸和偏移不能跟着变形（Richard 原型的做法）。
    Part->SetAbsolute(false, false, true);
    Part->SetRelativeLocation(Position / Mesh->GetComponentScale());
    Part->SetRelativeRotation(Rotation);
    Part->SetWorldScale3D(SizeCm / 100.f);
    if (bSolid) { Part->SetCollisionProfileName(TEXT("BlockAll")); }
    else
    {
        Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Part->SetCollisionResponseToAllChannels(ECR_Ignore);
        Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    }
    if (bEmissive) { Part->SetCastShadow(false); }
    Part->SetMaterial(0, MakeMaterial(Part, Color, bEmissive));
    Part->RegisterComponent();
    return Part;
}

UPointLightComponent* ADarkroomProp::AddLight(FVector Position, FLinearColor Color, float Candelas, float Radius)
{
    UPointLightComponent* NewLight = NewObject<UPointLightComponent>(this, TEXT("PropLight"));
    AddInstanceComponent(NewLight);
    NewLight->SetMobility(EComponentMobility::Movable);
    NewLight->SetupAttachment(Mesh);
    NewLight->SetAbsolute(false, false, true);
    NewLight->SetRelativeLocation(Position / Mesh->GetComponentScale());
    NewLight->SetIntensityUnits(ELightUnits::Candelas);
    NewLight->SetIntensity(Candelas);
    NewLight->SetAttenuationRadius(Radius);
    NewLight->SetLightColor(Color);
    NewLight->SetCastShadows(true);
    NewLight->RegisterComponent();
    Light = NewLight; LightCandelas = Candelas;
    return NewLight;
}

void ADarkroomProp::SetGhost()
{
    TInlineComponentArray<UPrimitiveComponent*> Parts(this);
    for (UPrimitiveComponent* Part : Parts)
    {
        Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Part->SetCollisionResponseToAllChannels(ECR_Ignore);
        Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    }
}

void ADarkroomProp::SetEmissive(FLinearColor Color)
{
    Mesh->SetMaterial(0, MakeMaterial(this, Color, true));
    Mesh->SetCastShadow(false);
}

void ADarkroomProp::HideBody()
{
    Mesh->SetVisibility(false, false);
    Mesh->SetCastShadow(false);
}

void ADarkroomProp::RefreshFromState()
{
    switch (Kind)
    {
    case EDarkroomKind::Fireplace:
    {
        const bool bLit = HasState(DarkroomTags::Lit);
        DarkroomShowPart(Flame, bLit);
        if (Light) { Light->SetVisibility(bLit); Light->SetIntensity(bLit ? LightCandelas : 0.f); }
        break;
    }
    case EDarkroomKind::Lantern:
        if (Light) { Light->SetVisibility(!IsHidden()); }
        break;
    case EDarkroomKind::Vase:
        for (const auto& Part : Extras) { DarkroomShowPart(Part, HasState(DarkroomTags::Filled)); }
        break;
    case EDarkroomKind::Letter:
        // 完好 / 正在烧 / 烧剩半张。
        for (const auto& Part : Extras) { DarkroomShowPart(Part, !HasState(DarkroomTags::Whole)); }
        DarkroomShowPart(Flame, HasState(DarkroomTags::Burning));
        break;
    default: break;
    }
}

FDarkroomSnapshot ADarkroomProp::Capture(const FTransform& Room) const
{
    FDarkroomSnapshot Result;
    const FTransform Local = GetActorTransform().GetRelativeTransform(Room);
    Result.Id = FilmId; Result.Location = Local.GetLocation(); Result.Rotation = Local.Rotator();
    Result.State = StateTags; Result.bHidden = IsHidden();
    return Result;
}

void ADarkroomProp::Restore(const FDarkroomSnapshot& Snapshot, const FTransform& Room)
{
    bTweening = false;
    const FTransform World = FTransform(Snapshot.Rotation, Snapshot.Location) * Room;
    SetActorLocationAndRotation(World.GetLocation(), World.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
    BaseRotation = GetActorRotation();
    HomeRotation = BaseRotation; ReturnTimer = 0.f;
    if (IsHidden() != Snapshot.bHidden)
    {
        SetActorHiddenInGame(Snapshot.bHidden);
        SetActorEnableCollision(!Snapshot.bHidden);
    }
    StateTags = Snapshot.State;
    RefreshFromState();
}

void ADarkroomProp::Flare(float Seconds) { FlareUntil = GetWorld()->GetTimeSeconds() + Seconds; }

void ADarkroomProp::TweenTo(FVector WorldLocation, FRotator WorldRotation, float Seconds)
{
    bTweening = true; TweenAlpha = 0.f; TweenDuration = FMath::Max(Seconds, 0.01f);
    TweenFromLocation = GetActorLocation(); TweenToLocation = WorldLocation;
    TweenFromRotation = GetActorQuat(); TweenToRotation = WorldRotation.Quaternion();
}

float ADarkroomProp::GetFacingYaw() const
{
    return FRotator::ClampAxis((bTweening ? TweenToRotation.Rotator() : BaseRotation).Yaw);
}

void ADarkroomProp::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Now = GetWorld()->GetTimeSeconds();
    if (bTweening)
    {
        TweenAlpha = FMath::Min(1.f, TweenAlpha + DeltaSeconds / TweenDuration);
        const float Eased = FMath::SmoothStep(0.f, 1.f, TweenAlpha);
        SetActorLocationAndRotation(FMath::Lerp(TweenFromLocation, TweenToLocation, Eased), FQuat::Slerp(TweenFromRotation, TweenToRotation, Eased));
        if (TweenAlpha >= 1.f) { bTweening = false; BaseRotation = GetActorRotation(); }
    }
    else if (ReturnTimer > 0.f)
    {
        // 胸像底下像是有根弹簧：不管转到了哪边，停一会儿都会自己转回去。朝哪边都一样，所以这不算提示。
        ReturnTimer -= DeltaSeconds;
        if (ReturnTimer <= 0.f)
        {
            ReturnTimer = 0.f;
            if (FMath::Abs(FMath::FindDeltaAngleDegrees(BaseRotation.Yaw, HomeRotation.Yaw)) > 1.f)
            {
                TweenTo(GetActorLocation(), HomeRotation, PropBustReturnSeconds);
                if (UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>()) { Flow->NotifyBustReturned(); }
            }
        }
    }
    if (Kind == EDarkroomKind::Fireplace && Light && HasState(DarkroomTags::Lit))
    {
        const float Boost = Now < FlareUntil ? 2.6f : 1.f;
        Light->SetIntensity(LightCandelas * Boost * (1.f + 0.15f * FMath::Sin(Now * 13.f)));
    }
    if (Kind == EDarkroomKind::Door && HasState(DarkroomTags::Shaking) && !bTweening)
    {
        // 一阵一阵地撞。
        const float Burst = FMath::Sin(Now * 2.3f) > 0.15f ? 1.f : 0.f;
        SetActorRotation(BaseRotation + FRotator(0, 0.9f * Burst * FMath::Sin(Now * 41.f), 0));
    }
}

bool ADarkroomProp::OnUse()
{
    UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (!Flow || Era != EDarkroomEra::Reality) { return false; }
    switch (Kind)
    {
    case EDarkroomKind::Reel:
        Flow->CollectReel();
        SetActorHiddenInGame(true); SetActorEnableCollision(false);
        return true;
    case EDarkroomKind::Bust:
    {
        if (bTweening) { return true; }
        const FRotator Next = BaseRotation + FRotator(0, 90, 0);
        TweenTo(GetActorLocation(), Next, 0.45f);
        ReturnTimer = PropBustHoldSeconds;      // 每转一下重新计时；转完才开始数
        Flow->NotifyBustTurned(FRotator::ClampAxis(Next.Yaw));
        return true;
    }
    case EDarkroomKind::Fireback:
        Flow->TryFireback(this);
        return true;
    default:
        if (RealityLine.IsEmpty()) { return false; }
        Flow->Say(RealityLine);
        Flow->Log(FString::Printf(TEXT("现在：查看 %s"), *FilmId.ToString()));
        return true;
    }
}
