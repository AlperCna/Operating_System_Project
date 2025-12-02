#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include <mqueue.h>
#include <time.h>
#include <errno.h>


// --- Veri Yapıları (Data Structures) ---

typedef enum { ATTACHED = 0, DETACHED = 1 } ProcessMode;
typedef enum { RUNNING = 0, TERMINATED = 1 } ProcessStatus;

// 9. ve 10. GÜN: Mesaj Komutları
#define CMD_START 1
#define CMD_TERMINATE 2

typedef struct {
    pid_t pid;
    pid_t owner_pid;
    char command[256];
    ProcessMode mode;
    ProcessStatus status;
    time_t start_time;
    int is_active;
} ProcessInfo;

typedef struct {
    ProcessInfo processes[50];
    int process_count;
} SharedData;

typedef struct {
    long msg_type;
    int command;
    pid_t sender_pid;
    pid_t target_pid;
} Message;

// IPC İsimleri (PDF Tablo 5.2'den alındı)
#define SHM_NAME "/procx_shm"
#define SEM_NAME "/procx_sem"
#define MQ_NAME  "/procx_mq"

// --- Global Değişkenler (Tüm fonksiyonlar erişebilsin diye) ---
int shm_fd;             // Shared Memory Dosya Tanımlayıcısı
SharedData *shared_mem; // Shared Memory'ye işaret eden pointer
sem_t *sem;             // Semafor
mqd_t msg_queue;        // Mesaj Kuyruğu

// --- Fonksiyon Prototipleri ---
void init_ipc();
void cleanup_ipc();
void show_menu();
void run_program();
void list_processes();
void terminate_program();
void exit_program();
void *monitor_thread(void *arg);
void *ipc_listener_thread(void *arg);

// --- Ana Fonksiyon ---
int main() {
    int choice;

    // 1. ADIM: IPC Mekanizmalarını Başlat (Günün Yıldızı Burası)
    init_ipc();

//  Monitor Thread Başlat (8. Gün)
    pthread_t tid_monitor;
    pthread_create(&tid_monitor, NULL, monitor_thread, NULL);
    pthread_detach(tid_monitor); // Thread'i serbest bırak, main kapanınca o da kapansın


 // 3. IPC Listener Thread Başlat (Mesaj Dinleyici - 10. Gün)
    pthread_t tid_listener;
    pthread_create(&tid_listener, NULL, ipc_listener_thread, NULL);
    pthread_detach(tid_listener);   

    // 2. ADIM: Menü Döngüsü
    while (1) {
        show_menu();
        printf("Your choice: ");
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            choice = -1; 
        }

        switch (choice) {
            case 1: run_program(); break;
            case 2: list_processes(); break;
            case 3: terminate_program(); break;
            case 0: exit_program(); return 0;
            default:
                printf("\n[ERROR] Invalid choice!\n");
                sleep(1);
        }
    }
    return 0;
}

// --- IPC Kurulum Fonksiyonu (BUGÜNÜN YENİLİĞİ) ---
void init_ipc() {
    // A. Shared Memory Oluşturma/Açma
    // O_CREAT: Yoksa oluştur, O_RDWR: Oku/Yaz, 0666: Okuma/Yazma izni
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Shared Memory acilamadi"); // Hata varsa ekrana bas
        exit(1);
    }
    
    // Boyutunu ayarla (sizeof(SharedData) kadar yer ayır)
    ftruncate(shm_fd, sizeof(SharedData));

    // Belleğe Map etme (Pointer ile erişim sağlama)
    shared_mem = (SharedData *)mmap(0, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("mmap hatasi");
        exit(1);
    }

    // B. Semafor Oluşturma/Açma
    // Başlangıç değeri 1 (Mutex gibi çalışsın diye, yani aynı anda sadece 1 kişi girebilir)
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("Semafor acilamadi");
        exit(1);
    }

    // C. Mesaj Kuyruğu Oluşturma
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;      // Maksimum 10 mesaj birikebilir
    attr.mq_msgsize = sizeof(Message); // Her mesajın boyutu
    attr.mq_curmsgs = 0;

    msg_queue = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
    if (msg_queue == (mqd_t)-1) {
        // Bazen attr parametresi sorun çıkarabilir, boş deneyelim
        msg_queue = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, NULL);
        if (msg_queue == (mqd_t)-1) {
            perror("Mesaj kuyrugu acilamadi");
            // Kritik hata değil, devam edebiliriz şimdilik
        }
    }

    printf("[INFO] IPC Sistemleri (SHM, SEM, MQ) baslatildi.\n");
    printf("[INFO] Shared Memory Adresi: %p\n", (void *)shared_mem);
    sleep(1); // Kullanıcı mesajı görsün diye bekleme
}

