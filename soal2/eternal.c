#include "arena.h"
#include <termios.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>

void clear_screen() {
    printf("\033[H\033[J");
}

void set_nonblocking_mode() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void reset_terminal_mode() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag |= ICANON | ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

void update_stats(SharedData *shm, int idx) {
    shm->users[idx].lvl = (shm->users[idx].xp / 100) + 1;
}

void render_battle(char *my_name, int my_hp, int my_max, char *en_name, int en_hp, int en_max) {
    clear_screen();
    printf("--- ARENA ---\n\n");
    printf("%s\n[%d / %d]\n\n", my_name, my_hp > 0 ? my_hp : 0, my_max);
    printf("     VS     \n\n");
    printf("%s\n[%d / %d]\n\n", en_name, en_hp > 0 ? en_hp : 0, en_max);
    printf("Combat Log:\n> Tekan 'a' atau 'u'.\n");
}

void start_battle(SharedData *shm, int my_idx) {
    clear_screen();
    printf("Searching for an opponent...\n");

    update_stats(shm, my_idx);
    int my_max_hp = 100 + (shm->users[my_idx].xp / 10);
    int my_dmg = 10 + (shm->users[my_idx].xp / 50) + shm->users[my_idx].weapon_bonus_dmg;
    
    sem_wait(&shm->mutex);
    shm->users[my_idx].current_hp = my_max_hp;
    shm->users[my_idx].opponent_idx = -1;
    shm->users[my_idx].is_matchmaking = true;
    sem_post(&shm->mutex);

    int opponent_idx = -1;
    for (int i = 0; i < 35; i++) {
        sem_wait(&shm->mutex);
        if (shm->users[my_idx].opponent_idx != -1) {
            opponent_idx = shm->users[my_idx].opponent_idx;
            sem_post(&shm->mutex);
            break;
        }

        for (int j = 0; j < shm->user_count; j++) {
            if (j != my_idx && shm->users[j].is_matchmaking) {
                shm->users[j].is_matchmaking = false;
                shm->users[j].opponent_idx = my_idx;
                shm->users[my_idx].is_matchmaking = false;
                shm->users[my_idx].opponent_idx = j;
                opponent_idx = j;
                break;
            }
        }
        sem_post(&shm->mutex);

        if (opponent_idx != -1) break;
        printf("Searching... [%d s]\n", 35 - i);
        sleep(1);
    }

    sem_wait(&shm->mutex);
    shm->users[my_idx].is_matchmaking = false;
    sem_post(&shm->mutex);

    bool is_bot = (opponent_idx == -1);
    
    char enemy_name[50];
    int enemy_max_hp, enemy_dmg;
    int bot_hp = 0;

    if (is_bot) {
        strcpy(enemy_name, "Monster (Bot)");
        enemy_max_hp = 50;
        enemy_dmg = 5;
        bot_hp = enemy_max_hp;
    } else {
        strcpy(enemy_name, shm->users[opponent_idx].username);
        enemy_max_hp = 100 + (shm->users[opponent_idx].xp / 10);
    }

    set_nonblocking_mode();
    tcflush(STDIN_FILENO, TCIFLUSH);

    time_t last_attack = 0;
    time_t last_bot_attack = time(NULL);
    bool battle_running = true;
    bool is_win = false;

    render_battle(shm->users[my_idx].username, shm->users[my_idx].current_hp, my_max_hp, enemy_name, is_bot ? bot_hp : shm->users[opponent_idx].current_hp, enemy_max_hp);

    int last_known_hp = my_max_hp;

    while (battle_running) {
        char input;
        time_t current_time = time(NULL);
        
        int current_enemy_hp = is_bot ? bot_hp : shm->users[opponent_idx].current_hp;
        
        if (shm->users[my_idx].current_hp <= 0) {
            battle_running = false;
            is_win = false;
            break;
        }
        
        if (current_enemy_hp <= 0) {
            battle_running = false;
            is_win = true;
            break;
        }

        if (shm->users[my_idx].current_hp != last_known_hp) {
            last_known_hp = shm->users[my_idx].current_hp;
            render_battle(shm->users[my_idx].username, shm->users[my_idx].current_hp, my_max_hp, enemy_name, current_enemy_hp, enemy_max_hp);
        }

        if (is_bot && current_time - last_bot_attack >= 2) {
            sem_wait(&shm->mutex);
            shm->users[my_idx].current_hp -= enemy_dmg;
            sem_post(&shm->mutex);
            last_bot_attack = current_time;
        }

        if (read(STDIN_FILENO, &input, 1) > 0) {
            if ((input == 'a' || input == 'u') && current_time - last_attack >= 1) {
                int dmg_dealt = my_dmg;
                
                if (input == 'u') {
                    if (shm->users[my_idx].weapon_bonus_dmg > 0) {
                        dmg_dealt = my_dmg * 3;
                    } else {
                        dmg_dealt = 0; 
                    }
                }

                if (dmg_dealt > 0) {
                    if (is_bot) {
                        bot_hp -= dmg_dealt;
                    } else {
                        sem_wait(&shm->mutex);
                        shm->users[opponent_idx].current_hp -= dmg_dealt;
                        sem_post(&shm->mutex);
                    }
                    last_attack = current_time;
                    render_battle(shm->users[my_idx].username, shm->users[my_idx].current_hp, my_max_hp, enemy_name, is_bot ? bot_hp : shm->users[opponent_idx].current_hp, enemy_max_hp);
                }
            }
        }
        usleep(50000); 
    }

    reset_terminal_mode();
    while(getchar() != '\n' && getchar() != EOF); 

    clear_screen();
    sem_wait(&shm->mutex);
    
    int hc = shm->users[my_idx].history_count;
    if (hc >= MAX_HISTORY) {
        for (int i = 1; i < MAX_HISTORY; i++) {
            shm->users[my_idx].match_history[i-1] = shm->users[my_idx].match_history[i];
        }
        hc = MAX_HISTORY - 1;
    }

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(shm->users[my_idx].match_history[hc].time_str, sizeof(shm->users[my_idx].match_history[hc].time_str), "%H:%M:%S", tm);
    strcpy(shm->users[my_idx].match_history[hc].opponent, enemy_name);

    if (is_win) {
        printf("VICTORY!\n");
        shm->users[my_idx].xp += 50;
        shm->users[my_idx].gold += 120;
        strcpy(shm->users[my_idx].match_history[hc].result, "WIN");
        shm->users[my_idx].match_history[hc].xp_gained = 50;
    } else {
        printf("DEFEAT!\n");
        shm->users[my_idx].xp += 15;
        shm->users[my_idx].gold += 30;
        strcpy(shm->users[my_idx].match_history[hc].result, "LOSS");
        shm->users[my_idx].match_history[hc].xp_gained = 15;
    }
    
    shm->users[my_idx].history_count = hc + 1;
    update_stats(shm, my_idx);
    shm->users[my_idx].opponent_idx = -1;
    sem_post(&shm->mutex);

    printf("Press [Enter] to continue...\n");
    getchar();
}

