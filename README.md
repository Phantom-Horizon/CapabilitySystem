# Capability System

## Overview
Capability System is a lightweight gameplay framework that lets you assemble actor behaviour from small, network-aware modules called *capabilities*. Each capability can decide where it executes (server, owning client, every client, etc.), access replicated data components, and optionally manage Enhanced Input bindings. Designers can work entirely in AngelScript while sharing the same lifecycle and networking rules as C++ counterparts.

The architecture takes cues from the capability framework Hazelight discussed at GDC: capabilities act as tiny, composable slices of logic that can be added, reordered, or removed without touching surrounding systems. This reference implementation keeps the core ideas approachable: capabilities own their own state, opt into networking, and cooperate through tags instead of hard-coded dependencies, making it easy to iterate rapidly on gameplay while keeping codebases decoupled.

## Core Types
- **`UCapabilityComponent`**: the manager that lives on an actor. It is responsible for loading capability sets, creating capability instances, ticking them each frame, and replicating state to clients. Every pawn, character, or actor that should run capabilities needs exactly one of these components.
- **`UCapabilitySet` & `UCapabilitySetCollection`**: data assets created in the editor. A set lists the capability classes (and optional data component classes) that should spawn together. Collections group several sets so you can apply a full loadout in one call. Reordering entries inside a set instantly changes execution priority without touching code.
- **`UCapability`**: the base class you extend for gameplay behaviour. It provides the lifecycle hooks (start, activation, tick, end) and handles networking based on `ExecuteSide`.
- **`UCapabilityInput`**: a convenience subclass of `UCapability` that adds Enhanced Input helpers (`OnBindActions`, `OnBindInputMappingContext`, action binding utilities). Use this whenever the capability reacts to player input.
- **`UCapabilityDataComponent`**: a replicated actor component spawned alongside a capability set to store shared runtime data (cooldowns, references, UI widgets, etc.). Access them through the owning actor, for example `GetOwner()->FindComponentByClass(Type)` in C++ or `SomeTypeComponent::Get(Owner)` in AngelScript.
- **`UInputAssetManager` & `UInputAssetManagerBind`**: a subsystem plus blueprint-callable helper class. Register `UInputAction` and `UInputMappingContext` assets in Project Settings > Input Asset Manager, then call `UInputAssetManagerBind::Action` / `::IMC` in your capabilities to fetch them without manual loading.

### Ordered Execution
Capabilities run in the order they appear inside a `UCapabilitySet`. Because the order is asset-driven, you can adjust execution priority simply by rearranging entries in the set instead of hard-coding dependencies.

## Usage Workflow
1. **Place a manager**: Add `UCapabilityComponent` to the actor that should host capabilities. Pick `ComponentMode` (`Authority` for replicated gameplay, `Local` for standalone/editor tooling) and optionally assign preset sets or collections.
2. **Author assets**: Create `UCapabilitySet` assets that list the capability classes to spawn together. If the group needs shared state, include the matching `UCapabilityDataComponent` classes in the same set.
3. **Implement capabilities**: Derive from `UCapability` or `UCapabilityInput` in C++ or AngelScript, override the lifecycle hooks you need, and set defaults such as `ExecuteSide`, `CanEverTick`, and `TickInterval`.
4. **Register loadouts**: At runtime call `AddCapabilitySet` or `AddCapabilitySetCollection` to apply gameplay loadouts. In `Authority` mode these calls must originate on the server; in `Local` mode any instance can add/remove sets.
5. **Drive behaviour**: Use `BlockCapability` tags, data components, and capability activations to coordinate systems instead of hard references between gameplay classes.

## Execution Sides
Set the execution mode with `default ExecuteSide` inside your capability.  

| Mode | Runs On | Typical Use |
| ---- | ------- | ----------- |
| `Always` | Every instance, including dedicated servers | Timers, replicated state machines |
| `AuthorityOnly` | Server only | Inventory updates, authoritative gameplay rules |
| `LocalControlledOnly` | Locally controlled pawn/controller | Input sampling, local prediction |
| `AuthorityAndLocalControlled` | Server + owning client | Client prediction with server reconciliation |
| `OwnerLocalControlledOnly` | Only the locally controlled owner (supports nested ownership) | Gadgets attached to player-owned actors |
| `AllClients` | Every non-dedicated instance (clients, standalone, listen servers) | Visual/audio feedback, pure UI |

`UCapabilityComponent` can run in two modes: `Authority` (default) where the server owns capability creation and replicates the sub-objects to clients, and `Local` where the owning instance builds everything locally (useful for standalone tools, previews, or controller-side widgets). Pick the mode per component instance in the details panel.

## Capability Lifecycle (AngelScript)
AngelScript exposes the same hooks as the C++ classes, but the day-to-day pattern is slightly different: you subclass `UCapability` or `UCapabilityInput` and rely on the lifecycle callbacks below instead of overriding an actor-level `BeginPlay()`. The template lists every overridable function in the order they may run.

```c++
class UMyCapability : UCapability
{
    default ExecuteSide = ECapabilityExecuteSide::Always;
    default CanEverTick = false;
    default TickInterval = 0.f; // 0 = evaluate every frame while ticking

    // Runs once on every machine (server and all clients) regardless of ExecuteSide.
    void StartLife() override {}

    // Runs only on sides where ShouldRunOnThisSide() == true. Use to cache components or data pointers.
    void Setup() override {}

    // Evaluated every update while the capability is INACTIVE. Return true to request activation.
    bool ShouldActive() override { return true; }

    // Evaluated every update while the capability is ACTIVE. Return true to request deactivation.
    bool ShouldDeactivate() override { return false; }

    // Called immediately after Activate() succeeds on the local side.
    void OnActivated() override {}

    // Called immediately after Deactivate() succeeds on the local side.
    void OnDeactivated() override {}

    // Ticks at the chosen interval while active and allowed to run on this side.
    void Tick(float DeltaTime) override {}

    // Called on teardown (or when removed from a set) before EndLife(). Only runs on sides allowed by ExecuteSide.
    void EndCapability() override {}

    // Final cleanup. Always runs on every side (even if ExecuteSide skips that side).
    void EndLife() override {}
}
```

