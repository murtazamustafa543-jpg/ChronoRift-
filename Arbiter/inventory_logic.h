#pragma once

#include "../common.h"

struct InventoryPlacementPlan
{
    int can_place;
    int slot_start;
    int evict_count;
    int evict_total_slots;
    int evict_indices[MAX_WEAPON_COPIES];
    int evict_unique_ids[MAX_WEAPON_COPIES];
    int evict_weapon_ids[MAX_WEAPON_COPIES];
};

inline const char* inventory_weapon_name(int weapon_id)
{
    switch (weapon_id) {
    case WEAPON_SOLAR_CORE: return "Solar Core";
    case WEAPON_LUNAR_BLADE: return "Lunar Blade";
    case WEAPON_IRON_HALBERD: return "Iron Halberd";
    case WEAPON_VENOM_DAGGER: return "Venom Dagger";
    case WEAPON_THUNDERSTAFF: return "Thunderstaff";
    case WEAPON_OBSIDIAN_AXE: return "Obsidian Axe";
    case WEAPON_FROSTBOW: return "Frostbow";
    case WEAPON_SPLINTER_STICK: return "Splinter Stick";
case WEAPON_ECLIPSE_RELIC: return "Eclipse Relic";
    default: return "weapon";
    }
}

inline int inventory_weapon_slots(int weapon_id)
{
    switch (weapon_id) {
    case WEAPON_SOLAR_CORE: return 10;
    case WEAPON_LUNAR_BLADE: return 10;
    case WEAPON_IRON_HALBERD: return 7;
    case WEAPON_VENOM_DAGGER: return 4;
    case WEAPON_THUNDERSTAFF: return 6;
    case WEAPON_OBSIDIAN_AXE: return 5;
    case WEAPON_FROSTBOW: return 6;
    case WEAPON_ECLIPSE_RELIC: return 8;
    case WEAPON_SPLINTER_STICK: return 2;
    default: return 0;
    }
}

inline void inventory_reset_plan(InventoryPlacementPlan* plan)
{
    plan->can_place = 0;
    plan->slot_start = -1;
    plan->evict_count = 0;
    plan->evict_total_slots = 0;
    for (int i = 0; i < MAX_WEAPON_COPIES; i++) {
        plan->evict_indices[i] = -1;
        plan->evict_unique_ids[i] = -1;
        plan->evict_weapon_ids[i] = WEAPON_NONE;
    }
}

inline int inventory_find_contiguous_free(const int* inv, int needed)
{
    for (int i = 0; i <= INVENTORY_SLOTS - needed; i++) {
        bool ok = true;
        for (int j = i; j < i + needed; j++) {
            if (inv[j] != WEAPON_NONE) {
                ok = false;
                break;
            }
        }
        if (ok) return i;
    }
    return -1;
}

inline bool inventory_storage_contains(const EntityData* p, int unique_id)
{
    for (int i = 0; i < p->storage_count; i++) {
        if (p->storage_ids[i] == unique_id) return true;
    }
    return false;
}

inline void inventory_storage_add(EntityData* p, int unique_id)
{
    if (inventory_storage_contains(p, unique_id)) return;
    if (p->storage_count < STORAGE_SLOTS) {
        p->storage_ids[p->storage_count++] = unique_id;
    }
}

inline void inventory_storage_remove(EntityData* p, int unique_id)
{
    for (int i = 0; i < p->storage_count; i++) {
        if (p->storage_ids[i] == unique_id) {
            p->storage_ids[i] = p->storage_ids[p->storage_count - 1];
            p->storage_ids[p->storage_count - 1] = 0;
            p->storage_count--;
            return;
        }
    }
}

inline void inventory_clear_unique_from_grid(EntityData* p, int unique_id)
{
    for (int i = 0; i < INVENTORY_SLOTS; i++) {
        if (p->inventory[i] == unique_id) p->inventory[i] = WEAPON_NONE;
    }
}

inline void inventory_set_artifact_flag(EntityData* p, int weapon_id, int value)
{
    if (weapon_id == WEAPON_SOLAR_CORE) p->has_solar_core = value;
    if (weapon_id == WEAPON_LUNAR_BLADE) p->has_lunar_blade = value;
    if (weapon_id == WEAPON_ECLIPSE_RELIC) p->has_eclipse_relic = value;
}

inline bool inventory_plan_better(const InventoryPlacementPlan* candidate,
                                  const InventoryPlacementPlan* best)
{
    if (!best->can_place) return true;
    if (candidate->evict_count != best->evict_count)
        return candidate->evict_count < best->evict_count;
    if (candidate->evict_total_slots != best->evict_total_slots)
        return candidate->evict_total_slots < best->evict_total_slots;
    if (candidate->slot_start != best->slot_start)
        return candidate->slot_start < best->slot_start;
    for (int i = 0; i < candidate->evict_count; i++) {
        if (candidate->evict_unique_ids[i] != best->evict_unique_ids[i])
            return candidate->evict_unique_ids[i] < best->evict_unique_ids[i];
    }
    return false;
}