void open_armory(SharedData *shm, int my_idx) {
    int choice;
    while(1) {
        clear_screen();
        printf("--- ARMORY ---\n");
        
        sem_wait(&shm->mutex);
        int current_gold = shm->users[my_idx].gold;
        sem_post(&shm->mutex);

        printf("Gold: %d\n", current_gold);
        printf("1. Wood Sword   |  100 G |   +5 Dmg\n");
        printf("2. Iron Sword   |  300 G |  +15 Dmg\n");
        printf("3. Steel Axe    |  600 G |  +30 Dmg\n");
        printf("4. Demon Blade  | 1500 G |  +60 Dmg\n");
        printf("5. God Slayer   | 5000 G | +150 Dmg\n");
        printf("0. Back | Choice: ");

        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n');
            continue;
        }

        if (choice == 0) break;

        int price = 0, bonus = 0;
        if (choice == 1) { price = 100; bonus = 5; }
        else if (choice == 2) { price = 300; bonus = 15; }
        else if (choice == 3) { price = 600; bonus = 30; }
        else if (choice == 4) { price = 1500; bonus = 60; }
        else if (choice == 5) { price = 5000; bonus = 150; }
        else continue;

        sem_wait(&shm->mutex);
        if (shm->users[my_idx].gold >= price) {
            shm->users[my_idx].gold -= price;
            if (bonus > shm->users[my_idx].weapon_bonus_dmg) {
                shm->users[my_idx].weapon_bonus_dmg = bonus;
            }
            printf("Pembelian berhasil!\n");
        } else {
            printf("Gold tidak cukup!\n");
        }
        sem_post(&shm->mutex);
        
        sleep(1);
    }
}

