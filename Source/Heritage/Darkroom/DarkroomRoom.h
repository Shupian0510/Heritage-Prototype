// 书房：一间灰盒房间，"现在"和"胶卷里"各盖一间，摆设一样，状态不同。
// 改自 Richard 原型的 HeritageDemoRoom：还是用基础形状在代码里盖，房间换成了新谜题要的书房。
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarkroomTypes.h"
#include "DarkroomRoom.generated.h"

class ADarkroomProp;
class UPointLightComponent;
class UStaticMeshComponent;

UCLASS()
class ADarkroomRoom : public AActor
{
    GENERATED_BODY()
public:
    ADarkroomRoom();
    void Build();
    ADarkroomProp* Find(FName Id) const;
    FVector ToWorld(FVector Local) const { return GetActorTransform().TransformPosition(Local); }
    FVector ToLocal(FVector World) const { return GetActorTransform().InverseTransformPosition(World); }

    EDarkroomEra Era = EDarkroomEra::Reality;
    UPROPERTY() TMap<FName, TObjectPtr<ADarkroomProp>> Props;
    UPROPERTY() TObjectPtr<UPointLightComponent> MainLight;     // 屋里的底光，时间轴会按火的状态调它
private:
    UStaticMeshComponent* Box(FName Name, FVector Center, FVector Size, FLinearColor Color, bool bCollide = true, bool bVisible = true, bool bEmissive = false);
    UPointLightComponent* Lamp(FName Name, FVector Position, FLinearColor Color, float Candelas, float Radius, bool bShadows);
    ADarkroomProp* Spawn(FName Id, EDarkroomKind Kind, FName Noun, EDarkroomShape Shape, FVector Size, FLinearColor Color);
    void BuildShell();
    void BuildProps();
};