// --- Temizlik Fonksiyonu ---
void cleanup_ipc() {
    // Kaynakları serbest bırak (Program kapanırken çağrılır)
    if (munmap(shared_mem, sizeof(SharedData)) == -1) perror("munmap");
    if (close(shm_fd) == -1) perror("close shm");
    if (sem_close(sem) == -1) perror("sem_close");
    if (mq_close(msg_queue) == -1) perror("mq_close");
}

// --- Menü Fonksiyonları (Şimdilik Boş) ---
void show_menu() {
    printf("\n");
    printf("╔════════════════════════════════════╗\n");
    printf("║             ProcX v1.0             ║\n");
    printf("╠════════════════════════════════════╣\n");
    printf("║ 1. Run a new program               ║\n");
    printf("║ 2. List running programs           ║\n");
    printf("║ 3. Terminate a program             ║\n");
    printf("║ 0. Exit                            ║\n");
    printf("╚════════════════════════════════════╝\n");
}

// --- 5. GÜN: Detached Mod ve Shared Memory Kaydı (GÜNCELLENMİŞ HALİ) ---
void run_program() {
    char command[256];
    int mode;
    
    // 1. Buffer Temizliği
    while(getchar() != '\n'); 

    // 2. Kullanıcıdan Komut Al
    printf("\nEnter the command to run (e.g., sleep 10): ");
    if (scanf("%[^\n]", command) != 1) return;

    // 3. Modu Al
    printf("Choose running mode (0: Attached, 1: Detached): ");
    scanf("%d", &mode);

    // 4. FORK İşlemi
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return;
    }

    if (pid == 0) {
        // --- CHILD PROCESS ---
        if (mode == DETACHED) {
            if (setsid() < 0) {
                perror("setsid failed");
                exit(1);
            }
        }

        char *args[64];
        int i = 0;
        char *token = strtok(command, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        execvp(args[0], args);
        perror("[ERROR] Exec failed");
        exit(1);
    } else {
        // --- PARENT PROCESS ---
        
        sem_wait(sem);

        int i;
        for (i = 0; i < 50; i++) {
            if (shared_mem->processes[i].is_active == 0) {
                shared_mem->processes[i].pid = pid;
                shared_mem->processes[i].owner_pid = getpid(); 
                strcpy(shared_mem->processes[i].command, command);
                shared_mem->processes[i].mode = mode;
                shared_mem->processes[i].status = RUNNING;
                shared_mem->processes[i].start_time = time(NULL); 
                shared_mem->processes[i].is_active = 1; 
                
                shared_mem->process_count++;
                break; 
            }
        }
        
        sem_post(sem);

        if (i < 50) {
            printf("[SUCCESS] Process started: PID %d (Saved to SHM)\n", pid);

            // --- BURASI EKLENDİ (10. GÜN) ---
            Message msg;
            msg.msg_type = 1;
            msg.command = CMD_START;
            msg.sender_pid = getpid();
            msg.target_pid = pid;
            mq_send(msg_queue, (char*)&msg, sizeof(Message), 0);
            // -------------------------------

        } else {
            printf("[ERROR] Process list is full! PID %d running but not tracked.\n", pid);
        }
        
        sleep(1);
    }
}
// --- 6. GÜN: Süreçleri Listeleme (Listing) ---
void list_processes() {
    // 1. Semafor Kilidi (Okuma yaparken veri değişmesin diye kilitliyoruz) [cite: 106]
    sem_wait(sem);

    // Tablo Başlıkları - PDF Sayfa 6'daki tasarıma uygun [cite: 108-109]
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                       RUNNING PROGRAMS                        ║\n");
    printf("╠══════════╦══════════════════════╦══════════╦═════════╦════════╣\n");
    printf("║ %-8s ║ %-20s ║ %-8s ║ %-7s ║ %-6s ║\n", "PID", "Command", "Mode", "Owner", "Time");
    printf("╠══════════╬══════════════════════╬══════════╬═════════╬════════╣\n");

    int count = 0;
    time_t now = time(NULL); // Geçen süreyi hesaplamak için şimdiki zamanı al

    for (int i = 0; i < 50; i++) {
        // Sadece aktif (is_active == 1) olan süreçleri listele [cite: 99]
        if (shared_mem->processes[i].is_active) {
            
            // Geçen süreyi hesapla: (Şu an - Başlangıç zamanı) [cite: 105]
            double elapsed = difftime(now, shared_mem->processes[i].start_time);
            
            // Mod ismini yazıya çevir (0 -> Attached, 1 -> Detached)
            char *mode_str = (shared_mem->processes[i].mode == ATTACHED) ? "Attached" : "Detached";

            // Satırı tablo formatında yazdır
            printf("║ %-8d ║ %-20s ║ %-8s ║ %-7d ║ %-5.0fs ║\n",
                   shared_mem->processes[i].pid,
                   shared_mem->processes[i].command,
                   mode_str,
                   shared_mem->processes[i].owner_pid,
                   elapsed);
            
            count++;
        }
    }

    printf("╚══════════╩══════════════════════╩══════════╩═════════╩════════╝\n");
    printf("Total: %d processes\n", count); // [cite: 110]
 
    // 2. Kilidi Bırak (Bunu unutursak program donar!)
    sem_post(sem);
    
    // Kullanıcının tabloyu incelemesi için bekle
    printf("\nPress ENTER to return menu...");
    while(getchar() != '\n'); // Önceki enter'ı temizle
    getchar(); // Yeni enter bekle
}

