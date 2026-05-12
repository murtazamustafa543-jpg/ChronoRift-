#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <csignal>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#include "../common.h"
#include "inventory_logic.h"
using namespace std;

static SharedData* g_sd      = nullptr;
static int         shmid     = -1;
static volatile    sig_atomic_t g_running = 1;
static bool        g_deadlock_test_mode = false;

// same order as WEAPON_* ids
struct WeaponStat { int slots; int damage; };
static const WeaponStat WEAPON_TABLE[] = {
    {0,  0 },
    {10, 95},
    {10, 90},
    {7,  55},
    {4,  30},
    {6,  50},
    {5,  45},
    {6,  48},
    {2,  12},
    {8, 85},
};

static void handle_sigterm(int) {
    g_running = 0;
    if (g_sd) {
        g_sd->game_over      = 1;
        g_sd->quit_requested = 1;
        g_sd->winner         = WINNER_QUIT;
    }
}
static void log_event_s(const char* message) {
    if (!g_sd) return;
    sem_wait(&g_sd->state_lock);
    // newest line stays on top
    for (int i = MAX_LOG_LINES - 1; i > 0; i--)
        strncpy(g_sd->log_lines[i], g_sd->log_lines[i-1], LOG_TEXT-1);
    strncpy(g_sd->log_lines[0], message, LOG_TEXT-1);
    g_sd->log_lines[0][LOG_TEXT-1] = '\0';
    if (g_sd->log_count < MAX_LOG_LINES) g_sd->log_count++;
    sem_post(&g_sd->state_lock);
    printf("[Arbiter] %s\n", message);
}

static void handle_sigalrm(int) {
    if (g_sd && g_sd->asp_pid > 0) {
        kill(g_sd->asp_pid, SIGCONT);
        log_event_s("Ultimate window expired. ASP resumed.");
    }
}

static void log_event_locked(const char* message) {
    if (!g_sd) return;
    for (int i = MAX_LOG_LINES - 1; i > 0; i--)
        strncpy(g_sd->log_lines[i], g_sd->log_lines[i-1], LOG_TEXT-1);
    strncpy(g_sd->log_lines[0], message, LOG_TEXT-1);
    g_sd->log_lines[0][LOG_TEXT-1] = '\0';
    if (g_sd->log_count < MAX_LOG_LINES) g_sd->log_count++;
    printf("[Arbiter] %s\n", message);
}

// caller already holds state_lock here
static void place_weapon(EntityData* p, int weapon_id, int unique_id) {
    InventoryPlacementPlan plan;
    if (!inventory_place_new_weapon(p, weapon_id, unique_id, &plan)) {
        log_event_locked("  [!] Inventory placement failed.");
        return;
    }

    for (int i = 0; i < plan.evict_count; i++) {
        char msg[LOG_TEXT];
        snprintf(msg, LOG_TEXT, "  >> %s moved to storage (space needed).",
                 inventory_weapon_name(plan.evict_weapon_ids[i]));
        log_event_locked(msg);
    }
}

// weapon drop after a kill
static void handle_weapon_drop(int killed_enemy_idx, int killer_player_idx) {
    int possible[] = {WEAPON_IRON_HALBERD, WEAPON_VENOM_DAGGER, WEAPON_THUNDERSTAFF, 
                      WEAPON_OBSIDIAN_AXE, WEAPON_FROSTBOW, WEAPON_SPLINTER_STICK};
    int pick = possible[rand() % 6];
    int uid = g_sd->next_weapon_unique_id++;

    // give it to killer first, else first alive player
    int target_player = killer_player_idx;
    if (target_player < 0 || target_player >= g_sd->player_count || !g_sd->players[target_player].alive) {
        for (int i = 0; i < g_sd->player_count; i++) {
            if (g_sd->players[i].alive) { target_player = i; break; }
        }
    }

    if (target_player < 0) {
        int e_idx = rand() % g_sd->enemy_count;
        while (!g_sd->enemies[e_idx].alive) e_idx = rand() % g_sd->enemy_count;
        char msg[LOG_TEXT];
        snprintf(msg, LOG_TEXT, "Enemy %s scavenged %s!", g_sd->enemies[e_idx].name, inventory_weapon_name(pick));
        log_event_locked(msg);
        return;
    }

    g_sd->drop_active = 1;
    g_sd->drop_weapon_id = pick;
    g_sd->drop_for_player_id = target_player;
    g_sd->drop_choice_ready = 0;
    g_sd->drop_choice_pick = 0;
    g_sd->drop_expire_time = time(nullptr) + 30;

    char msg[LOG_TEXT];
    snprintf(msg, LOG_TEXT, ">>> WEAPON DROP: %s for %s (uid=%d)", 
             inventory_weapon_name(pick), g_sd->players[target_player].name, uid);
    log_event_locked(msg);
}

