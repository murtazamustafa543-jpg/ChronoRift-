#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <time.h>

#define SHM_KEY 1234
#define STUN_SECONDS 3
#define ULTIMATE_PAUSE_SECONDS 10   
#define NPC_TURN_TIMEOUT 3

#define ROLL_NUMBER 835
#define ROLL_LAST_TWO 35
#define ROLL_LAST_DIGIT 5
#define ROLL_SECOND_LAST_DIGIT 3

#define MAX_PLAYERS 4
#define MAX_ENEMIES 9
#define INVENTORY_SLOTS 20
#define STORAGE_SLOTS 20
#define MAX_WEAPON_COPIES 24
#define MAX_LOG_LINES 20
#define LOG_TEXT 160
#define NAME_SIZE 32
#define ARTIFACT_COUNT 3

#define TYPE_NONE 0
#define TYPE_PLAYER 1
#define TYPE_ENEMY 2

#define TURN_NONE 0
#define TURN_PLAYER 1
#define TURN_ENEMY 2

#define ACTION_NONE 0
#define ACTION_STRIKE 1
#define ACTION_EXHAUST 2
#define ACTION_USE_WEAPON 3
#define ACTION_SWAP_IN 4
#define ACTION_HEAL 5
#define ACTION_SKIP 6
#define ACTION_ULTIMATE 7
#define ACTION_PICKUP_ARTIFACT 8  

#define WEAPON_NONE 0
#define WEAPON_SOLAR_CORE 1
#define WEAPON_LUNAR_BLADE 2
#define WEAPON_IRON_HALBERD 3
#define WEAPON_VENOM_DAGGER 4
#define WEAPON_THUNDERSTAFF 5
#define WEAPON_OBSIDIAN_AXE 6
#define WEAPON_FROSTBOW 7
#define WEAPON_SPLINTER_STICK 8
#define WEAPON_ECLIPSE_RELIC 9
#define ARTIFACT_SOLAR_CORE 1
#define ARTIFACT_LUNAR_BLADE 2
#define ARTIFACT_ECLIPSE_RELIC 3


#define WINNER_NONE 0
#define WINNER_PLAYERS 1
#define WINNER_ENEMIES 2
#define WINNER_QUIT 3

struct WeaponData
{
    int id;
    char name[NAME_SIZE];
    int slots;
    int damage;
    int is_artifact;
};

struct OwnedWeapon
{
    int used;
    int unique_id;
    int weapon_id;
    int slot_start;
    int slot_count;
    int in_storage;
};

struct EntityData
{
    int used;
    int type;
    int id;
    char name[NAME_SIZE];

    int hp;
    int max_hp;
    int damage;
    int speed;
    int stamina;
    int max_stamina;
int weapon_swapped_this_turn;  
    int alive;
    int stunned;
    time_t stun_end_time;

    int total_turns;
    int total_kills;

    int waiting_for_artifact;
    int has_solar_core;
    int has_lunar_blade;
    int has_eclipse_relic;

    int inventory[INVENTORY_SLOTS];
    struct OwnedWeapon weapons[MAX_WEAPON_COPIES];
    int storage_ids[STORAGE_SLOTS];
    int storage_count;
};

struct ArtifactData
{
    int present;
    int artifact_id;
    int owner_type;
    int owner_id;
    int locked_by_type;
    int locked_by_id;
    int waiting_by_type;
    int waiting_by_id;
};

struct ActionData
{
    int ready;
    int actor_type;
    int actor_id;

    int action;
    int target_type;
    int target_id;

    int weapon_unique_id;
    int storage_unique_id;
};

struct SharedData
{
    int hip_input_active;
    sem_t state_lock;
    sem_t player_turn_sem[MAX_PLAYERS];
    sem_t enemy_turn_sem[MAX_ENEMIES];
    sem_t hip_action_sem;
    sem_t asp_action_sem;
time_t drop_expire_time; 
    int initialized;
    int game_started;
    int game_over;
    int winner;
    int quit_requested;

    pid_t arbiter_pid;
    pid_t hip_pid;
    pid_t asp_pid;

    int player_count;
    int enemy_count;
    int total_enemies_to_defeat;
    int enemies_killed_total;
    int next_weapon_unique_id;
    int eclipse_present;
    int elapsed_seconds;

    int current_turn_type;
    int current_turn_index;

    int drop_active;
    int drop_weapon_id;
    int drop_for_player_id;
    int drop_choice_ready;
    int drop_choice_pick;

    struct EntityData players[MAX_PLAYERS];
    struct EntityData enemies[MAX_ENEMIES];
    struct ArtifactData artifacts[ARTIFACT_COUNT];
    struct ActionData pending_action;
    int hip_action_seq;
    int arbiter_consumed_seq;

#ifdef DEBUG_DIAG
    int arbiter_tick_count;
    int render_tick_count;
    int hip_thread_heartbeat[MAX_PLAYERS];
    int asp_thread_heartbeat[MAX_ENEMIES];
    int last_action_timestamp;
    int scheduler_tick_count;
    int deadlock_monitor_tick_count;
    int signal_event_count;
#endif

    char log_lines[MAX_LOG_LINES][LOG_TEXT];
    int log_count;
};
static const char* heroNames[] = {"Alya", "Chrono", "Frog", "Magnus"};
static const char* enemyName[] = {
    "Blob", "Cybot", "Free Lancher",
    "Ghost", "Imp", "Jinn",
    "Mage", "Mother Rain", "Son of Sun"
};
