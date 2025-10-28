# 能力系统
<p>
  <u><a href="README.md">English</a></u>
  <span style="display:inline-block; width: 2rem; text-align: center;">|</span>
  <u><b>&gt; 中文 &lt;</b></u>
</p>

## 概述
Capability System 是一套轻量级的玩法框架，通过称为“能力”（capability）的细粒度、具备网络感知的模块来拼装 Actor 行为。它与引擎版本解耦：无论你的项目主要使用 C++、蓝图（Blueprints）、UnrealSharp，还是 AngelScript，都可以复用同一套核心。每个能力都可以决定自己的执行侧（服务器、拥有者客户端、所有客户端等）、访问被复制的数据组件，并且可选地管理 Enhanced Input 绑定。设计师可以完全用 AngelScript 工作，同时享有与 C++ 实现一致的生命周期与网络规则。

该架构借鉴了 Hazelight 在 GDC 分享的能力框架理念：能力是小而可组合的逻辑切片，可以在不触碰周边系统的情况下添加、重排或移除。本参考实现保持理念的易用性：能力管理自己的状态，可选择性参与网络，同步通过标签协作而非硬编码依赖，从而在保持代码解耦的同时快速迭代玩法。

## 核心类型
- `UCapabilityComponent`：挂在 Actor 上的能力管理器。负责加载能力集、创建能力实例、逐帧 Tick，并将状态复制到客户端。任何需要运行能力的 Pawn/Character/Actor 都应恰好挂一个该组件。
- `UCapabilitySet` 与 `UCapabilitySetCollection`：在编辑器中创建的数据资产。Set 罗列要一同生成的能力类（以及可选的数据组件类），Collection 用于将多个 Set 组合，以便一次性应用完整“装配”。在 Set 中调整条目顺序即可立刻改变执行优先级，无需改代码。
- `UCapability`：用于扩展玩法行为的基类。提供生命周期钩子（Start、Activation、Tick、End），并依据 `ExecuteSide` 处理网络。
- `UCapabilityInput`：`UCapability` 的便捷子类，增加 Enhanced Input 相关辅助（`OnBindActions`、`OnBindInputMappingContext`、动作绑定工具）。当能力需要响应玩家输入时使用。
- `UCapabilityDataComponent`：与能力集一起生成的可复制 Actor 组件，用于存放共享的运行时数据（冷却、引用、UI 小部件等）。可在拥有者上访问，例如 C++ 中的 `GetOwner()->FindComponentByClass(Type)` 或 AngelScript 中的 `SomeTypeComponent::Get(Owner)`。
- `UInputAssetManager` 与 `UInputAssetManagerBind`：子系统与蓝图可调用的辅助类。在“项目设置 > Input Asset Manager”中注册 `UInputAction` 和 `UInputMappingContext` 资源，然后在能力中调用 `UInputAssetManagerBind::Action` / `::IMC` 即可获取，无需手动加载。

### 有序执行
能力会按其在 `UCapabilitySet` 中的排列顺序运行。由于顺序由资产驱动，你只需在资产中重排条目，就能调整执行优先级，而无需写死代码依赖。

## 使用流程
1. 放置管理器：将 `UCapabilityComponent` 添加到需要承载能力的 Actor 上。选择 `ComponentMode`（复制玩法选 `Authority`，单机/编辑器工具选 `Local`），并可选配置默认的 Set 或 Collection。
2. 产出资产：创建 `UCapabilitySet`，列出要一同生成的能力类。如该组需要共享状态，在同一个 Set 中加入对应的 `UCapabilityDataComponent` 类。
3. 实现能力：在 C++ 或 AngelScript 中派生自 `UCapability` 或 `UCapabilityInput`，覆写所需的生命周期钩子，并设置诸如 `ExecuteSide`、`CanEverTick`、`TickInterval` 等默认值。
4. 注册装配：运行期调用 `AddCapabilitySet` 或 `AddCapabilitySetCollection` 应用玩法装配。`Authority` 模式下必须在服务器端调用；`Local` 模式下任何实例都可以增删 Set。
5. 驱动行为：用 `BlockCapability` 标签、数据组件与能力的激活/失活来协调系统，而不是在玩法类之间做硬引用。

