#pragma once
#include <cstdint>

// ── Adobe Air.dll — offsets statiques AVM2 ──────────────────────────────
namespace AirOff {
    constexpr uintptr_t GC_HEAP_SLOT_RVA   = 0x1218E08;
    constexpr uintptr_t GC_HEAP_COLLECTORS  = 0xA688;
    constexpr uintptr_t GC_TO_AVM_CORE      = 0x870;
    constexpr uintptr_t CORE_LIVE_POOLS     = 0x78;
    constexpr uintptr_t NODE_NEXT           = 0x38;
    constexpr uintptr_t NODE_WEAK_POOL      = 0x40;
    constexpr uintptr_t WEAK_REF_OBJECT     = 0x00;
    constexpr uintptr_t POOL_SCRIPTS        = 0x150;
    constexpr uintptr_t LIST_COUNT          = 0x08;
    constexpr uintptr_t LIST_ENTRIES        = 0x10;
}

// ── Chaîne de jeu (depuis Main instance) ────────────────────────────────
namespace GameOff {
    constexpr uintptr_t MAIN_TO_GM          = 0x100;  // Main → GameManager
    constexpr uintptr_t GM_TO_LOCAL         = 0x600;  // GameManager → local entity
    constexpr uintptr_t GM_TO_ENTITYLIST    = 0x580;  // GameManager → EntityList
    constexpr uintptr_t ENTITY_TO_DISPLAY   = 0x4C8;  // entity → DisplayCache
    constexpr uintptr_t DISPLAY_Y           = 0x140;  // DisplayCache → Y (double)
    constexpr uintptr_t DISPLAY_X           = 0x148;  // DisplayCache → X (double)
}

// ── Offsets AOB code cave (fallback, positionnels depuis DisplayCache) ───
namespace Off {
    constexpr uintptr_t PosY            = 0x140;   // double — confirmé UKC
    constexpr uintptr_t PosX            = 0x148;   // double — confirmé UKC
    constexpr uintptr_t AttackLight     = 0x30;
    constexpr uintptr_t AttackHeavy     = 0x34;
    constexpr uintptr_t AttackState     = 0x50;
    constexpr uintptr_t AttackFrame     = 0x6C;
    constexpr uintptr_t HP              = 0x00;    // FILL
    constexpr uintptr_t MaxHP           = 0x04;    // FILL
    constexpr uintptr_t TeamID          = 0x08;    // FILL
    constexpr uintptr_t IsGrounded      = 0x0C;    // FILL
    constexpr uintptr_t FacingRight     = 0x10;    // FILL
    constexpr uintptr_t PlayerName      = 0x14;    // FILL
    constexpr uintptr_t DodgeCooldown   = 0x18;    // FILL
    constexpr uintptr_t Velocity        = 0x38;    // FILL
    constexpr uintptr_t CurrentWeapon   = 0x48;    // FILL
    constexpr uintptr_t WeaponType      = 0x10;    // FILL
    constexpr uintptr_t EntityType      = 0x58;    // FILL
    constexpr uintptr_t Transform       = 0x20;
    constexpr uintptr_t TransformMatrix = 0x38;
    constexpr uintptr_t SceneManager    = 0x0;
    constexpr uintptr_t CameraX        = 0x0;
    constexpr uintptr_t CameraY        = 0x0;
    constexpr uintptr_t CameraZoom     = 0x0;
}
