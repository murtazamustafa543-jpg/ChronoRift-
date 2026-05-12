#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "../common.h"

static SharedData* shared_data = nullptr;
static int shmid = -1;
static volatile sig_atomic_t stop_flag = 0;

static pthread_t enemy_threads[MAX_ENEMIES];
static int thread_indices[MAX_ENEMIES];
static sem_t stun_sem[MAX_ENEMIES];

static void stop_handler(int /*sig*/) {
    stop_flag = 1;
}

static void stun_handler(int /*sig*/) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        sem_post(&stun_sem[i]);
    }
}

static int lowest_hp_player_locked() {
    int best_id = -1;
    int best_hp = 2147483647;
    for (int i = 0; i < shared_data->player_count; i++) {
        if (shared_data->players[i].alive && shared_data->players[i].hp < best_hp) {
            best_hp = shared_data->players[i].hp;
            best_id = shared_data->players[i].id;
        }
    }
    return best_id;
}

static int first_alive_player_locked() {
    for (int i = 0; i < shared_data->player_count; i++) {
        if (shared_data->players[i].alive) return shared_data->players[i].id;
    }
    return -1;
}

static void submit_enemy_action(ActionData* action) {
    sem_wait(&shared_data->state_lock);
    if (!shared_data->game_over) {
        shared_data->pending_action = *action;
        shared_data->pending_action.ready = 1;
    }
    sem_post(&shared_data->state_lock);
    sem_post(&shared_data->asp_action_sem);
}

static void* enemy_thread(void* arg) {
    int index = *((int*)arg);

    while (!stop_flag) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        int rc = sem_timedwait(&shared_data->enemy_turn_sem[index], &ts);
        if (rc != 0) {
            if (errno == ETIMEDOUT || errno == EINTR) continue;
            break;
        }

        if (stop_flag || shared_data->game_over) break;

        while (sem_trywait(&stun_sem[index]) == 0) {
        }

        sem_wait(&shared_data->state_lock);
        int is_stunned = shared_data->enemies[index].stunned;
        time_t end = shared_data->enemies[index].stun_end_time;
        sem_post(&shared_data->state_lock);

        if (is_stunned) {
            time_t now = time(nullptr);
            if (end > now) {
                struct timespec stun_ts;
                clock_gettime(CLOCK_REALTIME, &stun_ts);
                stun_ts.tv_sec += (end - now);
                sem_timedwait(&stun_sem[index], &stun_ts);
            }
            printf("[ASP] Enemy %d stun expired. Skipping turn.\n", index);
            continue;
        }

        sem_wait(&shared_data->state_lock);

        if (shared_data->game_over) {
            sem_post(&shared_data->state_lock);
            break;
        }

        if (shared_data->current_turn_type != TURN_ENEMY ||
            shared_data->current_turn_index != index ||
            !shared_data->enemies[index].alive) {
            sem_post(&shared_data->state_lock);
            continue;
        }

        int target_id = lowest_hp_player_locked();
        if (target_id < 0) target_id = first_alive_player_locked();
        if (target_id < 0) {
            sem_post(&shared_data->state_lock);
            continue;
        }
        bool any_free_artifact = false;
        for (int a = 0; a < ARTIFACT_COUNT; a++) {
            if (shared_data->artifacts[a].present && 
                shared_data->artifacts[a].owner_type == TYPE_NONE &&
                shared_data->artifacts[a].locked_by_type == TYPE_NONE) {
                any_free_artifact = true;
                break;
            }
        }
        if (any_free_artifact && (rand() % 100) < 30) {
            ActionData art_action;
            memset(&art_action, 0, sizeof(ActionData));
            art_action.actor_type = TYPE_ENEMY;
            art_action.actor_id = shared_data->enemies[index].id;
            art_action.action = ACTION_PICKUP_ARTIFACT;
            art_action.target_type = TYPE_NONE;
            art_action.target_id = -1;
            printf("[ASP] Enemy %d (%s) attempts ARTIFACT PICKUP.\n",
                   index, shared_data->enemies[index].name);
            sem_post(&shared_data->state_lock);
            usleep(120000);
            submit_enemy_action(&art_action);
            continue;
        }
        ActionData action;
        memset(&action, 0, sizeof(ActionData));
        action.actor_type = TYPE_ENEMY;
        action.actor_id = shared_data->enemies[index].id;
        action.action = ACTION_STRIKE;
        action.target_type = TYPE_PLAYER;
        action.target_id = target_id;

        printf("[ASP] Enemy %d (%s) decides to STRIKE player %d.\n",
               index, shared_data->enemies[index].name, target_id);

        sem_post(&shared_data->state_lock);

        usleep(120000);
        submit_enemy_action(&action);
    }

    printf("[ASP] Enemy thread %d exiting.\n", index);
    return nullptr;
}

static void attach_shared_memory() {
    while (1) {
        shmid = shmget((key_t)SHM_KEY, sizeof(SharedData), 0666);
        if (shmid >= 0) break;
        if (stop_flag) exit(0);
        usleep(100000);
    }
    shared_data = (SharedData*)shmat(shmid, nullptr, 0);
    if (shared_data == (void*)-1) {
        perror("[ASP] shmat");
        exit(1);
    }
}

int main() {
    srand(ROLL_NUMBER);

    signal(SIGTERM, stop_handler);
    signal(SIGINT, stop_handler);
    signal(SIGUSR1, stun_handler);

    for (int i = 0; i < MAX_ENEMIES; i++) {
        sem_init(&stun_sem[i], 0, 0);
    }

    attach_shared_memory();

    while (!shared_data->initialized && !stop_flag) usleep(10000);
    while (!shared_data->game_started && !stop_flag) usleep(50000);

    if (stop_flag) goto cleanup;

    sem_wait(&shared_data->state_lock);
    shared_data->asp_pid = getpid();
    sem_post(&shared_data->state_lock);

    printf("[ASP] Attached. Enemy count: %d\n", shared_data->enemy_count);

    for (int i = 0; i < shared_data->enemy_count; i++) {
        thread_indices[i] = i;
        pthread_create(&enemy_threads[i], nullptr, enemy_thread, &thread_indices[i]);
        printf("[ASP] Spawned thread for enemy %d (%s).\n",
               i, shared_data->enemies[i].name);
    }

    while (!stop_flag) {
        sem_wait(&shared_data->state_lock);
        int done = shared_data->game_over;
        sem_post(&shared_data->state_lock);
        if (done) break;
        sleep(1);
    }

    printf("[ASP] Game over. Shutting down enemy threads.\n");

cleanup:
    stop_flag = 1;

    if (shared_data) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            sem_post(&shared_data->enemy_turn_sem[i]);
            sem_post(&stun_sem[i]);
        }

        for (int i = 0; i < shared_data->enemy_count; i++) {
            pthread_join(enemy_threads[i], nullptr);
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        sem_destroy(&stun_sem[i]);
    }

    if (shared_data) shmdt(shared_data);
    return 0;
}