## 执行侧（Execution Sides）
在能力类中通过 `default ExecuteSide` 设置执行模式。

| 模式 | 运行于 | 常见用途 |
| ---- | ------ | -------- |
| `Always` | 所有实例（含专用服务器） | 计时器、复制的状态机 |
| `AuthorityOnly` | 仅服务器 | 物品/库存更新、权威玩法规则 |
| `LocalControlledOnly` | 本地控制的 Pawn/Controller | 输入采样、本地预测 |
| `AuthorityAndLocalControlled` | 服务器 + 拥有者客户端 | 客户端预测 + 服务器回滚校正 |
| `OwnerLocalControlledOnly` | 仅本地控制的拥有者（支持嵌套所有权） | 挂在玩家拥有 Actor 上的装置/道具 |
| `AllClients` | 所有非专用实例（客户端、单机、监听服） | 视觉/音频反馈、纯 UI |

`UCapabilityComponent` 支持两种模式：`Authority`（默认）下，服务器负责创建能力并将其子对象复制到客户端；`Local` 下，拥有者实例在本地构建全部对象（适合单机工具、预览或控制器侧 UI）。可在 Details 面板为每个组件单独选择模式。

<details>
<summary>

### 能力生命周期（AngelScript）

</summary>
AngelScript 暴露与 C++ 相同的生命周期钩子，但日常模式略有不同：你继承 `UCapability` 或 `UCapabilityInput`，并依赖下面的生命周期回调，而非覆写 Actor 级的 `BeginPlay()`。模板按可能执行的顺序列出了全部可覆写函数。

```c++
class UMyCapability : UCapability
{
    default ExecuteSide = ECapabilityExecuteSide::Always;
    default CanEverTick = false;
    default TickInterval = 0.f; // 0 表示在启用 Tick 时每帧评估

    // 无论 ExecuteSide 为何，在每台机器（服务器与所有客户端）各运行一次。
    void StartLife() override {}

    // 仅在 ShouldRunOnThisSide() == true 的侧运行。用于缓存组件或数据指针。
    void Setup() override {}

    // 在能力处于未激活状态时每次更新评估。返回 true 以请求激活。
    bool ShouldActive() override { return true; }

    // 在能力处于激活状态时每次更新评估。返回 true 以请求失活。
    bool ShouldDeactivate() override { return false; }

    // 当本地侧 Activate() 成功后立即调用。
    void OnActivated() override {}

    // 当本地侧 Deactivate() 成功后立即调用。
    void OnDeactivated() override {}

    // 在激活且允许于此侧运行时，按设定的间隔进行 Tick。
    void Tick(float DeltaTime) override {}

    // 在拆卸（或从集合移除）时于 EndLife() 之前调用。仅在 ExecuteSide 允许的侧运行。
    void EndCapability() override {}

    // 最终清理。始终在每一侧运行（即使该侧被 ExecuteSide 跳过）。
    void EndLife() override {}
}
```
</details>

### 生命周期说明
- `StartLife` 无论执行侧为何，都会在每台机器（服务器和所有客户端）运行。可用于初始化在各侧都需要存在的种子数据（标签、初始复制值等）。
- `Setup`、`Tick` 与 `EndCapability` 仅会在 `ShouldRunOnThisSide()` 返回 true 的侧运行（例如 `LocalControlledOnly` 时的拥有者客户端）。
- 状态机流程：非激活态（Inactive）时，每帧询问 `ShouldActive` 是否需要启动（返回 `true` 激活）；激活态（Active）时，每帧询问 `ShouldDeactivate` 是否需要停止（返回 `true` 失活）。
- `OnActivated` 与 `OnDeactivated` 只在触发该状态切换的一侧调用。若其他机器也需响应，请使用可复制的数据组件或 RPC。
- `Tick` 仅在 `CanEverTick` 开启且侧向检查通过时执行。可用 `SetTickInterval` 做粗粒度更新，或用 `SetEnable(false)` 临时将能力移出 Tick 列表。
- `EndCapability` 遵循 `ExecuteSide` 限制，而 `EndLife` 会在每一侧执行（即便该侧从未 Tick 过），以保证最终清理一定发生。

