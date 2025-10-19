#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CapabilityCommon.h"
#include "CapabilityDataComponent.h"
#include "CapabilityBase.generated.h"

class UCapabilityMetaHead;
class UCapabilityComponent;

DECLARE_DWORD_COUNTER_STAT(TEXT("Capability Count"), STAT_CapabilityCount, STATGROUP_Capability);

UENUM(BlueprintType)
enum class ECapabilityExecuteSide : uint8 {
    Always UMETA(DisplayName = "Always Run"),
    AuthorityOnly UMETA(DisplayName = "Authority Only"),
    AllClients UMETA(DisplayName = "All Clients"),
    LocalControlledOnly UMETA(DisplayName = "Local Controlled Only"),
    AuthorityAndLocalControlled UMETA(DisplayName = "Authority And Local Controlled"),
    OwnerLocalControlledOnly UMETA(DisplayName = "Owner Local Controlled Only"),
};

UCLASS(NotBlueprintable)
class CAPABILITYSYSTEM_API UCapabilityBase : public UObject {
    GENERATED_BODY()

    friend class UCapabilityComponent;

    bool bHasBegunPlay = false;
    bool bHasEndedPlay = false;
    bool bHasPreEndedPlay = false;

protected:

    UPROPERTY(Replicated)
    TWeakObjectPtr<UCapabilityComponent> TargetCapabilityComponent = nullptr;

    UPROPERTY(Replicated)
    TObjectPtr<UCapabilityMetaHead> TargetMetaHead = nullptr;

    UPROPERTY(Replicated)
    int16 IndexInSet = 0;
    
    // Tick相关
    UPROPERTY(BlueprintReadOnly)
    bool bCanEverTick = true;

    UPROPERTY(BlueprintReadOnly)
    float tickInterval = 0.0f; // 0表示每帧Tick

    bool bIsTickEnabled = false;
    
    float tickTimeSum = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    ECapabilityExecuteSide executeSide = ECapabilityExecuteSide::Always;

    bool bIsCapabilityActive = false;

public:
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
    TArray<FName> Tags;
    