// mark stun and ping the right process
static void apply_stun(EntityData* target) {
    target->stunned      = 1;
    target->stun_end_time = time(nullptr) + STUN_SECONDS;

    pid_t target_pid = 0;
    if (target->type == TYPE_PLAYER && g_sd->hip_pid > 0)
        target_pid = g_sd->hip_pid;
    else if (target->type == TYPE_ENEMY && g_sd->asp_pid > 0)
        target_pid = g_sd->asp_pid;

    if (target_pid > 0)
        kill(target_pid, SIGUSR1);

    char msg[LOG_TEXT];
    snprintf(msg, LOG_TEXT, "  !! %s is STUNNED for %d seconds!",
             target->name, STUN_SECONDS);
    log_event_locked(msg);
}

static bool all_players_dead_locked() {
    for (int i = 0; i < g_sd->player_count; i++) {
        if (g_sd->players[i].alive) return false;
    }
    return true;
}

static bool all_spawned_enemies_dead_locked() {
    for (int i = 0; i < g_sd->total_enemies_to_defeat; i++) {
        if (g_sd->enemies[i].alive) return false;
    }
    return true;
}

static void register_enemy_defeat_locked(EntityData* actor, EntityData* target, int enemy_idx, int killer_player_idx)  {
    if (!target || target->alive) return;

    target->stunned = 0;
    target->stamina = 0;
    actor->total_kills++;
    g_sd->enemies_killed_total++;

    // dead enemy drops any artifacts it had
    if (target->type == TYPE_ENEMY) {
        if (target->has_solar_core) {
            target->has_solar_core = 0;
            for (int a = 0; a < ARTIFACT_COUNT; a++) {
                if (g_sd->artifacts[a].artifact_id == ARTIFACT_SOLAR_CORE &&
                    g_sd->artifacts[a].owner_type == TYPE_ENEMY &&
                    g_sd->artifacts[a].owner_id == enemy_idx) {
                    g_sd->artifacts[a].owner_type = TYPE_NONE;
                    g_sd->artifacts[a].owner_id = -1;
                    g_sd->artifacts[a].locked_by_type = TYPE_NONE;
                    g_sd->artifacts[a].locked_by_id = -1;
                    g_sd->artifacts[a].waiting_by_type = TYPE_NONE;
                    g_sd->artifacts[a].waiting_by_id = -1;
                    log_event_locked(">>> Solar Core released back to the Rift!");
                    break;
                }
            }
        }
        if (target->has_lunar_blade) {
            target->has_lunar_blade = 0;
            for (int a = 0; a < ARTIFACT_COUNT; a++) {
                if (g_sd->artifacts[a].artifact_id == ARTIFACT_LUNAR_BLADE &&
                    g_sd->artifacts[a].owner_type == TYPE_ENEMY &&
                    g_sd->artifacts[a].owner_id == enemy_idx) {
                    g_sd->artifacts[a].owner_type = TYPE_NONE;
                    g_sd->artifacts[a].owner_id = -1;
                    g_sd->artifacts[a].locked_by_type = TYPE_NONE;
                    g_sd->artifacts[a].locked_by_id = -1;
                    g_sd->artifacts[a].waiting_by_type = TYPE_NONE;
                    g_sd->artifacts[a].waiting_by_id = -1;
                    log_event_locked(">>> Lunar Blade released back to the Rift!");
                    break;
                }
            }
        }
        if (target->has_eclipse_relic) {
            target->has_eclipse_relic = 0;
            for (int a = 0; a < ARTIFACT_COUNT; a++) {
                if (g_sd->artifacts[a].artifact_id == ARTIFACT_ECLIPSE_RELIC &&
                    g_sd->artifacts[a].owner_type == TYPE_ENEMY &&
                    g_sd->artifacts[a].owner_id == enemy_idx) {
                    g_sd->artifacts[a].owner_type = TYPE_NONE;
                    g_sd->artifacts[a].owner_id = -1;
                    g_sd->artifacts[a].locked_by_type = TYPE_NONE;
                    g_sd->artifacts[a].locked_by_id = -1;
                    g_sd->artifacts[a].waiting_by_type = TYPE_NONE;
                    g_sd->artifacts[a].waiting_by_id = -1;
                    log_event_locked(">>> Eclipse Relic released back to the Rift!");
                    break;
                }
            }
        }
        memset(target->inventory, 0, sizeof(target->inventory));
        memset(target->weapons, 0, sizeof(target->weapons));
        target->storage_count = 0;
    }

    char msg[LOG_TEXT];
    snprintf(msg, LOG_TEXT, "  !! %s defeated! (%d/%d kills)",
             target->name, g_sd->enemies_killed_total, g_sd->total_enemies_to_defeat);
    log_event_locked(msg);
    handle_weapon_drop(enemy_idx, killer_player_idx);
        if (!g_sd->eclipse_present && g_sd->enemies_killed_total >= 2) {
        g_sd->artifacts[2].present = 1;
        g_sd->eclipse_present = 1;
        log_event_locked(">>> The Eclipse Relic materializes in the Rift!");
    }
}