## AngelScript 飞行控制器示例
下面的能力展示了如何用 Enhanced Input 控制前进/后退、平移与上升/下降，只依赖角色及其自身的相机组件（无需直接访问 PlayerController）。该能力继承自 `UCapabilityInput`，因此控制器挂接/分离事件与输入映射上下文的管理会自动处理。

```c++
class UFlyingInputCapability : UCapabilityInput
{
    default ExecuteSide = ECapabilityExecuteSide::LocalControlledOnly;
    default CanEverTick = true;

    ACharacter CachedCharacter;
    UCameraComponent CharacterCamera;

    void Setup() override
    {
        CachedCharacter = Cast<ACharacter>(Owner);
        CharacterCamera = CachedCharacter != nullptr ? CachedCharacter.FindComponentByClass(UCameraComponent::StaticClass()) : nullptr;
    }

    void OnBindActions() override
    {
        BindAction(UInputAssetManagerBind::Action(n"Fly_Move"), ETriggerEvent::Triggered, this, n"MoveXY");
        BindAction(UInputAssetManagerBind::Action(n"Fly_Elevate"), ETriggerEvent::Triggered, this, n"MoveZ");
        BindAction(UInputAssetManagerBind::Action(n"Fly_Look"), ETriggerEvent::Triggered, this, n"RotateCamera");
    }

    void OnBindInputMappingContext() override
    {
        BindInputMappingContext(UInputAssetManagerBind::IMC(n"IMC_Flying"));
    }

    bool ShouldActive() override
    {
        return CachedCharacter != nullptr;
    }

    bool ShouldDeactivate() override
    {
        return CachedCharacter == nullptr;
    }

    void MoveXY(FInputActionValue Value)
    {
        if (CachedCharacter == nullptr) return;
        const FVector2D Axis = Value.GetAxis2D();
        const FVector Forward = CharacterCamera != nullptr ? CharacterCamera.GetForwardVector() : CachedCharacter.GetActorForwardVector();
        const FVector Right = CharacterCamera != nullptr ? CharacterCamera.GetRightVector() : CachedCharacter.GetActorRightVector();
        CachedCharacter.AddMovementInput(Forward, Axis.Y);
        CachedCharacter.AddMovementInput(Right, Axis.X);
    }

    void MoveZ(FInputActionValue Value)
    {
        if (CachedCharacter == nullptr) return;
        CachedCharacter.AddMovementInput(FVector::UpVector, Value.GetAxis1D());
    }

    void RotateCamera(FInputActionValue Value)
    {
        if (CachedCharacter == nullptr) return;
        const FVector2D Axis = Value.GetAxis2D();
        CachedCharacter.AddControllerYawInput(Axis.X);
        CachedCharacter.AddControllerPitchInput(Axis.Y);
    }
}
```

`UInputAssetManagerBind` 被设计为顺手的辅助：一旦你在“项目设置 > Input Asset Manager”里注册了 `UInputAction` 与 `UInputMappingContext`，就可以在代码中通过 `UInputAssetManagerBind::Action(Name)` 或 `::IMC(Name)` 直接引用，而无需手动加载。

