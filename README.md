# SISOP-3-2026-IT-096
## Afriezal Suryapraba Laiasach || 5027251096
![tree](/assets/tree.png)
### Soal 1
Pada soal pertama ini kami akan membuat sebuah program chat simple menggunakan multithreading.  
#### Protocol.h
 
```c
#ifndef PROTOCOL_H
#define PROTOCOL_H
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 2048
#define MAX_CLIENTS 50
#endif
```
File ini digunakan untuk menaruh hal yang akan digunakan server dan client seperti IP, Port, Buffer size, jumlah max client, sehingga pembuatan program dan debugging lebih mudah. 
```c 
#ifndef PROTOCOL_H
#define PROTOCOL_H
```
 adalah  pencegah error kompilasi ganda. Jika ada banyak file yang memanggil ```#include "protocol.h"```, compiler C akan mengecek: "Apakah PROTOCOL_H sudah didefinisikan sebelumnya?" Jika belum (#ifndef = if not defined), maka baca isinya. Jika sudah, lewati saja. Ini mencegah variabel/konstanta dideklarasikan berulang kali yang bisa menyebabkan error.  IP dan port juga dimasukan pada file yang nanti akan digunakan untuk menyambungkan client ke servernya.  di file ini juga ada untuk menentukan besar buffer yang dimana saya pilih sebanyak 2048, ini untuk membatasi berapa banyak character yang akan di ketik sehingga tidak terjadi overflow. MAX_CLIENTS digunakan untuk menentuan berapa banyak klien yang bisa diterima. 

#### wired.c
```c 
typedef struct {
    int sockfd;
    char name[50];
    int is_active;
    int is_admin;
} Client;

Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
time_t start_time;
```
Kita membuat sebuah awalan untuk Client untuk menyimpan data penting setiap pengguna: ID soket jaringan (sockfd), nama, status online (is_active), dan hak akses (is_admin). clients[MAX_CLIENTS] adalah array yang bertindak untuk mencatat client ke server untuk memantau siapa saja yang sedang online. clients_mutex dan log_mutex adalah gembok (Mutex) untuk mencegah Race Condition saat banyak thread (klien) mencoba membaca/menulis data secara bersamaan.

```c
void log_write(const char *entity, const char *action){
    pthread_mutex_lock(&log_mutex);
    FILE *fp = fopen("history.log","a");
    if(fp != NULL){
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] [%s]\n",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                entity, action);
        fclose(fp);
    }
    pthread_mutex_unlock(&log_mutex);
}
```
Pertama, fungsi akan mengunci gembok log_mutex agar tidak ada klien lain yang ikut menulis log di saat bersamaan. Kemudian, file history.log dibuka dalam mode append ("a"). Fungsi mengambil waktu sistem saat ini menggunakan time() dan localtime(), memformatnya sesuai aturan, menuliskannya ke dalam file, menutup file, dan terakhir membuka kembali gemboknya.

```c
void broadcast_mutex(const char *message, int sender_fd){
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i].is_active && clients[i].sockfd != sender_fd && !clients[i].is_admin){
            send(clients[i].sockfd, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}
```
Fungsi ini menerima teks pesan dan ID soket si pengirim. Ia mengunci array klien, lalu mengecek semua slot dari 0 hingga batas maksimum. Jika slot tersebut berisi klien yang sedang online (is_active), bukan soket si pengirim (!= sender_fd), dan bukan admin (!is_admin), maka server akan meneruskan pesan tersebut menggunakan perintah send().

```c
        if(strcmp(name, "The Knights")==0){
            send(cli_sock, "AUTH_PASS", 9, 0);
            recv(cli_sock, buffer, BUFFER_SIZE, 0);
            is_admin=1;
            send(cli_sock, "AUTH_ADMIN_OK...", 16, 0);
            break;
        }else{
            int name_exists=0;
            pthread_mutex_lock(&clients_mutex);
            for(int i=0; i<MAX_CLIENTS; i++){
                if(clients[i].is_active && strcmp(clients[i].name, name)==0){
                    name_exists=1; break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);

            if(name_exists){
                send(cli_sock, "AUTH_ERR...", 11, 0); 
            }else{
                send(cli_sock, "AUTH_OK...", 10, 0);  
                break;
            }
        }
```
Klien mengirimkan namanya. Jika namanya "The Knights", server meminta password tambahan dan memberinya hak admin. Jika bukan, server mengecek "Buku Tamu". Jika nama sudah dipakai orang yang sedang online, tolak koneksinya. Jika aman, persilakan masuk.

```c
  pthread_mutex_lock(&clients_mutex);
    for(int i =0; i<MAX_CLIENTS;i++){
        if(!clients[i].is_active){
            clients[i].sockfd=cli_sock;
            strncpy(clients[i].name, name,sizeof(clients[i].name));
            clients[i].is_active=1;
            clients[i].is_admin=is_admin;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    while(1){
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(cli_sock,buffer,BUFFER_SIZE,0);
        if(bytes_received<=0)break;
        buffer[strcspn(buffer,"\n")]=0;

        if(strcmp(buffer,"/exit")==0 || (is_admin && strcmp(buffer,"4")==0))break;

        if(is_admin){
            if(strcmp(buffer,"1")==0){
                log_write("Admin", "RPC_GER_USERS");
                int count=0;
                pthread_mutex_lock(&clients_mutex);
                for(int i=0;i<MAX_CLIENTS;i++){
                    if(clients[i].is_active && !clients[i].is_admin) count++;
                }
                pthread_mutex_unlock(&clients_mutex);
                char res[BUFFER_SIZE];
                sprintf(res,"[System] Active Entities: %d\n",count);
                send(cli_sock,res,strlen(res),0);
            }else if (strcmp(buffer,"2")==0){
                log_write("Admin", "RPC_GET_UPTIME");
                time_t current_time = time(NULL);
                double seconds = difftime(current_time,start_time);
                char res[BUFFER_SIZE];
                sprintf(res, "[System] Server Uptime: %.0f seconds\n", seconds);
                send(cli_sock,res,strlen(res),0);
            }else if (strcmp(buffer, "3") == 0) {
                log_write("Admin", "RPC_SHUTDOWN");
                log_write("System", "EMERGENCY SHUTDOWN INITIATED");
                printf("Emergency Shutdown initiated by Admin.\n");
                exit(0);
            }
        }else{
            char chat_log[BUFFER_SIZE];
            sprintf(chat_log, "[%s]: %s", name, buffer);
            log_write("User",chat_log);

            char broadcast_buf[BUFFER_SIZE];
            sprintf(broadcast_buf, "[%s]: %s\n", name, buffer);
            broadcast_mutex(broadcast_buf, cli_sock);
        }
    }
```
Setelah lulus masuk, data klien disimpan ke array. Lalu masuk ke infinite loop (while(1)) untuk mendengarkan pesan masuk (recv). Jika admin, inputannya diproses sebagai fungsi khusus. Jika user biasa, pesan diteruskan ke broadcast_mutex dan dicatat di log_write.
```c
 char log_msg[BUFFER_SIZE];
    sprintf (log_msg, "User '%s' disconnected", name );
    log_write("System", log_msg);
    pthread_mutex_lock(&clients_mutex);
    for(int i=0;i<MAX_CLIENTS;i++){
        if(clients[i].sockfd ==cli_sock){
            clients[i].is_active=0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    close(cli_sock);
    return NULL;
```
Jika klien mengirim pesan /exit atau jaringannya terputus secara tiba-tiba, loop akan berhenti. Fungsi ini bertugas membersihkan sisa-sisa klien tersebut dengan mengubah statusnya menjadi 0 (agar nama dan slotnya bisa dipakai orang lain) dan menutup jalur soket komunikasinya.

```c
int main(){
    start_time = time(NULL);
    log_write("System", "SERVER ONLINE");

    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].is_active = 0;

    int server_sock, *new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT); 
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); 

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind gagal"); exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 10) == 0) {
        printf("The Wired is listening on %s:%d...\n", SERVER_IP, SERVER_PORT);
    } else {
        printf("Listen error\n"); exit(EXIT_FAILURE);
    }

    while (1) {
        new_sock = malloc(sizeof(int));
        *new_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (*new_sock < 0) {
            perror("Accept gagal"); free(new_sock); continue;
        }
        
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, (void*)new_sock);
        pthread_detach(client_thread);
    }

    close(server_sock);
    return 0;
}
```
Fungsi ini mencatat start_time (yang nanti akan dipakai oleh admin untuk mengecek uptime). Kemudian, mengatur konfigurasi jaringan soket TCP menggunakan IP dan Port dari file protocol.h. Server lalu diam di accept() menunggu ada klien yang menyambung.  
Begitu ada yang menyambung, alih-alih melayaninya langsung, server menyewa pekerja bayangan (pthread_create) untuk menjalankan fungsi handle_client yang sudah kita bahas di atas. pthread_detach memastikan memori dikembalikan ke sistem setelah klien pergi.

#### navi.c
```c 
void sigint_handler(int sig) {
    printf("\n[System] Disconnecting from The Wired...\n");
    send(sock, "/exit\n", 6, 0);
    close(sock);
    exit(0);
}
```
Secara default, jika menekan Ctrl+C di terminal Linux, program akan langsung dihentikan seketika oleh sistem operasi (mengirim sinyal SIGINT). Ini berbahaya karena server The Wired di seberang sana tidak akan tahu bahwa client sudah mati (menjadi Ghost Connection).
Fungsi ini mencegat tombol Ctrl+C itu. Alih-alih langsung mati, program akan diam-diam mengirimkan pesan rahasia "/exit" ke server agar server bisa menghapus Anda dari "Buku Tamu", barulah setelah itu klien menutup programnya sendiri dengan tenang.

```c
void *receive_messages(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[BUFFER_SIZE];
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("\n[System] Connection lost to The Wired.\n");
            exit(0);
        }
        printf("\r%s", buffer);
        
        if (is_admin) printf("Command >> ");
        else printf("> ");
        fflush(stdout);
    }
    return NULL;
}
```
Fungsi ini berjalan di latar belakang (background) sebagai thread terpisah. Tugasnya cuma satu: melototi koneksi jaringan dari server menggunakan recv().
Jika ada obrolan masuk, ia akan mencetaknya dengan awalan \r (Carriage Return) agar teks tersebut digeser ke ujung paling kiri layar terminal, menimpa apa pun yang sedang ada di situ sementara waktu. Setelah pesan dicetak, ia akan memunculkan ulang prompt >  dan memaksanya tampil dengan fflush(stdout), sehingga Anda tidak kehilangan baris tempat client mengetik.

```c
int main() {
    signal(SIGINT, sigint_handler);

    struct sockaddr_in server_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT); 
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); 

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed"); exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    char input[BUFFER_SIZE];
```
Klien mendaftarkan fungsi pencegat Ctrl+C. Lalu, klien mengambil konfigurasi alamat IP dan Port dari file protocol.h (tanpa di-hardcode di dalam navi.c) dan "menelepon" server menggunakan perintah connect().
```c
 while (1) {
        printf("Enter your name: ");
        fgets(input, BUFFER_SIZE, stdin);
        input[strcspn(input, "\n")] = 0;
        send(sock, input, strlen(input), 0);

        memset(buffer, 0, BUFFER_SIZE);
        recv(sock, buffer, BUFFER_SIZE, 0);

        if (strncmp(buffer, "AUTH_ERR", 8) == 0) {
            printf("%s", buffer + 9);
        } 
        else if (strncmp(buffer, "AUTH_PASS", 9) == 0) {
            printf("Enter Password: ");
            fgets(input, BUFFER_SIZE, stdin);
            input[strcspn(input, "\n")] = 0;
            send(sock, input, strlen(input), 0);
            
            memset(buffer, 0, BUFFER_SIZE);
            recv(sock, buffer, BUFFER_SIZE, 0);
            if (strncmp(buffer, "AUTH_ADMIN_OK", 13) == 0) {
                is_admin = 1;
                printf("[System] Authentication Successful. Granted Admin privileges.\n\n");
                printf("%s", buffer + 14);
                break;
            }
        }
        else if (strncmp(buffer, "AUTH_OK", 7) == 0) {
            printf("%s", buffer + 8);
            break;
        }
    }
```
Setelah tersambung, klien mengirimkan input nama. Klien kemudian membeku sesaat (recv) menunggu vonis dari server. Jika server membalas dengan AUTH_ERR (nama sudah dipakai), klien meminta input nama lagi. Jika server membalas AUTH_PASS, klien menyadari bahwa ia adalah admin dan akan memunculkan prompt password. Jika balasan AUTH_OK, klien dipersilakan masuk ke ruang chat.
```c
pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, (void*)&sock);
    while (1) {
        if (is_admin) printf("Command >> ");
        else printf("> ");
        
        fgets(input, BUFFER_SIZE, stdin);
        
        if (strcmp(input, "\n") == 0) continue;

        send(sock, input, strlen(input), 0);

        if (strncmp(input, "/exit", 5) == 0 || (is_admin && strncmp(input, "4", 1) == 0)) {
            printf("[System] Disconnecting from The Wired...\n");
            break;
        }
        
        usleep(100000); 
    }

    close(sock);
    return 0;
}
```
Ini adalah inti dari asinkronisasi antarmuka. Kita memicu "pekerja pendengar" (pthread_create) yang akan menjalankan fungsi receive_messages secara paralel.
Sementara "pekerja" itu sibuk mendengarkan server di background, program utama (while(1)) hanya diam menunggu Anda selesai mengetik satu baris kalimat (fgets) lalu mengirimkannya. Jika Anda mengetik /exit, loop ini akan dihancurkan (break) dan program selesai.

#### Output
1. Wired  
![wired](/assets/soal1/wired.png)
2. Navi  
![navi](/assets/soal1/navi.png)
3. Chatting  
![chat](/assets/soal1/chat.png)
4. Jika memasukan nama yang sama di navi  
![sameName](/assets/soal1/sameName.png)
5. Exit dengan /exit dan ctrl+c  
![exit](/assets/soal1/exit.png)  
![ctrlC](/assets/soal1/controlC.png)
6. Knights dan mematikan server  
![knights](/assets/soal1/knights.png)  
![wiredKilled](/assets/soal1/wiredMati.png)
7. history  
![history](/assets/soal1/history.png)

#### Bug dan Kendala
Terdapat sebuah bug dimana saat mengetikan command yang ke 2 untuk admin, tulisan 'command>> tercetak dua kali.  
Pada line ke 46 di navi.c, terdapat syntax error karena typo di akhir line yang secara tidak sengaja tertambah karakte ' - ' yang menyebabkan file tidak dapat dicompile

### Soal 2
Pada soal kedua ini, kami diminta untuk membuat sebuah sistem permainan pertempuran berbasis client-server bernama "Battle Eterion". Program ini sangat bergantung pada konsep Inter-Process Communication (IPC) menggunakan Shared Memory untuk berbagi data antar proses secara global, serta menggunakan Semaphore (Mutex) untuk mencegah terjadinya Race Condition saat beberapa klien mengakses data secara bersamaan. Selain itu, sistem pertempuran mewajibkan proses berjalan secara real-time (asynchronous), sehingga kami harus memanipulasi mode terminal.
#### Makefile
```file
CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS = -lrt

all: server client

server: orion.c arena.h
	$(CC) $(CFLAGS) orion.c -o orion $(LDFLAGS)

client: eternal.c arena.h
	$(CC) $(CFLAGS) eternal.c -o eternal $(LDFLAGS)

clean:
	rm -f orion eternal

clear_ipc:
	ipcs -m | grep 0x00001234 | awk '{print $$2}' | xargs -r ipcrm -m
	ipcs -q | grep 0x00005678 | awk '{print $$2}' | xargs -r ipcrm -q
	ipcs -s | grep 0x00009012 | awk '{print $$2}' | xargs -r ipcrm -s
```
File ini digunakan untuk mempermudah proses kompilasi orion.c (server) dan eternal.c (client). Opsi -pthread dan -lrt wajib disertakan agar library untuk multithreading dan shared memory dapat berfungsi. Bagian terpenting di sini adalah clear_ipc. Saat program dihentikan secara paksa, memori bersama (shared memory) seringkali masih tersandera di RAM komputer. Perintah ini berfungsi untuk mencari ID memori berdasarkan key 0x00001234 dan menghapusnya dari sistem operasi menggunakan ipcrm.

#### arena.h
```c
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
```
File ini bertindak sebagai kerangka data utama. SharedData adalah struktur yang nantinya akan dipetakan (di-mapping) ke dalam Shared Memory. Di dalamnya terdapat sem_t mutex yang bertindak sebagai gembok digital. Semua data pemain, mulai dari kredensial login, status (Gold, XP, Level, HP), hingga riwayat pertandingan (History) disimpan di dalam array users agar bisa diakses oleh program manapun yang terhubung ke memori tersebut.
#### orion.c
```c
#include "arena.h"

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        exit(1);
    }

    SharedData *shm = (SharedData *)shmat(shmid, NULL, 0);

    sem_init(&shm->mutex, 1, 1);
    
    shm->orion_ready = true;
    shm->user_count = 0; 

    printf("Orion is ready (PID: %d)\n", getpid());

    while (1) {
        sleep(1);
    }

    shmdt(shm);
    return 0;
}
```
orion.c bertindak sebagai server atau penyedia lapak. Program ini meminta alokasi memori ke sistem operasi menggunakan shmget dengan ukuran sebesar struct SharedData. Kemudian ia menempelkan memori tersebut ke prosesnya dengan shmat. Server ini menginisialisasi nilai awal mutex menjadi 1 (terbuka) agar siap digunakan klien. Setelah mengatur status orion_ready menjadi true, server akan masuk ke mode daemon (berjalan di background menggunakan while(1)) dan membiarkan klien (eternal) saling berinteraksi lewat memori tersebut.
#### eternal.c
```c
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
```
Fungsi ini adalah kunci utama agar pertempuran bisa berjalan asinkron (real-time). Secara bawaan, terminal Linux berada di canonical mode yang akan memblokir (pause) eksekusi program sampai pengguna menekan Enter. Dengan memanipulasi termios dan mengubah properti sistem menggunakan fcntl menjadi O_NONBLOCK, program bisa membaca ketikan keyboard secara instan tanpa menghentikan berjalannya pertempuran.
```c
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
```
Fungsi update_stats() memastikan bahwa perhitungan level pemain selalu termutakhir berdasarkan akumulasi Experience Points (XP) mereka. Level bertambah satu untuk setiap kelipatan 100 XP.
Fungsi render_battle() digunakan untuk menggambar ulang (re-draw) antarmuka Arena di terminal. Fungsi ini mencetak nama, Health Points (HP) saat ini, dan kapasitas HP maksimal. Jika HP menyentuh angka minus, fungsi ini akan mencegahnya tercetak di bawah nol dengan operator ternary my_hp > 0 ? my_hp : 0.
```c
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
```
Fungsi start_battle() menangani keseluruhan fase pencarian lawan dan pertempuran itu sendiri. Sebelum memulai pencarian, program mengatur status kesiapan bertarung pemain ke is_matchmaking = true.
Kemudian, program memasuki loop pencarian (matchmaking) berdurasi maksimum 35 detik (sleep(1) diulang 35 kali). Pada setiap detiknya, klien mengunci Shared Memory menggunakan sem_wait() dan mengecek array users. Jika ditemukan pengguna lain yang juga berstatus is_matchmaking, mereka akan "dikaitkan". Indeks memori dari pengguna lawan akan disimpan ke variabel opponent_idx masing-masing, dan status pencarian keduanya dicabut agar tidak ditemukan oleh pemain ketiga. Jika selama 35 detik berlalu tidak ada yang dikaitkan, variabel opponent_idx akan tetap bernilai -1, yang berarti klien tersebut akan dipasangkan dengan bot (Monster).
```c
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
    ```
Setelah fase matchmaking selesai, sistem akan mengalkulasi statistik lawan. Jika musuhnya adalah bot, atributnya telah ditentukan dengan spesifikasi yang lebih lemah (HP 50, Damage 5). Jika melawan pemain asli, kapasitas nyawa (Max HP) lawan dihitung secara dinamis dari rumus dasar (100) ditambah sepuluh persen dari total XP mereka.
Selanjutnya, mode terminal non-blocking diaktifkan. Fungsi tcflush() juga dipanggil untuk membuang antrean buffer keyboard yang mungkin tidak sengaja tertekan selama pemain menunggu di menu matchmaking, mencegah eksekusi perintah tak terduga.
```c
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
```
Ini adalah inti asinkronisasi sistem pertempuran. Program berputar dalam sebuah loop (while (battle_running)). Pada setiap siklusnya, ia mengambil waktu aktual sistem dan memantau status HP pemain serta lawannya. Jika HP siapa pun menyentuh nol, loop dihentikan secara prematur dengan status Win atau Loss yang relevan.
Sistem juga memiliki mekanisme refresh yang optimal: layar terminal tidak selalu diperbarui secara paksa (flickering). Klien hanya akan memanggil render_battle() jika current_hp pemain berubah secara signifikan dari pemeriksaan terakhir mereka (last_known_hp), menandakan mereka baru saja terkena serangan.
```c
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
```
Karena bot tidak dikendalikan oleh program klien independen lain, logika serangan bot disematkan langsung di dalam loop ini. Jika durasi dari serangan bot terakhir telah melebihi dua detik, bot akan secara mandiri mengunci Shared Memory dan merusak (-=) HP pemain.
Sistem input pemain bergantung pada fungsi read(). Meskipun diletakkan di non-blocking mode, jika ada karakter yang berhasil masuk (seperti menekan tombol 'a' atau 'u'), fungsi akan mengeksekusi validasi cooldown 1 detik. Jika cooldown terpenuhi, sistem akan memperhitungkan damage yang dihasilkan. Serangan Ultimate (u) hanya akan berfungsi dan memberikan damage 3 kali lipat jika pemain sudah membeli setidaknya satu senjata dari Armory (weapon_bonus_dmg > 0). Terakhir, usleep(50000) memberikan sedikit jeda istirahat bagi CPU agar looping tiada henti ini tidak menghabiskan terlalu banyak memori operasional.
```c
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
 ```
Ketika loop terhenti, mode non-blocking terminal dimatikan dan semua sisa spam tombol dari pengguna dibersihkan. Kemudian, sistem akan mencatat hasil pertempuran ke dalam array History di memori bersama.
Jika catatan riwayat sudah melebihi kuota maksimum (MAX_HISTORY), fungsi ini akan secara cerdas menggeser setiap elemen array satu indeks ke bawah, menghapus catatan paling lawas agar ada ruang untuk entri riwayat baru. Setelah fungsi Library C Time (strftime) berhasil memformat stempel waktu (timestamp), sistem mengevaluasi boolean is_win. Pemenang akan dihadiahi 50 XP dan 120 Gold, sementara pemain yang kalah mendapatkan consolation prize sebesar 15 XP dan 30 Gold. Seluruh hadiah ini, beserta hasil dan stempel waktunya, akan disimpan dengan aman di dalam Shared Memory.
```c
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
```
Fungsi open_armory menghadirkan sistem perbelanjaan di mana pemain dapat memperkuat serangannya. Dalam fungsi ini, klien memeriksa dompet Gold dari Shared Memory. Sistem Semaphore Mutex kembali digunakan untuk menjamin bahwa transaksi tidak dimanipulasi oleh konflik baca/tulis memori. Jika dana cukup, saldo pemain dipotong sebesar harga, dan kekuatan damage senjata baru dimasukkan ke weapon_bonus_dmg. Secara spesifik, program dirancang hanya menimpa variabel jika nilai bonus senjata yang baru dibeli lebih tinggi dari senjata yang sedang digunakan; dengan demikian, status tidak menurun kembali jika seseorang tak sengaja membeli senjata rendahan.
```c
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
```
Fungsi terakhir, view_history(), memberikan representasi visual dari riwayat yang telah diarsip di dalam Shared Memory pascapertempuran. Iterasinya diatur untuk berjalan mundur (for (int i = hc - 1; i >= 0; i--)) agar catatan yang paling baru dimainkan selalu tercetak di bagian paling atas tampilan riwayat. Ini meningkatkan tingkat navigasi dan kenyamanan pengguna secara signifikan.
```c
nt main() {
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
```
Bagian main() dari program bertindak sebagai pintu masuk dari interaksi IPC. Proses pendaftaran akun (Register) maupun pendaftaran login selalu menggunakan kunci pengaman sem_wait() dan sem_post(). Sebagai langkah penjamin integritas data tambahan, fase verifikasi memeriksa kecocokan tidak hanya antara kombinasi nama sandi tetapi juga meninjau variabel is_logged_in. Langkah krusial ini menghindarkan eksploitasi multi-terminal dengan akun tunggal.
#### Output
1. Make file  
![make](/assets/soal2/make.png)
2. Orion  
![orion](/assets/soal2/orion.png)
3. Eternal  
![Eternal](/assets/soal2/eternal.png)
4. Regist & Login  
![Regis](/assets/soal2/register.png)  
![Login](/assets/soal2/login.png)
5. Main Menu  
![Main Menu](/assets/soal2/mainMenu.png)
6. PVP & PVE  
![PVP](/assets/soal2/pvp.png)  
![PVE](/assets/soal2/pve.png)
7. Armory  
![Armory](/assets/soal2/armory.png)
8. History  
![history](/assets/soal2/history.png)