static void acknowledge_player_action_locked(int actor_type) {
    if (actor_type != TYPE_PLAYER) return;
    g_sd->arbiter_consumed_seq = g_sd->hip_action_seq;
    printf("[Arbiter] consumed HIP action seq=%d\n", g_sd->arbiter_consumed_seq);
}
static const char* artifact_name_from_id(int artifact_id) {
    switch (artifact_id) {
    case ARTIFACT_SOLAR_CORE: return "Solar Core";
    case ARTIFACT_LUNAR_BLADE: return "Lunar Blade";
    case ARTIFACT_ECLIPSE_RELIC: return "Eclipse Relic";
    default: return "artifact";
    }
}
static void process_pending_action() {
    sem_wait(&g_sd->state_lock);

    if (!g_sd->pending_action.ready) {
        sem_post(&g_sd->state_lock);
        return;
    }

    ActionData& pa  = g_sd->pending_action;
    int act         = pa.action;
    int actor_type  = pa.actor_type;
    int actor_id    = pa.actor_id;
    int t_type      = pa.target_type;
    int t_id        = pa.target_id;
    int w_uid       = pa.
    weapon_unique_id;
    int s_uid       = pa.storage_unique_id;

    EntityData* actor  = (actor_type == TYPE_PLAYER) ? &g_sd->players[actor_id]
                                                      : &g_sd->enemies[actor_id];
    EntityData* target = nullptr;
    if (t_type == TYPE_PLAYER && t_id >= 0) target = &g_sd->players[t_id];
    if (t_type == TYPE_ENEMY  && t_id >= 0) target = &g_sd->enemies[t_id];

    char msg[LOG_TEXT];

    switch (act) {

    case ACTION_STRIKE: {
        if (!target || !target->alive) break;
        target->hp -= actor->damage;
        if (target->hp <= 0) { target->hp = 0; target->alive = 0; }

        snprintf(msg, LOG_TEXT, "  %s STRIKES %s for %d dmg. (HP: %d/%d)",
                 actor->name, target->name, actor->damage,
                 target->hp, target->max_hp);
        log_event_locked(msg);

        if (target->alive && (rand() % 10) == 0) {
            apply_stun(target);
        }

        if (!target->alive) {
            if (t_type == TYPE_ENEMY) {
                register_enemy_defeat_locked(actor, target, t_id, actor_type == TYPE_PLAYER ? actor_id : -1);
            } else {
                actor->total_kills++;
                snprintf(msg, LOG_TEXT, "  !! %s has fallen!", target->name);
                log_event_locked(msg);
            }
        }

        actor->stamina = 0;
        break;
    }

    case ACTION_EXHAUST: {
        if (!target || !target->alive) break;
        const int before = target->stamina;
        int after = before - actor->damage;
        if (after < 0) after = 0;
        target->stamina = after;

        snprintf(msg, LOG_TEXT, "Player %s EXHAUSTS %s stamina %d -> %d",
                 actor->name, target->name, before, after);
        log_event_locked(msg);
        snprintf(msg, LOG_TEXT, "EXHAUST integrity: %s HP remains %d/%d",
                 target->name, target->hp, target->max_hp);
        log_event_locked(msg);

        actor->stamina = 0;
        break;
    }

    case ACTION_USE_WEAPON: {
        if (!target || !target->alive) break;

        int wdmg = 0;
        const char* wname = "Unknown";
        bool found = false;
        
        for (int w = 0; w < MAX_WEAPON_COPIES; w++) {
            OwnedWeapon& ow = actor->weapons[w];
            if (ow.used && !ow.in_storage && ow.unique_id == w_uid) {
                wdmg = WEAPON_TABLE[ow.weapon_id].damage;
                wname = inventory_weapon_name(ow.weapon_id);
                found = true;
                break;
            }
        }
        if (found && w_uid == actor->weapon_swapped_this_turn) {
            log_event_locked("  [!] UseWeapon: cannot use weapon on same turn it was swapped in.");
            break;
        }
        if (!found || wdmg == 0) {
            log_event_locked("  [!] UseWeapon: weapon not found in active inventory.");
            break;
        }

        target->hp -= wdmg;
        if (target->hp <= 0) { target->hp = 0; target->alive = 0; }

        snprintf(msg, LOG_TEXT, "  %s uses %s (DMG:%d) on %s! (HP: %d/%d)",
                 actor->name, wname, wdmg,
                 target->name, target->hp, target->max_hp);
        log_event_locked(msg);

        if (!target->alive) {
            if (t_type == TYPE_ENEMY) {
               register_enemy_defeat_locked(actor, target, t_id, actor_type == TYPE_PLAYER ? actor_id : -1);
            } else {
                actor->total_kills++;
                snprintf(msg, LOG_TEXT, "  !! %s has fallen!", target->name);
                log_event_locked(msg);
            }
        }

        actor->stamina = 0;
        break;
    }

    case ACTION_HEAL: {
        int restore = (int)(actor->max_hp * 0.10f);
        actor->hp  += restore;
        if (actor->hp > actor->max_hp) actor->hp = actor->max_hp;

        snprintf(msg, LOG_TEXT, "  %s HEALS +%d HP. (HP: %d/%d)",
                 actor->name, restore, actor->hp, actor->max_hp);
        log_event_locked(msg);

        actor->stamina = 0;
        break;
    }

    case ACTION_SKIP: {
        actor->stamina = actor->max_stamina / 2;

        snprintf(msg, LOG_TEXT, "  %s SKIPS turn. Stamina -> %d/%d.",
                 actor->name, actor->stamina, actor->max_stamina);
        log_event_locked(msg);
        break;
    }

    case ACTION_SWAP_IN: {
    int found_wid = WEAPON_NONE;
    for (int w = 0; w < MAX_WEAPON_COPIES; w++) {
        OwnedWeapon& ow = actor->weapons[w];
        if (ow.used && ow.in_storage && ow.unique_id == s_uid) {
            found_wid = ow.weapon_id;
            break;
        }
    }
    if (found_wid == WEAPON_NONE) {
        log_event_locked("  [!] Swap: storage weapon not found.");
        break;
    }

    // if player picked one active weapon to kick out, do it first
    if (w_uid >= 0) {
        for (int w = 0; w < MAX_WEAPON_COPIES; w++) {
            OwnedWeapon& ow = actor->weapons[w];
            if (ow.used && !ow.in_storage && ow.unique_id == w_uid) {
                inventory_clear_unique_from_grid(actor, ow.unique_id);
                ow.in_storage = 1;
                ow.slot_start = -1;
                inventory_storage_add(actor, ow.unique_id);
                snprintf(msg, LOG_TEXT, "  >> %s moved to storage.",
                         inventory_weapon_name(ow.weapon_id));
                log_event_locked(msg);
                break;
            }
        }
    }

    InventoryPlacementPlan plan;
    if (!inventory_swap_in_weapon(actor, s_uid, &plan)) {
        log_event_locked("  [!] Swap: unable to place weapon in inventory.");
        break;
    }

    for (int i = 0; i < plan.evict_count; i++) {
        snprintf(msg, LOG_TEXT, "  >> %s moved to storage (space needed).",
                 inventory_weapon_name(plan.evict_weapon_ids[i]));
        log_event_locked(msg);
    }

    snprintf(msg, LOG_TEXT, "  %s swapped %s into inventory.",
             actor->name, inventory_weapon_name(found_wid));
    log_event_locked(msg);

    actor->stamina = 0;
    actor->total_turns++;
    actor->weapon_swapped_this_turn = s_uid;
    pa.ready = 0;
    acknowledge_player_action_locked(actor_type);
    sem_post(&g_sd->state_lock);
    return;
}

    case ACTION_ULTIMATE: {
        if (!actor->has_solar_core || !actor->has_lunar_blade) {
            log_event_locked("  [!] Ultimate: missing Solar Core or Lunar Blade.");
            break;
        }

        snprintf(msg, LOG_TEXT, "  !! %s triggers ULTIMATE ABILITY! Enemies suspended 10s!",
                 actor->name);
        log_event_locked(msg);

        if (g_sd->asp_pid > 0) {
            kill(g_sd->asp_pid, SIGSTOP);
            signal(SIGALRM, handle_sigalrm);
            alarm(ULTIMATE_PAUSE_SECONDS);
        }

        actor->stamina = 0;
        break;
    }
    
    case ACTION_PICKUP_ARTIFACT: {
        bool picked_any = false;
        
                for (int a = 0; a < ARTIFACT_COUNT; a++) {
            ArtifactData& art = g_sd->artifacts[a];
            if (!art.present || art.owner_type != TYPE_NONE) continue;
            
            // someone else locked this one already
            if (art.locked_by_type != TYPE_NONE && 
                (art.locked_by_type != actor_type || art.locked_by_id != actor_id)) {
                actor->waiting_for_artifact = 1;
                art.waiting_by_type = actor_type;
                art.waiting_by_id = actor_id;
                continue;
            }
            
            art.locked_by_type = actor_type;
            art.locked_by_id = actor_id;
            
            int weapon_id;
            if (art.artifact_id == ARTIFACT_SOLAR_CORE) weapon_id = WEAPON_SOLAR_CORE;
            else if (art.artifact_id == ARTIFACT_LUNAR_BLADE) weapon_id = WEAPON_LUNAR_BLADE;
           else if (art.artifact_id == ARTIFACT_ECLIPSE_RELIC) weapon_id = WEAPON_ECLIPSE_RELIC;
            else {
                art.locked_by_type = TYPE_NONE;
                art.locked_by_id = -1;
                continue;
            }
            
            int uid = g_sd->next_weapon_unique_id++;
            
            InventoryPlacementPlan plan;
            if (!inventory_place_new_weapon(actor, weapon_id, uid, &plan)) {
                art.locked_by_type = TYPE_NONE;
                art.locked_by_id = -1;
                snprintf(msg, LOG_TEXT, "  [!] %s: inventory full, cannot pick up %s.",
                         actor->name, artifact_name_from_id(art.artifact_id));
                log_event_locked(msg);
                continue;
            }
            
            art.owner_type = actor_type;
            art.owner_id = actor_id;
            art.locked_by_type = TYPE_NONE;
            art.locked_by_id = -1;
            art.waiting_by_type = TYPE_NONE;
            art.waiting_by_id = -1;
            
            inventory_set_artifact_flag(actor, weapon_id, 1);
            actor->waiting_for_artifact = 0;
            
            snprintf(msg, LOG_TEXT, ">>> ARTIFACT: %s obtained %s!",
                     actor->name, artifact_name_from_id(art.artifact_id));
            log_event_locked(msg);
            picked_any = true;
            break;
        }
        
        if (!picked_any) {
            log_event_locked("  [!] No free artifacts available or inventory full.");
        }
        
        actor->stamina = 0;
        break;
    }
    
    default:
        break;
    }

    actor->total_turns++;
    pa.ready = 0;
    acknowledge_player_action_locked(actor_type);
    sem_post(&g_sd->state_lock);
}

