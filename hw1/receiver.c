
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
#define shared_memory_size sizeof(char) * 1000
#define SHM_NAME "shared_memory"
#define MQ_NAME "/i_am_queue_queue"//😭😭😭
int done = 0;
struct timespec start, end;
double elapsed;

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr) {
    if (mailbox_ptr->flag == 1) {  // Message passing
        mq_receive(mailbox_ptr->storage.msqid, message_ptr->message, MAX_MSG_SIZE, NULL);
    } else if (mailbox_ptr->flag == 2) {  // Shared memory
        strcpy(message_ptr->message, mailbox_ptr->storage.shm_addr);
    }

    if (message_ptr->message[0] == '1') {
        printf("\033[0;31m\nSender exit!\n\033[0m");
        done = 1;
        return;
    }
    printf("\033[0;33mReceiving Message:\033[0m %s", message_ptr->message);
}

int main(int argc, char* argv[]) {
    sem_t* sem_sender = sem_open("sem_sender", O_CREAT, 0666, 1);
    sem_t* sem_receiver = sem_open("sem_receiver", O_CREAT, 0666, 0);

    mailbox_t mailbox;
    mailbox.flag = atoi(argv[1]);

    if (mailbox.flag == 1) {  // Message passing
        struct mq_attr attr;
        attr.mq_flags = 0;
        attr.mq_maxmsg = 10;
        attr.mq_msgsize = MAX_MSG_SIZE;
        attr.mq_curmsgs = 0;
        mailbox.storage.msqid = mq_open(MQ_NAME, O_CREAT | O_RDONLY, 0644, &attr);
        if (mailbox.storage.msqid == (mqd_t)-1) {
            perror("mq_open");
            exit(1);
        }
    } else if (mailbox.flag == 2) {  // Shared memory
        int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
        void *ptr = mmap(0, shared_memory_size, PROT_READ, MAP_SHARED, shm_fd, 0);
        mailbox.storage.shm_addr = (char*)ptr;
    }

    message_t message;
    elapsed = 0;

    while (!done) {
        sem_wait(sem_receiver);
        clock_gettime(CLOCK_MONOTONIC, &start);
        receive(&message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        elapsed += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        sem_post(sem_sender);
    }

    printf("Total elapsed time: %f seconds\n", elapsed);

    if (mailbox.flag == 1) {
        mq_close(mailbox.storage.msqid);
        mq_unlink(MQ_NAME);
    } 
    munmap(mailbox.storage.shm_addr, shared_memory_size);
    shm_unlink(SHM_NAME);
    sem_close(sem_sender);
    sem_close(sem_receiver);
    sem_unlink("sem_sender");
    sem_unlink("sem_receiver");

    return 0;
}