inline bool inventory_build_plan(const EntityData* p, int needed, InventoryPlacementPlan* best_plan)
{
    inventory_reset_plan(best_plan);

    int start = inventory_find_contiguous_free(p->inventory, needed);
    if (start >= 0) {
        best_plan->can_place = 1;
        best_plan->slot_start = start;
        return true;
    }

    int active_indices[MAX_WEAPON_COPIES];
    int active_count = 0;
    for (int i = 0; i < MAX_WEAPON_COPIES; i++) {
        const OwnedWeapon& ow = p->weapons[i];
        if (ow.used && !ow.in_storage) {
            active_indices[active_count++] = i;
        }
    }

    const unsigned int subset_count = (active_count >= 31) ? 0U : (1U << active_count);
    for (unsigned int mask = 1; mask < subset_count; mask++) {
        InventoryPlacementPlan candidate;
        inventory_reset_plan(&candidate);

        int temp[INVENTORY_SLOTS];
        for (int i = 0; i < INVENTORY_SLOTS; i++) temp[i] = p->inventory[i];

        for (int bit = 0; bit < active_count; bit++) {
            if ((mask & (1U << bit)) == 0) continue;

            const int idx = active_indices[bit];
            const OwnedWeapon& ow = p->weapons[idx];

            candidate.evict_indices[candidate.evict_count] = idx;
            candidate.evict_unique_ids[candidate.evict_count] = ow.unique_id;
            candidate.evict_weapon_ids[candidate.evict_count] = ow.weapon_id;
            candidate.evict_count++;
            candidate.evict_total_slots += ow.slot_count;

            for (int slot = 0; slot < INVENTORY_SLOTS; slot++) {
                if (temp[slot] == ow.unique_id) temp[slot] = WEAPON_NONE;
            }
        }

        start = inventory_find_contiguous_free(temp, needed);
        if (start < 0) continue;

        candidate.can_place = 1;
        candidate.slot_start = start;
        if (inventory_plan_better(&candidate, best_plan)) {
            *best_plan = candidate;
        }
    }

    return best_plan->can_place != 0;
}

inline void inventory_apply_evictions(EntityData* p, const InventoryPlacementPlan* plan)
{
    for (int i = 0; i < plan->evict_count; i++) {
        const int weapon_index = plan->evict_indices[i];
        if (weapon_index < 0) continue;

        OwnedWeapon& ow = p->weapons[weapon_index];
        inventory_clear_unique_from_grid(p, ow.unique_id);
        ow.in_storage = 1;
        ow.slot_start = -1;
        inventory_storage_add(p, ow.unique_id);
        
    }
}

inline int inventory_find_free_weapon_record(EntityData* p)
{
    for (int i = 0; i < MAX_WEAPON_COPIES; i++) {
        if (!p->weapons[i].used) return i;
    }
    return -1;
}

inline bool inventory_place_new_weapon(EntityData* p,
                                       int weapon_id,
                                       int unique_id,
                                       InventoryPlacementPlan* applied_plan)
{
    InventoryPlacementPlan plan;
    if (!inventory_build_plan(p, inventory_weapon_slots(weapon_id), &plan)) {
        inventory_reset_plan(&plan);
        if (applied_plan) *applied_plan = plan;
        return false;
    }

    const int free_record = inventory_find_free_weapon_record(p);
    if (free_record < 0) {
        inventory_reset_plan(&plan);
        if (applied_plan) *applied_plan = plan;
        return false;
    }

    inventory_apply_evictions(p, &plan);

    OwnedWeapon& ow = p->weapons[free_record];
    ow.used = 1;
    ow.unique_id = unique_id;
    ow.weapon_id = weapon_id;
    ow.slot_start = plan.slot_start;
    ow.slot_count = inventory_weapon_slots(weapon_id);
    ow.in_storage = 0;

    for (int i = plan.slot_start; i < plan.slot_start + ow.slot_count; i++) {
        p->inventory[i] = unique_id;
    }
    inventory_set_artifact_flag(p, weapon_id, 1);

    if (applied_plan) *applied_plan = plan;
    return true;
}

inline bool inventory_swap_in_weapon(EntityData* p,
                                     int unique_id,
                                     InventoryPlacementPlan* applied_plan)
{
    int existing_index = -1;
    for (int i = 0; i < MAX_WEAPON_COPIES; i++) {
        if (p->weapons[i].used &&
            p->weapons[i].in_storage &&
            p->weapons[i].unique_id == unique_id) {
            existing_index = i;
            break;
        }
    }

    InventoryPlacementPlan plan;
    inventory_reset_plan(&plan);
    if (existing_index < 0) {
        if (applied_plan) *applied_plan = plan;
        return false;
    }

    OwnedWeapon& ow = p->weapons[existing_index];
    if (!inventory_build_plan(p, inventory_weapon_slots(ow.weapon_id), &plan)) {
        if (applied_plan) *applied_plan = plan;
        return false;
    }

    inventory_apply_evictions(p, &plan);
    inventory_storage_remove(p, unique_id);

    ow.in_storage = 0;
    ow.slot_start = plan.slot_start;
    ow.slot_count = inventory_weapon_slots(ow.weapon_id);
    for (int i = plan.slot_start; i < plan.slot_start + ow.slot_count; i++) {
        p->inventory[i] = unique_id;
    }
   

    if (applied_plan) *applied_plan = plan;
    return true;
}
