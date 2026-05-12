
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <csignal>


#include "../common.h"
#include "ui_renderer_full.h"
using namespace std;
using namespace sf;

// shared stuff used by threads
static SharedData*    g_sd  = nullptr;
static UIRenderer*    g_ui  = nullptr;
static volatile sig_atomic_t g_running = 1;
static bool g_headless = false;
static bool g_headless_exhaust_only = false;

// arg passed into each player thread
struct PlayerThreadArg {
    int player_index;
};

// push one action into shared memory
static void submitAction(int actor_type, int actor_id,
                         int action, int target_type, int target_id,
                         int weapon_unique_id, int storage_unique_id) {
    if (!g_sd) return;

    sem_wait(&g_sd->state_lock);

    g_sd->pending_action.ready           = 1;
    g_sd->pending_action.actor_type      = actor_type;
    g_sd->pending_action.actor_id        = actor_id;
    g_sd->pending_action.action          = action;
    g_sd->pending_action.target_type     = target_type;
    g_sd->pending_action.target_id       = target_id;
    g_sd->pending_action.weapon_unique_id  = weapon_unique_id;
    g_sd->pending_action.storage_unique_id = storage_unique_id;
    g_sd->hip_action_seq++;

    sem_post(&g_sd->state_lock);

   
    sem_post(&g_sd->hip_action_sem);
}

static void hip_sigusr1_handler(int signo)
{
    (void)signo;
    const char msg[] = "[HIP] Received SIGUSR1 stun signal\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
}

struct PlayerMailbox {
    int    action;
    int    target_type;
    int    target_id;
    int    weapon_uid;
    int    storage_uid;
    sem_t  ready_sem;
};
static PlayerMailbox g_mailbox[MAX_PLAYERS];

static int select_headless_enemy_target_locked(bool prefer_stamina_targets) {
    int fallback = -1;
    for (int e = 0; e < g_sd->enemy_count; e++) {
        if (!g_sd->enemies[e].alive) continue;
        if (fallback < 0) fallback = e;
        if (prefer_stamina_targets && g_sd->enemies[e].stamina > 0) return e;
    }
    return fallback;
}

static void* playerThread(void* arg) {
    PlayerThreadArg* a = (PlayerThreadArg*)arg;
    int idx = a->player_index;

    bool waiting_printed = false;
            if (g_sd->drop_active && g_sd->drop_for_player_id == idx) {
            if (g_headless) {
                sem_wait(&g_sd->state_lock);
                g_sd->drop_choice_ready = 1;
                g_sd->drop_choice_pick = 1;
                sem_post(&g_sd->state_lock);
            }
        }
    while (g_running) {
              
        if (g_headless && g_sd->drop_active && g_sd->drop_for_player_id == idx) {
            sem_wait(&g_sd->state_lock);
            g_sd->drop_choice_ready = 1;
            g_sd->drop_choice_pick = 1;  
            sem_post(&g_sd->state_lock);
        }
        if (g_headless && !waiting_printed) {
            printf("[HIP-HEADLESS] Player %d waiting for turn\n", idx);
            waiting_printed = true;
        }

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        int rc = sem_timedwait(&g_sd->player_turn_sem[idx], &ts);
        
        if (rc != 0) {
            if (g_headless) usleep(50000); 
            continue;
        }

        waiting_printed = false;

        if (!g_running) break;
        if (g_sd->game_over)   break;

        if (g_headless) {
            int seq_before;
            int action = ACTION_STRIKE;
            int target_id = -1;

            sem_wait(&g_sd->state_lock);
            target_id = select_headless_enemy_target_locked(g_headless_exhaust_only);
            sem_post(&g_sd->state_lock);

            if (target_id >= 0) {
                if (g_headless_exhaust_only) {
                    action = ACTION_EXHAUST;
                    printf("[HIP-HEADLESS] submitted action player=%d action=EXHAUST target=%d\n", idx, target_id);
                } else {
                    printf("[HIP-HEADLESS] submitted action player=%d action=STRIKE target=%d\n", idx, target_id);
                }
                submitAction(TYPE_PLAYER, idx, action, TYPE_ENEMY, target_id, -1, -1);
            } else {
                printf("[HIP-HEADLESS] submitted action player=%d action=SKIP (no enemies)\n", idx);
                submitAction(TYPE_PLAYER, idx, ACTION_SKIP, TYPE_NONE, -1, -1, -1);
            }

            sem_wait(&g_sd->state_lock);
            seq_before = g_sd->hip_action_seq;
            sem_post(&g_sd->state_lock);

            while (g_running && !g_sd->game_over) {
                sem_wait(&g_sd->state_lock);
                int consumed = g_sd->arbiter_consumed_seq;
                sem_post(&g_sd->state_lock);
                if (consumed >= seq_before) {
                    printf("[HIP-HEADLESS] observed consumed seq=%d\n", consumed);
                    break;
                }
                usleep(10000);
            }
            continue;
        }

        // wait for ui to fill this player's mailbox
        struct timespec ts2;
        clock_gettime(CLOCK_REALTIME, &ts2);
        ts2.tv_sec += 60;  
        sem_timedwait(&g_mailbox[idx].ready_sem, &ts2);

        if (g_sd->game_over || !g_running) break;

        int act       = g_mailbox[idx].action;
        int tgt_type  = g_mailbox[idx].target_type;
        int tgt_id    = g_mailbox[idx].target_id;
        int w_uid     = g_mailbox[idx].weapon_uid;
        int s_uid     = g_mailbox[idx].storage_uid;

        
        g_mailbox[idx].action      = ACTION_NONE;
        g_mailbox[idx].storage_uid = -1;

        submitAction(TYPE_PLAYER, idx, act, tgt_type, tgt_id, w_uid, s_uid);
    }
    return nullptr;
}