static void check_win_condition() {
    sem_wait(&g_sd->state_lock);

    if (g_sd->quit_requested) {
        g_sd->game_over = 1;
        g_sd->winner    = WINNER_QUIT;
        log_event_locked("Player quit the game.");
        sem_post(&g_sd->state_lock);
        return;
    }

    if (all_players_dead_locked()) {
        g_sd->game_over = 1;
        g_sd->winner    = WINNER_ENEMIES;
        log_event_locked("DEFEAT! All players died.");
    } else if (all_spawned_enemies_dead_locked() ||
               g_sd->enemies_killed_total >= g_sd->total_enemies_to_defeat) {
        g_sd->game_over = 1;
        g_sd->winner    = WINNER_PLAYERS;
        char msg[LOG_TEXT];
        snprintf(msg, LOG_TEXT,
                 "VICTORY! Players defeated all spawned enemies. (%d/%d kills)",
                 g_sd->enemies_killed_total, g_sd->total_enemies_to_defeat);
        log_event_locked(msg);
    }

    sem_post(&g_sd->state_lock);
}



static const char* entity_name_from_type_id(int entity_type, int entity_id) {
    if (entity_type == TYPE_PLAYER &&
        entity_id >= 0 &&
        entity_id < g_sd->player_count) {
        return g_sd->players[entity_id].name;
    }
    if (entity_type == TYPE_ENEMY &&
        entity_id >= 0 &&
        entity_id < g_sd->enemy_count) {
        return g_sd->enemies[entity_id].name;
    }
    return "unknown";
}

