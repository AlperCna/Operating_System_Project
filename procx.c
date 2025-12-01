#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include <mqueue.h>
#include <time.h>
#include <errno.h>


// --- Veri Yapıları (Data Structures) ---

typedef enum { ATTACHED = 0, DETACHED = 1 } ProcessMode;
typedef enum { RUNNING = 0, TERMINATED = 1 } ProcessStatus;

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

// --- Ana Fonksiyon ---
int main() {
    int choice;

    // 1. ADIM: IPC Mekanizmalarını Başlat (Günün Yıldızı Burası)
    init_ipc();

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

// --- 4. GÜN: Fork ve Exec ile Program Çalıştırma ---
void run_program() {
    char command[256];
    int mode;
    
    // 1. Önceki menü seçiminden kalan "Enter" tuşunu temizle (Buffer temizliği)
    while(getchar() != '\n'); 

    // 2. Kullanıcıdan Komut Al
    printf("\nEnter the command to run (e.g., sleep 10): ");
    // %[^\n] formatı, enter tuşuna basılana kadar boşluklar dahil her şeyi okur
    if (scanf("%[^\n]", command) != 1) return;

    // 3. Modu Al (0: Attached, 1: Detached)
    printf("Choose running mode (0: Attached, 1: Detached): ");
    scanf("%d", &mode);

    // 4. FORK: Süreci kopyala
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed"); // Fork başarısız olursa hata bas
        return;
    }

    if (pid == 0) {
        // =======================================
        // BURASI ÇOCUK SÜREÇ (CHILD PROCESS)
        // =======================================
        
        // Komutu parçala (Parse): "sleep 10" -> ["sleep", "10", NULL]
        // execvp fonksiyonu komutları kelime kelime ayrılmış bir dizi olarak ister.
        char *args[64];
        int i = 0;
        char *token = strtok(command, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL; // Dizinin sonu mutlaka NULL ile bitmeli

        // Detached Mod kontrolü (Yarın buraya setsid eklenecek)
        if (mode == DETACHED) {
            printf("[CHILD] Detached mode selected (setsid will be added in Day 5)\n");
        }

        // Komutu Çalıştır (Exec)
        // Eğer execvp başarılı olursa, programın hafızası yeni komutla değişir
        // ve bu satırdan sonrası ASLA çalışmaz.
        execvp(args[0], args);

        // Eğer kod buraya ulaşıyorsa execvp başarısız olmuş demektir (örn: yanlış komut)
        perror("[ERROR] Exec failed");
        exit(1); // Çocuğu hata koduyla öldür
    } else {
        // =======================================
        // BURASI EBEVEYN SÜREÇ (PARENT PROCESS)
        // =======================================
        printf("[SUCCESS] Process started: PID %d\n", pid);
        
        // Kullanıcı sonucu görsün diye menüye dönmeden önce 1 saniye bekle
        sleep(1);
    }
}
void list_processes() {
    printf("\n>>> [2] List selected.\n");
    sleep(1);
}

void terminate_program() {
    printf("\n>>> [3] Terminate selected.\n");
    sleep(1);
}

void exit_program() {
    printf("\nExiting ProcX... Temizlik yapiliyor.\n");
    cleanup_ipc();
    printf("Goodbye!\n");
}