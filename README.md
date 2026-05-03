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