// --- 7. GÜN: Süreç Sonlandırma (GÜNCELLENMİŞ HALİ) ---
void terminate_program() {
    int target_pid;
    int found = 0;

    printf("\nEnter PID to terminate: ");
    if (scanf("%d", &target_pid) != 1) {
        while(getchar() != '\n'); 
        return;
    }

    sem_wait(sem);

    for (int i = 0; i < 50; i++) {
        if (shared_mem->processes[i].is_active && shared_mem->processes[i].pid == target_pid) {
            
            if (kill(target_pid, SIGTERM) == 0) {
                printf("[SUCCESS] Signal SIGTERM sent to Process %d\n", target_pid);
                
                shared_mem->processes[i].is_active = 0;
                shared_mem->processes[i].status = TERMINATED;
                shared_mem->process_count--;
                found = 1;

                // --- BURASI EKLENDİ (10. GÜN) ---
                Message msg;
                msg.msg_type = 1;
                msg.command = CMD_TERMINATE;
                msg.sender_pid = getpid();
                msg.target_pid = target_pid;
                mq_send(msg_queue, (char*)&msg, sizeof(Message), 0);
                // -------------------------------

            } else {
                perror("[ERROR] Failed to kill process");
            }
            break; 
        }
    }

    sem_post(sem);

    if (!found) {
        printf("[ERROR] Process PID %d not found in the list.\n", target_pid);
    }
    
    sleep(1);
}

// --- 8. GÜN: Monitor Thread (GÜNCELLENMİŞ HALİ) ---
void *monitor_thread(void *arg) {
    int status;
    
    while (1) {
        sleep(2); 

        sem_wait(sem); 
        
        for (int i = 0; i < 50; i++) {
            if (shared_mem->processes[i].is_active && 
                shared_mem->processes[i].owner_pid == getpid()) {
                
                pid_t res = waitpid(shared_mem->processes[i].pid, &status, WNOHANG);
                
                if (res > 0) {
                    
                    shared_mem->processes[i].is_active = 0;
                    shared_mem->processes[i].status = TERMINATED;
                    shared_mem->process_count--;
                    
                    // --- BURASI EKLENDİ (10. GÜN) ---
                    Message msg;
                    msg.msg_type = 1;
                    msg.command = CMD_TERMINATE;
                    msg.sender_pid = getpid();
                    msg.target_pid = res;
                    mq_send(msg_queue, (char*)&msg, sizeof(Message), 0);
                    // -------------------------------

                    printf("\n[MONITOR] Process %d was terminated\n", res);
                }
            }
        }
        
        sem_post(sem); 
    }
    return NULL;
}

// --- 9. ve 10. GÜN: IPC Listener Thread (Mesaj Dinleyici) [cite: 726-734] ---
void *ipc_listener_thread(void *arg) {
    Message msg;
    while (1) {
        // Mesaj bekle (bloke olur, mesaj gelince uyanır)
        // Eğer hata olursa (örn: kuyruk kapatıldıysa) döngüden çık
        if (mq_receive(msg_queue, (char*)&msg, sizeof(Message), NULL) > 0) {
            
            // Kendi gönderdiğimiz mesajları ekrana basma (Sadece başkalarını gör)
            if (msg.sender_pid != getpid()) {
                if (msg.command == CMD_START) {
                    printf("\n\n[IPC] New process started: PID %d\n", msg.target_pid);
                } else if (msg.command == CMD_TERMINATE) {
                    printf("\n\n[IPC] Process terminated: PID %d\n", msg.target_pid);
                }
                
                // Menü satırını tekrar göster ki ekran karışmasın
                printf("Your choice: ");
                fflush(stdout);
            }
        } else {
            // Hata veya kapanış durumu
            break; 
        }
    }
    return NULL;
}

void exit_program() {
    printf("\nExiting ProcX... Temizlik yapiliyor.\n");
    cleanup_ipc();
    printf("Goodbye!\n");
}