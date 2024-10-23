

#include "sender.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <mqueue.h>

#define MAX_MSG_SIZE 256
#define MQ_NAME "/i_am_queue_queue"//😭😭😭
#define shared_memory_size sizeof(char) * 1000
#define SHM_NAME "shared_memory"
struct timespec start, end;
double elapsed;
void send(message_t message, mailbox_t* mailbox_ptr) {
    if (mailbox_ptr->flag == 1) {  // Message passing
        mq_send(mailbox_ptr->storage.msqid, message.message, strlen(message.message) + 1, 0);
    } else if (mailbox_ptr->flag == 2) {  // Shared memory
        strcpy(mailbox_ptr->storage.shm_addr, message.message);
    }
    
    if (message.message[0] == '\0') {
        printf("\033[0;31m\nEnd of input file! exit!\n\033[0m");
        return;
    }
    printf("\033[0;36mSending Message:\033[0m %s", message.message);
}

int main(int argc, char* argv[]) {
    elapsed = 0;
    FILE* file = fopen(argv[2], "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    char line[1000];
    mailbox_t mailbox;
    mailbox.flag = atoi(argv[1]);

    sem_t* sem_sender = sem_open("sem_sender", O_CREAT, 0666, 1);
    sem_t* sem_receiver = sem_open("sem_receiver", O_CREAT, 0666, 0);

    if (mailbox.flag == 1) {  // Message passing
        struct mq_attr attr;
        attr.mq_flags = 0;
        attr.mq_maxmsg = 10;
        attr.mq_msgsize = MAX_MSG_SIZE;
        attr.mq_curmsgs = 0;

        mailbox.storage.msqid = mq_open(MQ_NAME, O_CREAT | O_WRONLY, 0644, &attr);
        if (mailbox.storage.msqid == (mqd_t)-1) {
            perror("mq_open");
            exit(1);
        }
    } else if (mailbox.flag == 2) {  // Shared memory
        int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
        ftruncate(shm_fd, shared_memory_size);
        void *ptr = mmap(0, shared_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        mailbox.storage.shm_addr = (char*)ptr;
    }

    message_t message;
    while (fgets(line, sizeof(line), file)) {
        sem_wait(sem_sender);
        strcpy(message.message, line);
        clock_gettime(CLOCK_MONOTONIC, &start);
        send(message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        elapsed += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        sem_post(sem_receiver);
    }
    sem_wait(sem_sender);
    message.message[0] = '\0';
    send(message, &mailbox);
    sem_post(sem_receiver);

    printf("Total elapsed time: %f seconds\n", elapsed);

    if (mailbox.flag == 1) {
        mq_close(mailbox.storage.msqid);
        mq_unlink(MQ_NAME);
    } else if (mailbox.flag == 2) {
        munmap(mailbox.storage.shm_addr, shared_memory_size);
        shm_unlink(SHM_NAME);
    }
    fclose(file);

    return 0;
}
