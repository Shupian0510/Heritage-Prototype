#include "DarkroomCharacter.h"
#include "DarkroomFlowSubsystem.h"
#include "DarkroomDirector.h"
#include "DarkroomRoom.h"
#include "DarkroomProp.h"
#include "DarkroomReel.h"
#include "DarkroomWidgets.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SceneView.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Widgets/Layout/SDPIScaler.h"

namespace
{
    const float PlayerWalkSpeed = 260.f;
    const float PlayerFilmRange = 1200.f;       // 过去：隔着整间屋也拍得到
    const float PlayerReachRange = 300.f;       // 现在：得走到跟前
    const float PlayerAimAssistDegrees = 4.f;
    const float PlayerCursorSpeed = 20.f;       // 鼠标 → 笔记本上的笔尖
    const float PlayerPageWidthCm = 38.f;
    const float PlayerPageSharpness = 1.5f;     // 页面按 1.5 倍分辨率画，举到眼前字不糊
    const FVector PlayerPageLowered(24, 0, -34);
    const FVector PlayerPageRaised(25, 0, 0.4f); // 举起来占画面宽度的四分之三，四个角上的 HUD 还露着
}

ADarkroomCharacter::ADarkroomCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(32.f, 84.f);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(GetRootComponent());
    Camera->SetRelativeLocation(FVector(0, 0, 62));
    Camera->bUsePawnControlRotation = true;
    Camera->SetFieldOfView(90.f);
    Camera->PostProcessBlendWeight = 1.f;

    // 拍照用的第二个镜头：比眼睛看到的窄，拍下来的是准星对着的那一块。
    PhotoCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PhotoCapture"));
    PhotoCapture->SetupAttachment(Camera);
    PhotoCapture->bCaptureEveryFrame = false;
    PhotoCapture->bCaptureOnMovement = false;
    PhotoCapture->FOVAngle = 48.f;
    PhotoCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

    // 笔记本：拿在手里的一个本子，不是贴在屏幕上的菜单。
    NotebookPivot = CreateDefaultSubobject<USceneComponent>(TEXT("NotebookPivot"));
    NotebookPivot->SetupAttachment(Camera);
    NotebookCover = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NotebookCover"));
    NotebookCover->SetupAttachment(NotebookPivot);
    NotebookCover->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NotebookCover->SetCastShadow(false);
    NotebookPage = CreateDefaultSubobject<UWidgetComponent>(TEXT("NotebookPage"));
    NotebookPage->SetupAttachment(NotebookPivot);
    NotebookPage->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NotebookPage->SetCastShadow(false);

    OverrideInputComponentClass = UEnhancedInputComponent::StaticClass();
    GetCharacterMovement()->MaxWalkSpeed = PlayerWalkSpeed;
}

void ADarkroomCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // 输入也在代码里建，原型不带任何资产（Richard 原型的做法）。
    MappingContext = NewObject<UInputMappingContext>(this);
    auto MakeAction = [this](EInputActionValueType Type)
    {
        UInputAction* Action = NewObject<UInputAction>(this); Action->ValueType = Type; return Action;
    };
    MoveAction = MakeAction(EInputActionValueType::Axis2D);
    MoveAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
    LookAction = MakeAction(EInputActionValueType::Axis2D);
    UseAction = MakeAction(EInputActionValueType::Boolean);
    RewindAction = MakeAction(EInputActionValueType::Boolean);
    ShootAction = MakeAction(EInputActionValueType::Boolean);
    NotebookAction = MakeAction(EInputActionValueType::Boolean);
    EraAction = MakeAction(EInputActionValueType::Boolean);
    ResetAction = MakeAction(EInputActionValueType::Boolean);

    auto MapMove = [this](FKey Key, bool bSwizzle, bool bNegative)
    {
        auto& Mapping = MappingContext->MapKey(MoveAction, Key);
        if (bSwizzle) { Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext)); }
        if (bNegative) { Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext)); }
    };
    MapMove(EKeys::W, true, false); MapMove(EKeys::S, true, true);
    MapMove(EKeys::A, false, true); MapMove(EKeys::D, false, false);
    auto& LookMapping = MappingContext->MapKey(LookAction, EKeys::Mouse2D);
    auto* NegateY = NewObject<UInputModifierNegate>(MappingContext);
    NegateY->bX = false; NegateY->bY = true; NegateY->bZ = false;
    LookMapping.Modifiers.Add(NegateY);
    MappingContext->MapKey(UseAction, EKeys::E);
    MappingContext->MapKey(RewindAction, EKeys::Q);
    MappingContext->MapKey(ShootAction, EKeys::LeftMouseButton);
    MappingContext->MapKey(NotebookAction, EKeys::Tab);
    MappingContext->MapKey(NotebookAction, EKeys::N);
    MappingContext->MapKey(EraAction, EKeys::F);
    MappingContext->MapKey(ResetAction, EKeys::F5);
}

