#include "CapabilitySystem/Public/Capability.h"

UCapability::UCapability(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
}

void UCapability::BeginPlay() {
    Super::BeginPlay();
    StartLife();

    // 2. 判断规则
    if (!ShouldRunOnThisSide()) return;

    // 3. 初始化
    NativeInitializeCapability(); //组件初始化

    // 4. 查看是否需要激活（未激活->启用：使用 ShouldDeactivate 判定是否保持未激活）
    if (!ShouldDeactivate())
        Activate();
    else
        Deactivate();
}

void UCapability::NativeInitializeCapability() { Setup(); }

void UCapability::UpdateCapabilityState() {
    // 已激活->考虑停用：使用 ShouldActive 判定是否保持激活
    if (IsCapabilityActive()) {
        if (ShouldDeactivate()) Deactivate();
    }
    else {
        // 未激活->考虑启用：使用 ShouldDeactivate 判定是否保持未激活
        if (ShouldActive()) Activate();
    }
}