static void clear_entity_artifact_wait_flag(int entity_type, int entity_id) {
    if (entity_type == TYPE_PLAYER &&
        entity_id >= 0 &&
        entity_id < g_sd->player_count) {
        g_sd->players[entity_id].waiting_for_artifact = 0;
    } else if (entity_type == TYPE_ENEMY &&
               entity_id >= 0 &&
               entity_id < g_sd->enemy_count) {
        g_sd->enemies[entity_id].waiting_for_artifact = 0;
    }
}

static void seed_deadlock_test_locked() {
    if (!g_deadlock_test_mode || g_sd->player_count < 2) return;

    g_sd->artifacts[0].owner_type = TYPE_PLAYER;
    g_sd->artifacts[0].owner_id = 0;
    g_sd->artifacts[0].waiting_by_type = TYPE_PLAYER;
    g_sd->artifacts[0].waiting_by_id = 1;
    g_sd->players[0].has_solar_core = 1;

    g_sd->artifacts[1].owner_type = TYPE_PLAYER;
    g_sd->artifacts[1].owner_id = 1;
    g_sd->artifacts[1].waiting_by_type = TYPE_PLAYER;
    g_sd->artifacts[1].waiting_by_id = 0;
    g_sd->players[1].has_lunar_blade = 1;

    g_sd->players[0].waiting_for_artifact = 1;
    g_sd->players[1].waiting_for_artifact = 1;

    log_event_locked("Deadlock test mode seeded circular wait between Solar Core and Lunar Blade.");
}