void ADarkroomCharacter::BeginPlay()
{
    Super::BeginPlay();
    InstallMappingContext();

    // 取景器一直开着：Lumen 的间接光要攒几帧才有，镜头没有自己的渲染状态时拍出来只剩直射光，照片是黑的。
    PhotoCapture->bAlwaysPersistRenderingState = true;
    if (FApp::CanEverRender())
    {
        UTextureRenderTarget2D* Viewfinder = NewObject<UTextureRenderTarget2D>(this);
        Viewfinder->RenderTargetFormat = RTF_RGBA8_SRGB;
        Viewfinder->ClearColor = FLinearColor::Black;
        Viewfinder->InitAutoFormat(320, 240);
        Viewfinder->UpdateResourceImmediate(true);
        PhotoCapture->TextureTarget = Viewfinder;
    }
    PhotoCapture->HideComponent(NotebookCover);     // 本子还没放下去就按快门，也不会拍进照片里
    PhotoCapture->HideComponent(NotebookPage);

    NotebookPivot->SetRelativeLocation(PlayerPageLowered);
    NotebookCover->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    NotebookCover->SetRelativeLocation(FVector(0.7f, 0, 0));
    NotebookCover->SetRelativeScale3D(FVector(0.008f, 0.405f, 0.26f));
    if (UMaterialInstanceDynamic* Leather = UMaterialInstanceDynamic::Create(
        LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")), NotebookCover))
    {
        Leather->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.07f, 0.035f, 0.02f));
        NotebookCover->SetMaterial(0, Leather);
    }

    const FVector2D PageSize = SDarkroomNotebook::PageSize();
    const float PageScale = PlayerPageWidthCm / (PageSize.X * PlayerPageSharpness);
    NotebookPage->SetWidgetSpace(EWidgetSpace::World);
    NotebookPage->SetDrawSize(PageSize * PlayerPageSharpness);
    NotebookPage->SetBlendMode(EWidgetBlendMode::Transparent);
    NotebookPage->SetTwoSided(false);
    NotebookPage->SetWindowFocusable(false);
    NotebookPage->SetRelativeRotation(FRotator(0, 180, 0));     // 页面朝着眼睛
    NotebookPage->SetRelativeScale3D(FVector(1.f, PageScale, PageScale));
    if (FApp::CanEverRender() && FSlateApplication::IsInitialized())
    {
        Notebook = SNew(SDarkroomNotebook).Flow(GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>());
        NotebookPage->SetSlateWidget(SNew(SDPIScaler).DPIScale(PlayerPageSharpness)[ Notebook.ToSharedRef() ]);
    }
    NotebookPivot->SetVisibility(false, true);
    ApplyEra(EDarkroomEra::Reality);
}

void ADarkroomCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    if (NotebookPage) { NotebookPage->SetSlateWidget(nullptr); }
    Notebook.Reset();
    Super::EndPlay(Reason);
}

void ADarkroomCharacter::PawnClientRestart() { Super::PawnClientRestart(); InstallMappingContext(); }

void ADarkroomCharacter::InstallMappingContext()
{
    if (!MappingContext) { return; }
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (auto* Input = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                Input->RemoveMappingContext(MappingContext);
                Input->AddMappingContext(MappingContext, 0);
                PC->SetInputMode(FInputModeGameOnly());
                PC->bShowMouseCursor = false;
            }
        }
        // 非 Shipping 版里引擎把 F5 绑在调试命令 viewmode shadercomplexity 上，和这里的"F5 重来"撞了：
        // 画面会变成没有光照的平涂，重开以后也不恢复。只把 F5 这一条拿掉，F1–F4 那些照旧。
        if (PC->PlayerInput)
        {
            PC->PlayerInput->DebugExecBindings.RemoveAll([](const FKeyBind& Bind) { return Bind.Key == EKeys::F5; });
        }
    }
}

void ADarkroomCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADarkroomCharacter::Move);
        Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADarkroomCharacter::Look);
        Input->BindAction(UseAction, ETriggerEvent::Started, this, &ADarkroomCharacter::UsePressed);
        Input->BindAction(UseAction, ETriggerEvent::Completed, this, &ADarkroomCharacter::UseReleased);
        Input->BindAction(UseAction, ETriggerEvent::Canceled, this, &ADarkroomCharacter::UseReleased);
        Input->BindAction(RewindAction, ETriggerEvent::Started, this, &ADarkroomCharacter::RewindPressed);
        Input->BindAction(RewindAction, ETriggerEvent::Completed, this, &ADarkroomCharacter::RewindReleased);
        Input->BindAction(RewindAction, ETriggerEvent::Canceled, this, &ADarkroomCharacter::RewindReleased);
        Input->BindAction(ShootAction, ETriggerEvent::Started, this, &ADarkroomCharacter::ShootPressed);
        Input->BindAction(NotebookAction, ETriggerEvent::Started, this, &ADarkroomCharacter::ToggleNotebook);
        Input->BindAction(EraAction, ETriggerEvent::Started, this, &ADarkroomCharacter::ToggleEra);
        Input->BindAction(ResetAction, ETriggerEvent::Started, this, &ADarkroomCharacter::ResetLevel);
    }
}