## 管理能力集
1. 创建 `UCapabilitySet` 资产，并加入你的能力类以及所需的 `UCapabilityDataComponent` 类。注意顺序：列表顺序即执行顺序。
2. 可选地将多个 Set 组合为 `UCapabilitySetCollection`，以便一次性应用整套装配。
3. 将 `UCapabilityComponent` 挂到 Actor 上，并在 Details 面板中指定默认的 Set/Collection，或在运行期调用 `AddCapabilitySet` / `RemoveCapabilitySet`。

由于执行顺序由资产驱动，你可以仅通过在能力集中重排条目来调整行为优先级，无需改动代码，也无需在能力之间硬编码依赖。

## 数据组件与共享状态
- 当某个 Set 声明了 `UCapabilityDataComponent` 类时，能力组件会在拥有者 Actor 上自动生成它们，将其挂到共享的 `UCapabilityMetaHead` 下，并在移除该 Set 时一并销毁。
- 数据组件以推送模型（push-model）参与复制，与其同级能力一起同步，因此你添加的可复制属性会在能力集同步时立刻对客户端可见。
- 手动获取共享组件：在 AngelScript 中用 `Owner.GetComponentByClass(SomeDataComponentClass)`；在 C++ 中用 `GetOwner()->FindComponentByClass(SomeDataComponentClass)`，或在 `Setup` 期间缓存指针。
- 将数据组件用于跨能力共享的状态，例如冷却、目标引用，或被多个能力触达的 UI 小部件。

## 输入型能力
- 当需要 Enhanced Input 绑定时，继承 `UCapabilityInput`。当本地控制器挂接时，组件会转发 `OnGetControllerAndInputComponent`，你可以在同一处完成动作与映射上下文的绑定。
- 覆写 `OnBindActions` / `OnBindInputMappingContext` 描述绑定。通过 `UseInput()` 与 `StopUseInput()` 在不销毁能力的情况下开关映射上下文。
- 配置 `UInputAssetManager`（项目设置 > Input Asset Manager），这样能力就可以通过 `UInputAssetManagerBind::Action/IMC` 获取动作与映射。

## 网络说明
- 在 `Authority` 模式下，应在服务器端增删能力集。组件会将每个能力、其 Meta Head 以及所有数据组件注册为可复制子对象，确保拥有者在各端拥有一致的生命周期。
- 客户端会延后能力的 `BeginPlay`，直到所需 Actor 组件就绪，避免数据组件初始化时的竞争条件。
- `AllClients` 会在所有非专用实例（含监听服）执行；`OwnerLocalControlledOnly` 会沿所有权链向上查找，因此挂在玩家拥有 Actor 上的小装置也能正确遵循本地控制。
- `Local` 模式将所有工作保留在当前实例，适合编辑器工具、单机预览或仅限控制器侧的 UI 逻辑。

## 调试与技巧
- `GetCapabilityComponentStates` 返回组件所承载的全部能力快照（名称、激活状态、执行侧）。
- `GetString()` 打印能力集与实例的树状结构，便于快速游戏内检查。
- 使用 `stat Capability` 监控能力总数与正在 Tick 的能力数量。
- 对事件驱动型能力优先设置 `SetCanEverTick(false)`；仅在必要时再开启 Tick。
- 用 `BlockCapability(Tag, Source)` / `UnBlockCapability` 屏蔽互斥能力，避免在各处分散写标签判断。
- 从 Pawn/Controller 将控制器变更转发到能力组件（`OnControllerChanged` / `OnControllerRemoved`），以便 `UCapabilityInput` 能正确绑定（可参考 `ACapabilityCharacter` 与 `ACapabilityController`）。
- 启用 `CapabilitySystemLog` 日志类别以获取运行期诊断；当资产加载失败或复制前置条件不满足时，组件也会给出警告。

Capability System 强调可预测的生命周期钩子、明确的网络路由与数据驱动的执行顺序，你可以用 AngelScript、UnrealSharp 等脚本语言在保证安全与可迭代的前提下，持续扩展玩法行为。
