// 灰盒道具：一个基础形状加若干零件，能存取快照。
// 改自 Richard 原型的 HeritageProp + HeritageProps：去掉了物理、抓取、碰撞刺激和 ObjectComponent，
// 各种道具不再各写一个类，用 Kind 区分那一点点不同的行为。
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarkroomTypes.h"
#include "DarkroomProp.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class ADarkroomProp : public AActor
{
    GENERATED_BODY()
public:
    ADarkroomProp();
    virtual void Tick(float DeltaSeconds) override;

    void Setup(FName InId, EDarkroomKind InKind, EDarkroomEra InEra, FName InNoun, EDarkroomShape Shape, FVector SizeCm, FLinearColor Color);
    UStaticMeshComponent* AddShape(FName Name, EDarkroomShape Shape, FVector Position, FVector SizeCm, FLinearColor Color,
        FRotator Rotation = FRotator::ZeroRotator, bool bEmissive = false, bool bSolid = false);
    UPointLightComponent* AddLight(FVector Position, FLinearColor Color, float Candelas, float Radius);
    void SetGhost();                    // 挡视线（拍得到），不挡人
    void SetEmissive(FLinearColor Color);   // 根形状自己发光（提灯）
    void HideBody();                    // 根形状只用来瞄准，不画出来

    bool HasState(FGameplayTag Tag) const { return StateTags.HasTagExact(Tag); }
    void RefreshFromState();
    FDarkroomSnapshot Capture(const FTransform& Room) const;
    void Restore(const FDarkroomSnapshot& Snapshot, const FTransform& Room);

    bool OnUse();                       // 现在：对着它按 E
    void Flare(float Seconds);          // 火苗窜一下
    void TweenTo(FVector WorldLocation, FRotator WorldRotation, float Seconds);
    float GetFacingYaw() const;

    FName FilmId;
    FName Noun;
    EDarkroomKind Kind = EDarkroomKind::Plain;
    EDarkroomEra Era = EDarkroomEra::Reality;
    FDarkroomText RealityLine;          // 现在对着它按 E 时角色说的话
    FDarkroomText UsePrompt;
    FGameplayTagContainer StateTags;
    float LightCandelas = 0.f;

    UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UPointLightComponent> Light;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Flame;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Extras;    // 跟着 Filled 状态显示的零件（花）
private:
    static UStaticMesh* LoadShape(EDarkroomShape Shape);
    static UMaterialInstanceDynamic* MakeMaterial(UObject* Outer, FLinearColor Color, bool bEmissive);
    FRotator BaseRotation = FRotator::ZeroRotator;
    FRotator HomeRotation = FRotator::ZeroRotator;  // 胸像的原位：转过之后会自己回到这里
    float ReturnTimer = 0.f;                        // 胸像还能在转过去的位置上停多久
    float FlareUntil = 0.f;
    bool bTweening = false;
    float TweenAlpha = 0.f;
    float TweenDuration = 0.f;
    FVector TweenFromLocation = FVector::ZeroVector;
    FVector TweenToLocation = FVector::ZeroVector;
    FQuat TweenFromRotation = FQuat::Identity;
    FQuat TweenToRotation = FQuat::Identity;
};
