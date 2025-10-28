# Capability System

## Overview
Capability System is a lightweight gameplay framework that lets you assemble actor behaviour from small, network-aware modules called *capabilities*. Each capability can decide where it executes (server, owning client, every client, etc.), access replicated data components, and optionally manage Enhanced Input bindings. Designers can work entirely in AngelScript while sharing the same lifecycle and networking rules as C++ counterparts.

## Core Types
- **`UCapabilityComponent`** – the manager that lives on an actor. It is responsible for loading capability sets, creating capability instances, ticking them each frame, and replicating state to clients. Every pawn, character, or actor that should run capabilities needs exactly one of these components.
- **`UCapabilitySet` & `UCapabilitySetCollection`** – data assets created in the editor. A set lists the capability classes (and optional data component classes) that should spawn together. Collections group several sets so you can apply a full loadout in one call. Reordering entries inside a set instantly changes execution priority without touching code.
- **`UCapability`** – the base class you extend for gameplay behaviour. It provides the lifecycle hooks (start, activation, tick, end) and handles networking based on `ExecuteSide`.
- **`UCapabilityInput`** – a convenience subclass of `UCapability` that adds Enhanced Input helpers (`OnBindActions`, `OnBindInputMappingContext`, action binding utilities). Use this whenever the capability reacts to player input.
- **`UCapabilityDataComponent`** – a replicated actor component spawned alongside a capability set to store shared runtime data (cooldowns, references, UI widgets, etc.). Capabilities grab them via `GetC` / `GetCA`.
- **`UInputAssetManager` & `UInputAssetManagerBind`** – a subsystem plus blueprint-callable helper class. Register `UInputAction` and `UInputMappingContext` assets in Project Settings → Input Asset Manager, then call `UInputAssetManagerBind::Action` / `::IMC` in your capabilities to fetch them without manual loading.

### Ordered Execution
Capabilities run in the order they appear inside a `UCapabilitySet`. Because the order is asset-driven, you can adjust execution priority simply by rearranging entries in the set instead of hard-coding dependencies.

## Execution Sides
Set the execution mode with `default ExecuteSide` inside your capability.  

| Mode | Runs On | Typical Use |
| ---- | ------- | ----------- |
| `Always` | Every instance, including dedicated servers | Timers, replicated state machines |
| `AuthorityOnly` | Server only | Inventory updates, authoritative gameplay rules |
| `LocalControlledOnly` | Locally controlled pawn/controller | Input sampling, local prediction |
| `AuthorityAndLocalControlled` | Server + owning client | Client prediction with server reconciliation |
| `OwnerLocalControlledOnly` | Only the locally controlled owner (supports nested ownership) | Gadgets attached to player-owned actors |
| `AllClients` | Every client except dedicated servers | Visual/audio feedback, pure UI |

`UCapabilityComponent` can also operate in `Authority` mode (server spawns capabilities and replicates them) or `Local` mode (spawn locally for tools, previews, or standalone gameplay).

## Capability Lifecycle (AngelScript)
AngelScript exposes the same hooks as the C++ classes, but the day-to-day pattern is slightly different: you only subclass `UCapability` or `UCapabilityInput`, and there is no `BeginPlay()` override. The template below lists every overridable function in the order they may run.

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

    // Evaluated every update while the capability is ACTIVE. Return true to stay active.
    bool ShouldActive() override { return true; }

    // Evaluated every update while the capability is INACTIVE. Return true to stay inactive.
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
- `StartLife` is the only hook that runs on *every* machine, even if `ExecuteSide` would normally skip that side. Use it for seed data that must exist everywhere (replicated defaults, tags, etc.).
- `Setup`, `ShouldActive`, `ShouldDeactivate`, `Tick`, and `EndCapability` respect `ExecuteSide`. They only execute where `ShouldRunOnThisSide()` returns true (for example, the owning client in `LocalControlledOnly` mode).
- The activation checks form a simple state machine:
  - While active, `ShouldActive` is asked “should I keep going?” Returning `false` queues a deactivation.
  - While inactive, `ShouldDeactivate` is asked “should I stay off?” Returning `false` requests activation.
- `OnActivated` and `OnDeactivated` run only on the side that performed the transition. If other machines need to react, communicate via replicated data components or RPCs.
- `Tick` is skipped automatically whenever the capability is inactive, `CanEverTick` is false, or the network side is not allowed to run it.
- `EndCapability` respects `ExecuteSide`, while `EndLife` runs on every side (even ones that never ticked) to guarantee that final cleanup happens everywhere.

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

`UInputAssetManagerBind` is designed as an ergonomic helper: once you register your `UInputAction` and `UInputMappingContext` assets in Project Settings → Input Asset Manager, you can reference them with `UInputAssetManagerBind::Action(Name)` or `::IMC(Name)` directly from code without manual loading.

## Managing Capability Sets
1. Create a `UCapabilitySet` asset and add your capability classes along with any required `UCapabilityDataComponent` classes. Order matters: the list determines the execution sequence.
2. Optionally group multiple sets inside a `UCapabilitySetCollection` to apply an entire loadout at once.
3. Add `UCapabilityComponent` to your actor and assign default sets/collections in the details panel, or call `AddCapabilitySet` / `RemoveCapabilitySet` at runtime.

Because execution order is asset-driven, you can reprioritize behaviour simply by reshuffling entries in the capability set. No code changes are required to coordinate dependencies between abilities.

## Debugging & Tips
- `GetCapabilityComponentStates` returns a snapshot of all capabilities hosted by a component (name, active state, execute side).
- `GetString()` prints a tree of capability sets and instances for quick in-game inspection.
- Use `stat Capability` to monitor total and ticking capability counts.
- Prefer `SetCanEverTick(false)` for event-driven capabilities; re-enable ticking only when necessary.
- Block mutually exclusive abilities with `BlockCapability(Tag, Source)` / `UnBlockCapability` instead of spreading tag checks across code.

Capability System emphasises predictable lifecycle hooks, explicit network routing, and data-driven ordering, so you can extend gameplay behaviour safely and iteratively using AngelScript. 