// tell arbiter we are quitting too using sigterm, and also set our running flag to false to stop threads
static void onSigterm(int) {
    if (g_sd) {
        sem_wait(&g_sd->state_lock);
        g_sd->quit_requested = 1;
        sem_post(&g_sd->state_lock);
        if (g_sd->arbiter_pid > 0) kill(g_sd->arbiter_pid, SIGTERM);
    }
    g_running = 0;
}

static void init_mailboxes() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        g_mailbox[i].action      = ACTION_NONE;
        g_mailbox[i].target_type = TYPE_NONE;
        g_mailbox[i].target_id   = -1;
       g_mailbox[i].weapon_uid  = -1;
g_mailbox[i].storage_uid = -1;
    }
}

int main() {
    signal(SIGTERM, onSigterm);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = hip_sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("[HIP] sigaction SIGUSR1 failed");
    }

    // attach shared memory
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shmid < 0) {
        perror("[HIP] shmget failed – is Arbiter running?");
      
    }
    g_sd = (SharedData*)shmat(shmid, nullptr, 0);
    if (g_sd == (void*)-1) { perror("[HIP] shmat"); return 1; }

    // wait till arbiter init is done
    while (!g_sd->initialized) usleep(10000);

    // init mailboxes
    init_mailboxes();
    for (int i = 0; i < MAX_PLAYERS; i++)
        sem_init(&g_mailbox[i].ready_sem, 0, 0);

    const char* env_headless = getenv("CHRONO_HEADLESS");
    if (env_headless && strcmp(env_headless, "1") == 0) {
        g_headless = true;
    }
    const char* env_headless_policy = getenv("CHRONO_HEADLESS_POLICY");
    if (env_headless_policy && strcmp(env_headless_policy, "exhaust") == 0) {
        g_headless_exhaust_only = true;
    }

    if (g_headless) {
    printf("[HIP] Running in HEADLESS mode.\n");

    const int headless_players = 4;

    sem_wait(&g_sd->state_lock);
    g_sd->player_count = headless_players;
    g_sd->game_started = 1;
    g_sd->hip_pid = getpid();
    sem_post(&g_sd->state_lock);

    static PlayerThreadArg persistent_args[MAX_PLAYERS];
    static pthread_t threads[MAX_PLAYERS];

    for (int i = 0; i < headless_players; i++) {
        persistent_args[i].player_index = i;
        pthread_create(&threads[i], nullptr, playerThread, &persistent_args[i]);
        pthread_detach(threads[i]);
    }

    printf("[HIP] Spawned %d player threads (HEADLESS)\n", headless_players);

        while (g_running && !g_sd->game_over) {
            sleep(1);
        }
        
        g_running = 0;
        for (int i = 0; i < MAX_PLAYERS; i++)
            sem_destroy(&g_mailbox[i].ready_sem);
        shmdt(g_sd);
        return 0;
    }

    // Initializeung UI
    UIRenderer ui;
    g_ui = &ui;
    if (!ui.init()) {
        fprintf(stderr, "[HIP] Failed to init UI\n");
        return 1;
    }
    ui.currentScreen = Screen::SPLASH;



    // ui picked one player action
    ui.onActionSelected = [&](int action, int targetEnemyIdx, int weaponUid) {
    if (!g_sd) return;
    int pi = g_sd->current_turn_index;
    if (pi < 0 || pi >= MAX_PLAYERS) return;
    
    if (action == ACTION_STRIKE || action == ACTION_EXHAUST) {
        if (targetEnemyIdx < 0) return;
    }
    
    if (action == ACTION_USE_WEAPON) {
        if (weaponUid < 0) return;
        bool active = false;
        for (int w = 0; w < MAX_WEAPON_COPIES; w++) {
            if (g_sd->players[pi].weapons[w].used && 
                !g_sd->players[pi].weapons[w].in_storage &&
                g_sd->players[pi].weapons[w].unique_id == weaponUid) {
                active = true;
                break;
            }
        }
        if (!active) return;
    }
    
    if (action == ACTION_ULTIMATE && 
        !(g_sd->players[pi].has_solar_core && g_sd->players[pi].has_lunar_blade)) return;
    
    g_mailbox[pi].action = action;
    g_mailbox[pi].target_type = (targetEnemyIdx >= 0 ? TYPE_ENEMY : TYPE_NONE);
    g_mailbox[pi].target_id = targetEnemyIdx;
    g_mailbox[pi].weapon_uid = weaponUid;
    sem_post(&g_mailbox[pi].ready_sem);
};

   ui.onStorageWeaponSelected = [&](int storageUid) {
        if (!g_sd) return;
        int pi = g_sd->current_turn_index;
        if (pi < 0 || pi >= MAX_PLAYERS) return;
        g_mailbox[pi].action      = ACTION_SWAP_IN;
        g_mailbox[pi].storage_uid = storageUid;
        g_mailbox[pi].weapon_uid  = ui.m_swapActiveUid;
        sem_post(&g_mailbox[pi].ready_sem);
    };

    ui.onMenuAction = [&](const std::string& act) {
        if (act == "start") {
            ui.currentScreen = Screen::PLAYER_SELECT;
        } else if (act == "quit") {
            onSigterm(SIGTERM);
            ui.close();
        }
    };

   ui.onPlayerCountConfirmed = [&](int count) {
    if (!g_sd) return;

    sem_wait(&g_sd->state_lock);
    g_sd->player_count = count;
    g_sd->game_started = 1;
    sem_post(&g_sd->state_lock);

    ui.currentScreen = Screen::GAME;

    // spawn player threads
    static PlayerThreadArg persistent_args[MAX_PLAYERS];  
    static pthread_t threads[MAX_PLAYERS];

    for (int i = 0; i < count; i++) {
        persistent_args[i].player_index = i;
        pthread_create(&threads[i], nullptr, playerThread, &persistent_args[i]);
        pthread_detach(threads[i]);
    }

    printf("[HIP] Spawned %d player threads\n", count);
};

    ui.onQuitRequested = [&]() { onSigterm(SIGTERM); };

    
    sem_wait(&g_sd->state_lock);
    g_sd->hip_pid = getpid();
    sem_post(&g_sd->state_lock);

   
    Clock frameClock;
    float splashTime = 0.f;

    while (ui.isOpen() && g_running) {
        splashTime += frameClock.restart().asSeconds();

       
        if (g_sd->game_over && ui.currentScreen == Screen::GAME)
            ui.currentScreen = Screen::GAME_OVER;

        switch (ui.currentScreen) {
            case Screen::SPLASH:
                ui.renderSplash(splashTime);
                if(splashTime > 3) ui.currentScreen = Screen::MAIN_MENU;
                break;
            case Screen::MAIN_MENU:
                if (!ui.processEvents()) goto done;
                ui.renderMainMenu();
                break;
            case Screen::PLAYER_SELECT:
                if (!ui.processEvents()) goto done;
                ui.renderPlayerSelect(1);
                break;
            case Screen::GAME:
                if (!ui.processEvents()) goto done;
                ui.renderGame(*g_sd);
                break;
            case Screen::GAME_OVER:
                if (!ui.processEvents()) goto done;
                ui.renderGameOver(*g_sd);
                break;
        }
    }

done:
    g_running = 0;

    // cleanup
    for (int i = 0; i < MAX_PLAYERS; i++){
        cout<<"cleaning"<<endl;
        sem_destroy(&g_mailbox[i].ready_sem);
    }
    shmdt(g_sd);
    return 0;
}