void view_history(SharedData *shm, int my_idx) {
    clear_screen();
    printf("--- MATCH HISTORY ---\n\n");
    printf("%-10s | %-15s | %-6s | %-5s\n", "Time", "Opponent", "Result", "XP");
    printf("--------------------------------------------------\n");

    sem_wait(&shm->mutex);
    int hc = shm->users[my_idx].history_count;
    if (hc == 0) {
        printf("Belum ada histori pertempuran.\n");
    } else {
        for (int i = hc - 1; i >= 0; i--) {
            printf("%-10s | %-15s | %-6s | +%-4d\n", 
                shm->users[my_idx].match_history[i].time_str,
                shm->users[my_idx].match_history[i].opponent,
                shm->users[my_idx].match_history[i].result,
                shm->users[my_idx].match_history[i].xp_gained);
        }
    }
    sem_post(&shm->mutex);

    printf("\nPress [Enter] to back...\n");
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    getchar();
}

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shmid < 0) exit(1);

    SharedData *shm = (SharedData *)shmat(shmid, NULL, 0);
    if (!shm->orion_ready) exit(1);

    int choice;
    while (1) {
        clear_screen();
        printf("1. Register\n2. Login\n3. Exit\nChoice: ");
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n'); 
            continue;
        }

        if (choice == 1) {
            char uname[50], pass[50];
            printf("Username: ");
            scanf("%s", uname);
            printf("Password: ");
            scanf("%s", pass);

            sem_wait(&shm->mutex);
            bool exists = false;
            for (int i = 0; i < shm->user_count; i++) {
                if (strcmp(shm->users[i].username, uname) == 0) exists = true;
            }

            if (!exists && shm->user_count < MAX_USERS) {
                int idx = shm->user_count;
                strcpy(shm->users[idx].username, uname);
                strcpy(shm->users[idx].password, pass);
                shm->users[idx].gold = 150;
                shm->users[idx].lvl = 1;
                shm->users[idx].xp = 0;
                shm->users[idx].weapon_bonus_dmg = 0;
                shm->users[idx].is_logged_in = false;
                shm->users[idx].is_matchmaking = false;
                shm->users[idx].history_count = 0;
                shm->user_count++;
            }
            sem_post(&shm->mutex);

        } else if (choice == 2) {
            char uname[50], pass[50];
            printf("Username: ");
            scanf("%s", uname);
            printf("Password: ");
            scanf("%s", pass);

            int my_idx = -1;
            sem_wait(&shm->mutex);
            for (int i = 0; i < shm->user_count; i++) {
                if (strcmp(shm->users[i].username, uname) == 0 && strcmp(shm->users[i].password, pass) == 0) {
                    if (!shm->users[i].is_logged_in) {
                        my_idx = i;
                        shm->users[i].is_logged_in = true;
                    }
                    break;
                }
            }
            sem_post(&shm->mutex);

            if (my_idx >= 0) {
                bool in_menu = true;
                while (in_menu) {
                    clear_screen();
                    update_stats(shm, my_idx);
                    printf("PROFILE: %s | Lvl: %d | XP: %d | Gold: %d\n", shm->users[my_idx].username, shm->users[my_idx].lvl, shm->users[my_idx].xp, shm->users[my_idx].gold);
                    printf("1. Battle\n2. Armory\n3. History\n4. Logout\n> Choice: ");
                    int sub_choice;
                    if (scanf("%d", &sub_choice) != 1) {
                        while(getchar() != '\n');
                        continue;
                    }

                    if (sub_choice == 1) {
                        start_battle(shm, my_idx);
                    } else if (sub_choice == 2) {
                        open_armory(shm, my_idx);
                    } else if (sub_choice == 3) {
                        view_history(shm, my_idx);
                    } else if (sub_choice == 4) {
                        sem_wait(&shm->mutex);
                        shm->users[my_idx].is_logged_in = false;
                        sem_post(&shm->mutex);
                        in_menu = false;
                    }
                }
            }
        } else if (choice == 3) {
            break;
        }
    }

    shmdt(shm);
    return 0;
}