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

typedef enum {
    ATTACHED = 0,
    DETACHED = 1
} ProcessMode;

typedef enum {
    RUNNING = 0,
    TERMINATED = 1
} ProcessStatus;

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

// IPC İsimleri
#define SHM_NAME "/procx_shm"
#define SEM_NAME "/procx_sem"
#define MQ_NAME  "/procx_mq"

// --- Fonksiyon Prototipleri ---
void run_program();
void list_processes();
void terminate_program();
void exit_program();
void show_menu();

// --- Ana Fonksiyon ---
int main() {
    int choice;

    // Sonsuz döngü
    while (1) {
        show_menu();
        
        printf("Your choice: ");
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // Buffer temizle
            choice = -1; 
        }

        switch (choice) {
            case 1:
                run_program();
                break;
            case 2:
                list_processes();
                break;
            case 3:
                terminate_program();
                break;
            case 0:
                exit_program();
                return 0; 
            default:
                printf("\n[ERROR] Invalid choice! Please try again.\n");
                sleep(1);
        }
    }
    return 0;
}

// --- Yardımcı Fonksiyonlar ---

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

void run_program() {
    printf("\n>>> [1] Run a New Program seçildi.\n");
    printf("Bu özellik 4. Günde kodlanacak.\n");
    sleep(1);
}

void list_processes() {
    printf("\n>>> [2] List Running Programs seçildi.\n");
    printf("Bu özellik 6. Günde kodlanacak.\n");
    sleep(1);
}

void terminate_program() {
    printf("\n>>> [3] Terminate a Program seçildi.\n");
    printf("Bu özellik 7. Günde kodlanacak.\n");
    sleep(1);
}

void exit_program() {
    printf("\nExiting ProcX... Goodbye!\n");
}