void ADarkroomCharacter::Move(const FInputActionValue& Value)
{
    const UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (Flow && !Flow->CanMove()) { return; }
    const FVector2D Axis = Value.Get<FVector2D>();
    const FRotator Yaw(0, GetControlRotation().Yaw, 0);
    AddMovementInput(Yaw.Vector(), Axis.Y);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void ADarkroomCharacter::Look(const FInputActionValue& Value)
{
    UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    const FVector2D Axis = Value.Get<FVector2D>();
    if (Flow && Flow->bNotebookOpen)
    {
        // 本子打开时鼠标动的是笔尖，不是头。
        // 视角那条映射把 Y 取了反（鼠标往上推是负的），页面的 Y 又是往下长的，所以这里直接加：往上推，笔尖往上走。
        const FVector2D PageSize = SDarkroomNotebook::PageSize();
        Flow->Cursor.X = FMath::Clamp(Flow->Cursor.X + Axis.X * PlayerCursorSpeed, 0.0, PageSize.X);
        Flow->Cursor.Y = FMath::Clamp(Flow->Cursor.Y + Axis.Y * PlayerCursorSpeed, 0.0, PageSize.Y);
        return;
    }
    if (Flow && Flow->bSliceComplete) { return; }
    AddControllerYawInput(Axis.X); AddControllerPitchInput(Axis.Y);
}

void ADarkroomCharacter::SetHeld(int32 InDir, bool bDown) { (InDir > 0 ? bForwardHeld : bRewindHeld) = bDown; }

void ADarkroomCharacter::UsePressed()
{
    const UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (Flow && Flow->Era == EDarkroomEra::Film) { SetHeld(1, true); } else { Use(); }
}
void ADarkroomCharacter::UseReleased() { SetHeld(1, false); }
void ADarkroomCharacter::RewindPressed() { SetHeld(-1, true); }
void ADarkroomCharacter::RewindReleased() { SetHeld(-1, false); }

void ADarkroomCharacter::ShootPressed()
{
    const UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (Flow && Flow->bNotebookOpen) { ClickNotebook(); } else { Shoot(); }
}

// 现在：对着东西按 E。
void ADarkroomCharacter::Use()
{
    const UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (!Flow || Flow->Era != EDarkroomEra::Reality || Flow->bNotebookOpen || Flow->bSliceComplete) { return; }
    UpdateFocus();
    if (Focus.IsValid()) { Focus->OnUse(); }
}

// 过去：对着这一帧里的东西拍一张，记进笔记本。
void ADarkroomCharacter::Shoot()
{
    UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (!Flow || !Flow->IsReady() || Flow->Era != EDarkroomEra::Film || Flow->bNotebookOpen || !Flow->Director->IsIdle()) { return; }
    UpdateFocus();
    if (FocusClue == INDEX_NONE)
    {
        Flow->Flash(FLinearColor(1.f, 1.f, 1.f, 0.22f), 0.15f);
        Flow->NoteEmptyShot();
        return;
    }
    if (Flow->HasScanned(DarkroomReel::Clues()[FocusClue].Id)) { return; }
    UTextureRenderTarget2D* Photo = TakePhoto();
    if (Flow->Scan(FocusClue, Photo)) { Flow->Flash(FLinearColor(1.f, 1.f, 1.f, 0.85f), 0.28f); }
}

UTextureRenderTarget2D* ADarkroomCharacter::TakePhoto()
{
    if (!FApp::CanEverRender()) { return nullptr; }
    UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(this);
    Target->RenderTargetFormat = RTF_RGBA8_SRGB;
    Target->ClearColor = FLinearColor::Black;
    Target->InitAutoFormat(320, 240);
    Target->UpdateResourceImmediate(true);
    // 借取景器的镜头拍这一张，拍完把取景器换回去。逐帧模式下直接调 CaptureScene 引擎会报警，先关一下。
    UTextureRenderTarget2D* Viewfinder = PhotoCapture->TextureTarget;
    const bool bLive = PhotoCapture->bCaptureEveryFrame;
    PhotoCapture->bCaptureEveryFrame = false;
    PhotoCapture->TextureTarget = Target;
    PhotoCapture->CaptureScene();
    PhotoCapture->TextureTarget = Viewfinder;
    PhotoCapture->bCaptureEveryFrame = bLive;
    return Target;
}

void ADarkroomCharacter::ToggleNotebook()
{
    if (UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>()) { Flow->SetNotebookOpen(!Flow->bNotebookOpen); }
}

void ADarkroomCharacter::ToggleEra()
{
    if (UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>()) { Flow->ToggleEra(); }
}

void ADarkroomCharacter::ClickNotebook()
{
    UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (!Flow || !Flow->bNotebookOpen || !Notebook.IsValid()) { return; }
    int32 Index = INDEX_NONE;
    switch (Notebook->HitTest(Flow->Cursor, Index))
    {
    case EDarkroomHot::Entry: Flow->ClickEntry(Index); break;
    case EDarkroomHot::Slot: Flow->ClickSlot(Index); break;
    case EDarkroomHot::Develop: Flow->Develop(); break;
    default: break;
    }
}

void ADarkroomCharacter::ResetLevel()
{
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void ADarkroomCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (Flow && Flow->IsReady())
    {
        const bool bCanScrub = Flow->Era == EDarkroomEra::Film && !Flow->bNotebookOpen;
        Flow->Director->SetTransport(bCanScrub ? (bForwardHeld ? 1 : 0) - (bRewindHeld ? 1 : 0) : 0);
        UpdateFocus();
        Flow->CheckPassage(GetActorLocation());
    }
    UpdateNotebook(DeltaSeconds);
}

void ADarkroomCharacter::UpdateFocus()
{
    Focus = nullptr; FocusClue = INDEX_NONE;
    const UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (!Flow || !Flow->IsReady() || Flow->bNotebookOpen || Flow->bSliceComplete) { return; }
    const bool bFilm = Flow->Era == EDarkroomEra::Film;
    const ADarkroomDirector* Director = Flow->Director;
    const auto Useful = [bFilm, Director](const ADarkroomProp* Prop)
    {
        return Prop && !Prop->IsHidden() && (bFilm ? Director->ClueFor(Prop) != INDEX_NONE : !Prop->UsePrompt.IsEmpty());
    };

    const FVector Eye = Camera->GetComponentLocation();
    const FVector Aim = Camera->GetForwardVector();
    const float Range = bFilm ? PlayerFilmRange : PlayerReachRange;
    const FCollisionQueryParams Params(SCENE_QUERY_STAT(DarkroomFocus), false, this);

    // 小东西先吸一下准星：信、花、闩都只有巴掌大，隔着屋子瞄不准。
    ADarkroomProp* Best = nullptr;
    float BestAngle = PlayerAimAssistDegrees;
    for (const auto& Pair : Flow->CurrentRoom()->Props)
    {
        ADarkroomProp* Prop = Pair.Value;
        if (!Useful(Prop)) { continue; }
        FVector Origin, Extent;
        Prop->GetActorBounds(true, Origin, Extent);
        if (Extent.GetMax() > 30.f) { continue; }
        const FVector ToProp = Origin - Eye;
        const float Distance = ToProp.Size();
        if (Distance > Range || Distance < 1.f) { continue; }
        const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToProp / Distance, Aim), -1.0, 1.0)));
        if (Angle >= BestAngle) { continue; }
        FHitResult Blocker;
        if (GetWorld()->LineTraceSingleByChannel(Blocker, Eye, Origin, ECC_Visibility, Params) && Blocker.GetActor() != Prop) { continue; }
        Best = Prop; BestAngle = Angle;
    }
    if (!Best)
    {
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Aim * Range, ECC_Visibility, Params))
        {
            ADarkroomProp* Prop = Cast<ADarkroomProp>(Hit.GetActor());
            if (Useful(Prop)) { Best = Prop; }
        }
    }
    Focus = Best;
    if (Best && bFilm) { FocusClue = Director->ClueFor(Best); }
}

FText ADarkroomCharacter::GetPrompt() const
{
    const UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (!Flow || !Focus.IsValid()) { return FText::GetEmpty(); }
    if (Flow->Era == EDarkroomEra::Film)
    {
        if (FocusClue == INDEX_NONE) { return FText::GetEmpty(); }
        return (Flow->HasScanned(DarkroomReel::Clues()[FocusClue].Id) ? DR_TEXT("已记下", "Noted") : DR_TEXT("左键 拍照", "LMB  Photograph")).Get();
    }
    return Focus->UsePrompt.Get();
}

