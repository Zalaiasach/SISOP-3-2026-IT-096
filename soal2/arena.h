#ifndef ARENA_H
#define ARENA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <stdbool.h>

#define SHM_KEY 0x00001234
#define MAX_USERS 100
#define MAX_HISTORY 50

typedef struct {
    char time_str[20];
    char opponent[50];
    char result[10]; 
    int xp_gained;
} History;

typedef struct {
    char username[50];
    char password[50];
    int gold;
    int lvl;
    int xp;
    int weapon_bonus_dmg; 
    bool is_logged_in;
    bool is_matchmaking;
    int opponent_idx;
    int current_hp;
    History match_history[MAX_HISTORY];
    int history_count;
} User;

typedef struct {
    sem_t mutex;
    bool orion_ready;
    User users[MAX_USERS];
    int user_count;
} SharedData;

#endif