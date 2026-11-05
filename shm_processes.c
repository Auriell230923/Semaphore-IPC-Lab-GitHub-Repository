#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

typedef struct {
    int BankAccount;
    sem_t mutex;
} SharedData;

SharedData *shared;
volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

void random_sleep(int max) {
    sleep((unsigned int)(rand() % (max + 1)));
}

void dad_process() {
    int localBalance = 0;

    srand((unsigned int)(time(NULL) ^ getpid()));

    while (running) {
        random_sleep(5);

        if (!running)
            break;

        printf("Dear Old Dad: Attempting to Check Balance\n");

        int choice = rand();

        if (choice % 2 == 0) {
            if (localBalance < 100) {

                sem_wait(&shared->mutex);

                localBalance = shared->BankAccount;

                int amount = (rand() % 100) + 1;
                int depositChoice = rand();

                if (depositChoice % 2 == 0) {
                    localBalance += amount;

                    printf("Dear old Dad: Deposits $%d / Balance = $%d\n",
                           amount, localBalance);

                    shared->BankAccount = localBalance;
                } else {
                    printf("Dear old Dad: Doesn't have any money to give\n");
                }

                sem_post(&shared->mutex);

            } else {
                printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n",
                       localBalance);
            }
        } else {
            printf("Dear Old Dad: Last Checking Balance = $%d\n",
                   localBalance);
        }

        fflush(stdout);
    }
}

void mom_process() {
    int localBalance = 0;

    srand((unsigned int)(time(NULL) ^ getpid()));

    while (running) {
        random_sleep(10);

        if (!running)
            break;

        printf("Loveable Mom: Attempting to Check Balance\n");

        if (localBalance <= 100) {

            sem_wait(&shared->mutex);

            localBalance = shared->BankAccount;

            int amount = (rand() % 125) + 1;

            localBalance += amount;

            printf("Lovable Mom: Deposits $%d / Balance = $%d\n",
                   amount, localBalance);

            shared->BankAccount = localBalance;

            sem_post(&shared->mutex);
        }

        fflush(stdout);
    }
}

void student_process(int studentID) {
    int localBalance = 0;

    srand((unsigned int)(time(NULL) ^ getpid()));

    while (running) {
        random_sleep(5);

        if (!running)
            break;

        printf("Poor Student: Attempting to Check Balance\n");

        int choice = rand();

        if (choice % 2 == 0) {

            sem_wait(&shared->mutex);

            localBalance = shared->BankAccount;

            int need = (rand() % 50) + 1;

            printf("Poor Student needs $%d\n", need);

            if (need <= localBalance) {
                localBalance -= need;

                printf("Poor Student: Withdraws $%d / Balance = $%d\n",
                       need, localBalance);
            } else {
                printf("Poor Student: Not Enough Cash ($%d)\n",
                       localBalance);
            }

            shared->BankAccount = localBalance;

            sem_post(&shared->mutex);

        } else {
            printf("Poor Student: Last Checking Balance = $%d\n",
                   localBalance);
        }

        fflush(stdout);
    }

    (void)studentID;
}

int main(int argc, char *argv[]) {
    int numParents = 1;
    int numStudents = 1;

    if (argc >= 2)
        numParents = atoi(argv[1]);

    if (argc >= 3)
        numStudents = atoi(argv[2]);

    if (numParents < 1 || numParents > 2 || numStudents < 1) {
        fprintf(stderr,
                "Usage: %s [1 or 2 parents] [number of students]\n",
                argv[0]);
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    setbuf(stdout, NULL);

    shared = mmap(NULL,
                  sizeof(SharedData),
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS,
                  -1, 0);

    if (shared == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    shared->BankAccount = 0;

    if (sem_init(&shared->mutex, 1, 1) == -1) {
        perror("sem_init failed");
        munmap(shared, sizeof(SharedData));
        return 1;
    }

    int totalProcesses = numParents + numStudents;

    pid_t *children = calloc((size_t)totalProcesses,
                             sizeof(pid_t));

    if (children == NULL) {
        perror("calloc failed");
        sem_destroy(&shared->mutex);
        munmap(shared, sizeof(SharedData));
        return 1;
    }

    int count = 0;

    for (int i = 0; i < numParents; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            goto cleanup;
        }

        if (pid == 0) {
            if (i == 0)
                dad_process();
            else
                mom_process();

            _exit(0);
        }

        children[count++] = pid;
    }

    for (int i = 0; i < numStudents; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            goto cleanup;
        }

        if (pid == 0) {
            student_process(i + 1);
            _exit(0);
        }

        children[count++] = pid;
    }

    while (running)
        pause();

cleanup:
    for (int i = 0; i < count; i++) {
        kill(children[i], SIGTERM);
    }

    for (int i = 0; i < count; i++) {
        waitpid(children[i], NULL, 0);
    }

    sem_destroy(&shared->mutex);
    munmap(shared, sizeof(SharedData));
    free(children);

    return 0;
}
