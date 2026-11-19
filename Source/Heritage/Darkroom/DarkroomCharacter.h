// 原型 3「暗房」的玩家：走、看、按住 E/Q 放时间、左键拍照、Tab 翻笔记本、F 进出过去。
// 改自 Richard 原型的 HeritageCharacter + HeritageInteractorComponent：
// 去掉了跳、蹲、冲刺、抓、扔、固定机位和档案馆导航；相机改成"拍下来记进笔记本"。
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DarkroomTypes.h"
#include "DarkroomCharacter.generated.h"

class UCameraComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UInputAction;
class UInputMappingContext;
class UTextureRenderTarget2D;
class ADarkroomProp;
class SDarkroomNotebook;
struct FInputActionValue;

UCLASS()
class ADarkroomCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ADarkroomCharacter();
    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PawnClientRestart() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    void ApplyEra(EDarkroomEra Era);
    ADarkroomProp* GetFocus() const { return Focus.Get(); }
    int32 GetFocusClue() const { return FocusClue; }
    FText GetPrompt() const;
    bool IsFocusHot() const;                // 准星该不该亮

    // 动词。输入和调试命令都走这里。
    void Use();
    void Shoot();
    void ToggleNotebook();
    void ToggleEra();
    void ClickNotebook();
    void SetHeld(int32 InDir, bool bDown);
    UTextureRenderTarget2D* TakePhoto();

    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> PhotoCapture;
    UPROPERTY() TObjectPtr<USceneComponent> NotebookPivot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> NotebookCover;
    UPROPERTY() TObjectPtr<UWidgetComponent> NotebookPage;
private:
    void InstallMappingContext();
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void UsePressed();
    void UseReleased();
    void RewindPressed();
    void RewindReleased();
    void ShootPressed();
    void ResetLevel();
    void UpdateFocus();
    void UpdateNotebook(float DeltaSeconds);

    UPROPERTY() TObjectPtr<UInputMappingContext> MappingContext;
    UPROPERTY() TObjectPtr<UInputAction> MoveAction;
    UPROPERTY() TObjectPtr<UInputAction> LookAction;
    UPROPERTY() TObjectPtr<UInputAction> UseAction;
    UPROPERTY() TObjectPtr<UInputAction> RewindAction;
    UPROPERTY() TObjectPtr<UInputAction> ShootAction;
    UPROPERTY() TObjectPtr<UInputAction> NotebookAction;
    UPROPERTY() TObjectPtr<UInputAction> EraAction;
    UPROPERTY() TObjectPtr<UInputAction> ResetAction;

    TWeakObjectPtr<ADarkroomProp> Focus;
    int32 FocusClue = INDEX_NONE;
    bool bForwardHeld = false;
    bool bRewindHeld = false;
    float NotebookAlpha = 0.f;
    TSharedPtr<SDarkroomNotebook> Notebook;
};