// watches artifact deadlocks
static void* deadlock_monitor(void* /*arg*/) {
    while (g_running && g_sd && !g_sd->game_over) {
        sleep(1);
        if (!g_sd || g_sd->game_over) break;

        sem_wait(&g_sd->state_lock);
        if (g_deadlock_test_mode) {
            log_event_locked("Deadlock monitor tick");
        }

        // look for a wait cycle across artifacts
        for (int a = 0; a < ARTIFACT_COUNT; a++) {
            ArtifactData& artA = g_sd->artifacts[a];
            if (!artA.present || artA.owner_type == TYPE_NONE) continue;
            if (artA.waiting_by_type == TYPE_NONE) continue;

            int owner_type = artA.owner_type;
            int owner_id   = artA.owner_id;
            int waiter_type = artA.waiting_by_type;
            int waiter_id   = artA.waiting_by_id;

            for (int b = 0; b < ARTIFACT_COUNT; b++) {
                if (b == a) continue;
                ArtifactData& artB = g_sd->artifacts[b];
                if (!artB.present) continue;

                bool waiter_holds_B = (artB.owner_type == waiter_type && artB.owner_id == waiter_id);
                bool owner_wants_B  = (artB.waiting_by_type == owner_type && artB.waiting_by_id == owner_id);

                if (waiter_holds_B && owner_wants_B) {
                    log_event_locked("DEADLOCK detected");

                    const char* forced_entity = entity_name_from_type_id(owner_type, owner_id);
                    const char* forced_artifact = artifact_name_from_id(artA.artifact_id);
                    artA.owner_type      = TYPE_NONE;
                    artA.owner_id        = -1;
                    artA.locked_by_type  = TYPE_NONE;
                    artA.locked_by_id    = -1;
                    artA.waiting_by_type = TYPE_NONE;
                    artA.waiting_by_id   = -1;
                    artB.waiting_by_type = TYPE_NONE;
                    artB.waiting_by_id   = -1;

                    if (owner_type == TYPE_PLAYER && owner_id >= 0) {
                        EntityData& e = g_sd->players[owner_id];
                        if (a == 0) e.has_solar_core  = 0;
                        if (a == 1) e.has_lunar_blade  = 0;
                        
                        e.waiting_for_artifact = 0;
                    } else if (owner_type == TYPE_ENEMY && owner_id >= 0) {
                        EntityData& e = g_sd->enemies[owner_id];
                        if (a == 0) e.has_solar_core   = 0;
                        if (a == 1) e.has_lunar_blade   = 0;
                        
                    }
                    if (a == 2) {
                      if (owner_type == TYPE_PLAYER) g_sd->players[owner_id].has_eclipse_relic = 0;
                    else if (owner_type == TYPE_ENEMY) g_sd->enemies[owner_id].has_eclipse_relic = 0;
                       }
                    clear_entity_artifact_wait_flag(owner_type, owner_id);
                    clear_entity_artifact_wait_flag(waiter_type, waiter_id);
                    char msg[LOG_TEXT];
                    snprintf(msg, LOG_TEXT,
                             "DEADLOCK resolved by forcing %s to release %s",
                             forced_entity, forced_artifact);
                    log_event_locked(msg);
                    break;
                }
            }
        }

        sem_post(&g_sd->state_lock);
    }
    return nullptr;
}