bool ADarkroomCharacter::IsFocusHot() const
{
    const UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    if (!Flow || !Focus.IsValid()) { return false; }
    return Flow->Era != EDarkroomEra::Film || (FocusClue != INDEX_NONE && !Flow->HasScanned(DarkroomReel::Clues()[FocusClue].Id));
}

void ADarkroomCharacter::UpdateNotebook(float DeltaSeconds)
{
    const UDarkroomFlowSubsystem* Flow = GetWorld()->GetSubsystem<UDarkroomFlowSubsystem>();
    const bool bOpen = Flow && Flow->bNotebookOpen;
    NotebookAlpha = FMath::FInterpConstantTo(NotebookAlpha, bOpen ? 1.f : 0.f, DeltaSeconds, 5.f);
    const bool bShow = NotebookAlpha > 0.01f;
    if (NotebookPivot->IsVisible() != bShow) { NotebookPivot->SetVisibility(bShow, true); }
    // 从下面举到眼前。
    const float Eased = FMath::SmoothStep(0.f, 1.f, NotebookAlpha);
    NotebookPivot->SetRelativeLocation(FMath::Lerp(PlayerPageLowered, PlayerPageRaised, Eased));
    NotebookPivot->SetRelativeRotation(FMath::Lerp(FRotator(-55, 0, 0), FRotator::ZeroRotator, Eased));
    if (bShow)
    {
        // 本子的大小跟着画面走：横向不超过画面的 78%，纵向不超过 84%。窗口窄的时候（编辑器里的小视口）就缩一点，
        // 不靠"拿远一点"来缩，离远了面对墙站会穿进墙里。
        float Fit = 1.f;
        const APlayerController* PC = Cast<APlayerController>(GetController());
        const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
        FSceneViewProjectionData Projection;
        if (LP && LP->ViewportClient && LP->ViewportClient->Viewport && LP->GetProjectionData(LP->ViewportClient->Viewport, Projection))
        {
            const double ScaleX = Projection.ProjectionMatrix.M[0][0], ScaleY = Projection.ProjectionMatrix.M[1][1];
            if (ScaleX > KINDA_SMALL_NUMBER && ScaleY > KINDA_SMALL_NUMBER)
            {
                const FVector2D PageSize = SDarkroomNotebook::PageSize();
                const double HalfWidth = PlayerPageWidthCm * 0.5, HalfHeight = HalfWidth * PageSize.Y / PageSize.X;
                Fit = float(FMath::Min3(1.0, 0.78 * PlayerPageRaised.X / (ScaleX * HalfWidth), 0.84 * PlayerPageRaised.X / (ScaleY * HalfHeight)));
            }
        }
        NotebookPivot->SetRelativeScale3D(FVector(Fit));
    }
    if (Notebook.IsValid() && bShow) { Notebook->Sync(); }
}

// 两个时代各一套镜头：曝光锁死，过去偏黄、有颗粒和暗角。
void ADarkroomCharacter::ApplyEra(EDarkroomEra Era)
{
    const bool bFilm = Era == EDarkroomEra::Film;
    FPostProcessSettings& Settings = Camera->PostProcessSettings;
    Settings.bOverride_AutoExposureMethod = true; Settings.AutoExposureMethod = AEM_Manual;
    Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true; Settings.AutoExposureApplyPhysicalCameraExposure = false;
    Settings.bOverride_AutoExposureBias = true; Settings.AutoExposureBias = 0.f;
    Settings.bOverride_ColorSaturation = true; Settings.ColorSaturation = FVector4(1, 1, 1, bFilm ? 0.72f : 0.92f);
    Settings.bOverride_ColorGain = true; Settings.ColorGain = bFilm ? FVector4(1.06f, 0.98f, 0.84f, 1) : FVector4(0.96f, 1.f, 1.05f, 1);
    Settings.bOverride_FilmGrainIntensity = true; Settings.FilmGrainIntensity = bFilm ? 0.45f : 0.f;
    Settings.bOverride_VignetteIntensity = true; Settings.VignetteIntensity = bFilm ? 0.75f : 0.35f;
    Settings.bOverride_MotionBlurAmount = true; Settings.MotionBlurAmount = 0.f;
    PhotoCapture->PostProcessSettings = Settings;
    PhotoCapture->PostProcessBlendWeight = 1.f;
    PhotoCapture->bCaptureEveryFrame = bFilm && FApp::CanEverRender();      // 只有在过去里才拍照
}