### Lifecycle Notes
- `StartLife` runs on every machine regardless of execute side. Use it for seed data that must exist everywhere (tags, initial replicated values, etc.).
- `Setup`, `Tick`, and `EndCapability` only run on sides where `ShouldRunOnThisSide()` returns true (for example, the owning client in `LocalControlledOnly` mode).
- The state machine flow is: while inactive, `ShouldActive` is asked if the capability should come online (return `true` to activate); while active, `ShouldDeactivate` is asked if it should shut down (return `true` to deactivate).
- `OnActivated` and `OnDeactivated` fire only on the side that drove the transition. Use replicated data components or RPCs if other machines must respond.
- `Tick` executes for capabilities that keep `CanEverTick` enabled and pass the side check. Use `SetTickInterval` for coarse updates or `SetEnable(false)` to temporarily remove the capability from the tick list.
- `EndCapability` respects `ExecuteSide`, while `EndLife` runs on every side (even ones that never ticked) so final cleanup always happens.

## AngelScript Flight Controller Example
The following capability shows how to wire Enhanced Input to control forward/back/strafe/ascend/descend movement using only the character and its own camera component (no explicit player-controller access). It uses `UCapabilityInput`, so controller attach/detach events and input mapping context management are handled automatically.

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

`UInputAssetManagerBind` is designed as an ergonomic helper: once you register your `UInputAction` and `UInputMappingContext` assets in Project Settings > Input Asset Manager, you can reference them with `UInputAssetManagerBind::Action(Name)` or `::IMC(Name)` directly from code without manual loading.

## Managing Capability Sets
1. Create a `UCapabilitySet` asset and add your capability classes along with any required `UCapabilityDataComponent` classes. Order matters: the list determines the execution sequence.
2. Optionally group multiple sets inside a `UCapabilitySetCollection` to apply an entire loadout at once.
3. Add `UCapabilityComponent` to your actor and assign default sets/collections in the details panel, or call `AddCapabilitySet` / `RemoveCapabilitySet` at runtime.

Because execution order is asset-driven, you can reprioritize behaviour simply by reshuffling entries in the capability set. No code changes are required to coordinate dependencies between abilities.

## Data Components & Shared State
- When a set lists `UCapabilityDataComponent` classes, the capability component automatically spawns them on the owning actor, wires them to the shared `UCapabilityMetaHead`, and destroys them again when the set is removed.
- Data components replicate (push-model) alongside their sibling capabilities, so any replicated properties you add are visible to clients as soon as the capability set syncs.
- Retrieve the shared component manually: in AngelScript call `Owner.GetComponentByClass(SomeDataComponentClass)`; in C++ call `GetOwner()->FindComponentByClass(SomeDataComponentClass)` or cache the pointer during `Setup`.
- Use data components for cross-capability state such as cooldowns, target references, or UI widgets that several capabilities need to touch.

## Input Capabilities
- Extend `UCapabilityInput` when you need Enhanced Input bindings. The component forwards `OnGetControllerAndInputComponent` whenever a local controller attaches so you can bind actions and mapping contexts in one place.
- Override `OnBindActions` / `OnBindInputMappingContext` to describe bindings. Call `UseInput()` and `StopUseInput()` to toggle the mapping context without destroying the capability.
- Configure the `UInputAssetManager` settings (Project Settings > Input Asset Manager) so your capabilities can fetch actions through `UInputAssetManagerBind::Action/IMC`.

## Networking Notes
- In `Authority` mode, add and remove capability sets from the server. The component registers each capability, its meta head, and every data component as replicated sub-objects so owners receive identical lifecycles.
- Clients defer capability `BeginPlay` until required actor components exist, preventing race conditions when data components are still initializing.
- `AllClients` executes on every non-dedicated instance (including listen servers), while `OwnerLocalControlledOnly` walks up the ownership chain so gadget actors attached to a player still respect local control.
- `Local` mode keeps all work on the current instance, which is ideal for editor utilities, standalone previews, or controller-specific UI logic.

## Debugging & Tips
- `GetCapabilityComponentStates` returns a snapshot of all capabilities hosted by a component (name, active state, execute side).
- `GetString()` prints a tree of capability sets and instances for quick in-game inspection.
- Use `stat Capability` to monitor total and ticking capability counts.
- Prefer `SetCanEverTick(false)` for event-driven capabilities; re-enable ticking only when necessary.
- Block mutually exclusive abilities with `BlockCapability(Tag, Source)` / `UnBlockCapability` instead of spreading tag checks across code.
- Forward controller changes from your pawns/controllers to the capability component (`OnControllerChanged` / `OnControllerRemoved`) so `UCapabilityInput` instances bind correctly (see `ACapabilityCharacter` and `ACapabilityController` for reference).
- Enable the `CapabilitySystemLog` category for runtime diagnostics; the component already emits warnings when assets fail to load or when replication preconditions are not met.

Capability System emphasises predictable lifecycle hooks, explicit network routing, and data-driven ordering, so you can extend gameplay behaviour safely and iteratively using AngelScript. 