int main() {
    printf("[Arbiter] Starting up...\n");
    srand(time(0));
    g_deadlock_test_mode = (getenv("CHRONO_TEST_DEADLOCK") != nullptr &&
                            strcmp(getenv("CHRONO_TEST_DEADLOCK"), "1") == 0);

    signal(SIGTERM,  handle_sigterm);
    signal(SIGINT,   handle_sigterm);
    signal(SIGALRM,  handle_sigalrm);

    shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) { perror("shmget"); return 1; }
    g_sd = (SharedData*)shmat(shmid, nullptr, 0);
    if (g_sd == (void*)-1) { perror("shmat"); return 1; }
    memset(g_sd, 0, sizeof(SharedData));

    sem_init(&g_sd->state_lock,     1, 1);
    sem_init(&g_sd->hip_action_sem, 1, 0);
    sem_init(&g_sd->asp_action_sem, 1, 0);
    for (int i = 0; i < MAX_PLAYERS; i++) sem_init(&g_sd->player_turn_sem[i], 1, 0);
    for (int i = 0; i < MAX_ENEMIES; i++) sem_init(&g_sd->enemy_turn_sem[i],  1, 0);

    sem_wait(&g_sd->state_lock);

    g_sd->arbiter_pid          = getpid();
    g_sd->player_count         = 0;
    g_sd->enemy_count          = rand() % 8 + 2;
    g_sd->total_enemies_to_defeat = g_sd->enemy_count;
    g_sd->game_over            = 0;
    g_sd->winner               = WINNER_NONE;
    g_sd->elapsed_seconds      = 0;
    g_sd->enemies_killed_total = 0;
    g_sd->next_weapon_unique_id= 1;
    g_sd->drop_active          = 0;
    g_sd->eclipse_present      = 0;
    g_sd->current_turn_type    = TURN_NONE;
    g_sd->current_turn_index   = -1;
    g_sd->hip_action_seq       = 0;
    g_sd->arbiter_consumed_seq = 0;

    g_sd->artifacts[0].present        = 1;
    g_sd->artifacts[0].artifact_id    = ARTIFACT_SOLAR_CORE;
    g_sd->artifacts[0].owner_type     = TYPE_NONE;
    g_sd->artifacts[0].owner_id       = -1;
    g_sd->artifacts[0].locked_by_type = TYPE_NONE;
    g_sd->artifacts[0].locked_by_id   = -1;

    g_sd->artifacts[1].present        = 1;
    g_sd->artifacts[1].artifact_id    = ARTIFACT_LUNAR_BLADE;
    g_sd->artifacts[1].owner_type     = TYPE_NONE;
    g_sd->artifacts[1].owner_id       = -1;
    g_sd->artifacts[1].locked_by_type = TYPE_NONE;
    g_sd->artifacts[1].locked_by_id   = -1;

    g_sd->artifacts[2].present        = 0;
    g_sd->artifacts[2].artifact_id    = ARTIFACT_ECLIPSE_RELIC;

    g_sd->initialized = 1;
    printf("[Arbiter] Spawned enemy_count=%d\n", g_sd->enemy_count);
    log_event_locked("Arbiter ready. Waiting for player count from HIP...");
    sem_post(&g_sd->state_lock);

    // wait till hip sets player count
    while (g_running && !g_sd->game_started && !g_sd->quit_requested)
        usleep(100000);
    if (!g_running || g_sd->quit_requested) goto cleanup;

    sem_wait(&g_sd->state_lock);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        EntityData& p = g_sd->players[i];
        p.used      = (i < g_sd->player_count) ? 1 : 0;
        p.type      = TYPE_PLAYER;
        p.id        = i;
        snprintf(p.name, NAME_SIZE, "%s", heroNames[i]);

        p.max_hp     = ROLL_NUMBER + (rand() % 901 + 100);
        p.hp         = p.max_hp;
        p.damage     = ROLL_LAST_DIGIT + 10;
        p.speed      = 100 / g_sd->player_count;
        p.max_stamina = 100;
        p.stamina    = 0;
        p.alive      = p.used;
        p.stunned    = 0;
        p.total_turns = 0;
        p.total_kills = 0;
        p.storage_count = 0;
        p.weapon_swapped_this_turn = -1;
        memset(p.inventory, 0, sizeof(p.inventory));
        memset(p.weapons,   0, sizeof(p.weapons));
    }
   
    for (int i = 0; i < MAX_ENEMIES; i++) {
        EntityData& e = g_sd->enemies[i];
        e.used      = (i < g_sd->enemy_count) ? 1 : 0;
        e.type      = TYPE_ENEMY;
        e.id        = i;
        snprintf(e.name, NAME_SIZE, "%s", enemyName[i]);

        e.max_hp     = ROLL_LAST_TWO + (rand() % 151 + 50);
        e.hp         = e.max_hp;
        e.damage     = ROLL_SECOND_LAST_DIGIT + 10;
        e.speed      = rand() % 21 + 10;
        e.max_stamina = 150;
        e.stamina    = 0;
        e.alive      = e.used;
        e.stunned    = 0;
        e.total_turns = 0;
        e.total_kills = 0;
    }

    seed_deadlock_test_locked();
    sem_post(&g_sd->state_lock);
    log_event_s("Battle has begun!");

    {
        pthread_t dl_thread;
        pthread_create(&dl_thread, nullptr, deadlock_monitor, nullptr);
        pthread_detach(dl_thread);
    }

    while (g_running && !g_sd->game_over) {

        sem_wait(&g_sd->state_lock);

        int active_type  = TYPE_NONE;
        int active_index = -1;

        for (int i = 0; i < g_sd->player_count; i++) {
            EntityData& p = g_sd->players[i];
            if (!p.alive) continue;
            if (p.stunned) {
                if (time(nullptr) >= p.stun_end_time) {
                    p.stunned = 0;
                    log_event_locked("Player stun expired.");
                }
                continue;
            }
            p.stamina += p.speed-5;
            if (p.stamina >= p.max_stamina && active_type == TYPE_NONE) {
                p.stamina    = p.max_stamina;
                active_type  = TYPE_PLAYER;
                active_index = i;
            }
        }

        for (int i = 0; i < g_sd->enemy_count; i++) {
            EntityData& e = g_sd->enemies[i];
            if (!e.alive) continue;
            if (e.stunned) {
                if (time(nullptr) >= e.stun_end_time) {
                    e.stunned = 0;
                }
                continue;
            }
            e.stamina += e.speed;
            if (e.stamina >= e.max_stamina && active_type == TYPE_NONE) {
                e.stamina    = e.max_stamina;
                active_type  = TYPE_ENEMY;
                active_index = i;
            }
        }
        
        if (active_type != TYPE_NONE) {
            g_sd->current_turn_type  = active_type;
            g_sd->current_turn_index = active_index;
        }

        sem_post(&g_sd->state_lock);

        if (active_type == TYPE_NONE) {
            usleep(250000);
            sem_wait(&g_sd->state_lock);
            g_sd->elapsed_seconds++;
            sem_post(&g_sd->state_lock);
            continue;
        }

              if (active_type == TYPE_PLAYER) {
            int pi = active_index;

            sem_post(&g_sd->player_turn_sem[active_index]);
            sem_wait(&g_sd->hip_action_sem);

        } else {
            sem_post(&g_sd->enemy_turn_sem[active_index]);

            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += NPC_TURN_TIMEOUT;

            if (sem_timedwait(&g_sd->asp_action_sem, &ts) != 0) {
                sem_wait(&g_sd->state_lock);
                g_sd->pending_action.ready      = 1;
                g_sd->pending_action.actor_type  = TYPE_ENEMY;
                g_sd->pending_action.actor_id    = active_index;
                g_sd->pending_action.action      = ACTION_SKIP;
                g_sd->pending_action.target_type = TYPE_NONE;
                g_sd->pending_action.target_id   = -1;
                sem_post(&g_sd->state_lock);
                log_event_s("NPC turn timed out; treating action as SKIP");
            }
        }

        sem_wait(&g_sd->state_lock);
        g_sd->current_turn_type  = TURN_NONE;
        g_sd->current_turn_index = -1;
        sem_post(&g_sd->state_lock);

        process_pending_action();

        // wait for drop answer, else auto decline
        if (g_sd->drop_active && !g_sd->drop_choice_ready) {
            time_t expire = g_sd->drop_expire_time;
            while (!g_sd->drop_choice_ready && time(nullptr) < expire && !g_sd->game_over) {
                usleep(50000);
            }
            if (!g_sd->drop_choice_ready) {
                sem_wait(&g_sd->state_lock);
                g_sd->drop_choice_ready = 1;
                g_sd->drop_choice_pick  = 0;
                sem_post(&g_sd->state_lock);
            }
            sem_wait(&g_sd->state_lock);
            if (g_sd->drop_choice_pick == 1) {
                int uid = g_sd->next_weapon_unique_id++;
                place_weapon(&g_sd->players[g_sd->drop_for_player_id],
                             g_sd->drop_weapon_id, uid);
                char msg[LOG_TEXT];
                snprintf(msg, LOG_TEXT, "%s picked up %s!",
                         g_sd->players[g_sd->drop_for_player_id].name,
                         inventory_weapon_name(g_sd->drop_weapon_id));
                log_event_locked(msg);
            } else {
                int e_idx = rand() % g_sd->enemy_count;
                while (!g_sd->enemies[e_idx].alive) e_idx = rand() % g_sd->enemy_count;
                char msg[LOG_TEXT];
                snprintf(msg, LOG_TEXT, "Enemy %s scavenged %s (declined by %s)!",
                         g_sd->enemies[e_idx].name,
                         inventory_weapon_name(g_sd->drop_weapon_id),
                         g_sd->players[g_sd->drop_for_player_id].name);
                log_event_locked(msg);
            }
            g_sd->drop_active       = 0;
            g_sd->drop_choice_ready = 0;
            sem_post(&g_sd->state_lock);
        }

        sem_wait(&g_sd->state_lock);
        if (g_sd->current_turn_type == TURN_PLAYER && g_sd->current_turn_index >= 0) {
            g_sd->players[g_sd->current_turn_index].weapon_swapped_this_turn = -1;
        }
        sem_post(&g_sd->state_lock);
        check_win_condition();
    }
cleanup:
    printf("[Arbiter] Game Over. Cleaning up.\n");

    // unblock anyone still waiting
    for (int i = 0; i < MAX_PLAYERS; i++) sem_post(&g_sd->player_turn_sem[i]);
    for (int i = 0; i < MAX_ENEMIES; i++) sem_post(&g_sd->enemy_turn_sem[i]);

    sem_destroy(&g_sd->state_lock);
    sem_destroy(&g_sd->hip_action_sem);
    sem_destroy(&g_sd->asp_action_sem);
    for (int i = 0; i < MAX_PLAYERS; i++) sem_destroy(&g_sd->player_turn_sem[i]);
    for (int i = 0; i < MAX_ENEMIES; i++) sem_destroy(&g_sd->enemy_turn_sem[i]);

    shmdt(g_sd);
    shmctl(shmid, IPC_RMID, nullptr);
    return 0;
}