    UCapabilityBase(const FObjectInitializer& ObjectInitializer);

    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable)
    AActor* GetOwner() const;

    template <typename T>
    T* GetOwner() const;

    UFUNCTION(BlueprintCallable)
    UCapabilityComponent* GetCapabilityComponent() const;

    UFUNCTION(BlueprintCallable)
    FString GetString();

    /**
      * 配置能力的网络执行模式, 需要再构造时调用
      * 
      * @param Mode 执行模式，决定该能力在网络环境中的运行条件
      * 
      * 使用指南：
      * 
      * [Always] - 无条件执行（包括专用服务器）
      *   使用场景：需要在所有实例上保持一致的逻辑、状态管理、数据记录
      *   示例：状态机更新、冷却时间计算、Buff计时器、调试日志
      *   注意：会在专用服务器上执行，不适合纯表现效果
      * 
      * [AuthorityOnly] - 仅在服务器端执行
      *   使用场景：游戏核心逻辑、状态修改、需要防作弊的操作
      *   示例：伤害计算、物品生成、游戏规则判定、分数统计
      *   注意：结果会自动同步给客户端
      * 
      * [LocalControlledOnly] - 仅在本地控制的角色上执行
      *   使用场景：玩家输入处理、本地预测、仅影响自己的效果
      *   示例：移动输入处理、视角控制、本地UI交互、技能预表现
      *   注意：AI控制的角色在服务器上执行，玩家控制的角色在其客户端执行
      *
      * [AuthorityAndLocalControlled] - 仅本地控制角色以及权威端
      *   使用场景：射线检测, 并且需要权威答案, 本地则使用预测
      * 
      * [AllClients] - 在所有客户端执行（不包括专用服务器）
      *   使用场景：纯表现效果、视觉/音效反馈、不影响游戏逻辑的内容
      *   示例：粒子特效、音效播放、动画效果、环境特效、UI更新
      *   注意：专用服务器不会执行，适合纯客户端表现
      * 
      * 快速选择建议：
      * - 影响游戏状态？选 AuthorityOnly
      * - 处理玩家输入？选 LocalControlledOnly  
      * - 纯视觉/音效？选 AllClients
      * - 需要所有端同步的计时器/状态？选 Always
      * - 不确定？优先选 AuthorityOnly 保证安全性
      */
    UFUNCTION(BlueprintCallable)
    void SetExecuteSide(ECapabilityExecuteSide Mode) { executeSide = Mode; }
    
    UFUNCTION(BlueprintCallable)
    ECapabilityExecuteSide GetExecuteSide() { return executeSide; }
    
    // 需要在构造时调用
    // 决定该Capability是否具有Tick的能力
    UFUNCTION(BlueprintCallable)
    void SetCanEverTick(bool bCanTick) { bCanEverTick = bCanTick; }

    UFUNCTION(BlueprintCallable)
    bool GetCanEverTick() const { return bCanEverTick; }

    // 控制 bCanEverTick
    UFUNCTION(BlueprintCallable)
    void SetEnable(bool bEnable);

    // 可以在运行时调用
    // 设置Tick间隔
    UFUNCTION(BlueprintCallable)
    void SetTickInterval(float InInterval) { tickInterval = InInterval; }
    
    UFUNCTION(BlueprintCallable)
    float GetTickInterval() const { return tickInterval; }
    
    // 检查是否应该在当前端运行
    bool ShouldRunOnThisSide() const;
    
    // 激活管理
    UFUNCTION(BlueprintCallable)
    void Activate();

    UFUNCTION(BlueprintCallable)
    void Deactivate();

    UFUNCTION(BlueprintCallable)
    bool IsCapabilityActive() const { return bIsCapabilityActive; }

    UFUNCTION(BlueprintCallable)
    bool IsSideLocalControlled() const;

    UFUNCTION(BlueprintCallable)
    bool IsSideAuthorityOnly() const;

    UFUNCTION(BlueprintCallable)
    bool IsSideAllClients() const;
    
    //获取安全的Component
    UFUNCTION(BlueprintCallable)
    UCapabilityDataComponent* GetC(TSubclassOf<UCapabilityDataComponent> ClassOfComponent) const;

    UFUNCTION(BlueprintCallable)
    UCapabilityDataComponent* GetCA(TSubclassOf<UCapabilityDataComponent> ClassOfComponent, AActor* Target) const;

    UFUNCTION(BlueprintCallable)
    void BlockCapability(const FName& Tag, UObject* From);

    UFUNCTION(BlueprintCallable)
    void UnBlockCapability(const FName& Tag, UObject* From);
    
    // 生命周期
    UFUNCTION(BlueprintNativeEvent)
    void StartLife();
    
    UFUNCTION(BlueprintNativeEvent)
    void Setup();
    
    // 启停判定拆分：
    // - ShouldActive：仅在“当前已激活，考虑是否停用”时调用；返回 true=保持激活，false=应停用
    // - ShouldDeactivate：仅在“当前未激活，考虑是否启用”时调用；返回 true=保持未激活，false=应启用
    UFUNCTION(BlueprintNativeEvent)
    bool ShouldActive();

    UFUNCTION(BlueprintNativeEvent)
    bool ShouldDeactivate();

    UFUNCTION(BlueprintNativeEvent)
    void OnActivated();

    UFUNCTION(BlueprintNativeEvent)
    void OnDeactivated();
    
    UFUNCTION(BlueprintNativeEvent)
    void Tick(float DeltaTime);

    UFUNCTION(BlueprintNativeEvent)
    void EndCapability();

    UFUNCTION(BlueprintNativeEvent)
    void EndLife();

    void NativeBeginPlay();
    void NativeEndPlay();
    void NativePreEndPlay();
    void NativeTick(float DeltaTime);

    virtual void BeginPlay() {}
    virtual void EndPlay() {}
    virtual void UpdateCapabilityState() {}

    virtual void PreDestroyFromReplication() override;
    
    // 网络相关配置
    
    virtual bool IsSupportedForNetworking() const override { return true; }

    virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override {
        int32 Callspace = FunctionCallspace::Local;
        if (UObject* Outer = GetOuter()) Callspace = Outer->GetFunctionCallspace(Function, Stack);
        return Callspace;
    }

    virtual bool CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack) override {
        AActor* Actor = GetOwner();
        if (!Actor) return false;
        if (UNetDriver* NetDriver = Actor->GetNetDriver()) {
            NetDriver->ProcessRemoteFunction(Actor, Function, Parms, OutParms, Stack, this);
            return true;
        }
        return false;
    }
};
