#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "a2_helper.h"
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

pthread_mutex_t mutexP4_t1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexP4_t5 = PTHREAD_MUTEX_INITIALIZER;
int P4T1start;
int P4T5end;
pthread_cond_t cond_P4_t1_start= PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_P4_t5_end= PTHREAD_COND_INITIALIZER;

sem_t semaphoreP2; //that leaves 4 threads in at a moment
int T10in; //0 if T10 didn't enter yet, 1 if T10 is in, 2 if T10 exited
pthread_mutex_t mutexT10in = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_T10_in= PTHREAD_COND_INITIALIZER;
int In4threads=0;
pthread_mutex_t mutexIn4threads = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_in_4threads= PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_T10_out= PTHREAD_COND_INITIALIZER;

sem_t* semaphore_7_1;
sem_t* semaphore_4_3;

void* p7threads(void* arg)
{
    long id =(long)arg;
    if(id==3)
    {   
        sem_wait(semaphore_4_3);
        info(BEGIN, 7, id);
        info(END, 7, id);
        return NULL;
    }
    else if(id==1)
    {
        info(BEGIN, 7, id);
        info(END, 7, id);
        sem_post(semaphore_7_1);
        return NULL;
    }
    else
    {
        info(BEGIN, 7, id);
        info(END, 7, id);
        return NULL;
    } 
}

int process7()
{
    pthread_t threads[5];

    semaphore_7_1=sem_open("sem_7_1",O_RDWR);
    semaphore_4_3=sem_open("sem_4_3",O_RDWR);

    for(long i=1;i<=5;i++)
        pthread_create(&(threads[i-1]),NULL, p7threads, (void*)i);
    for(long i=0;i<5;i++)
        pthread_join(threads[i], NULL); 
    return 0;
}

int process5()
{
    int statusP7;
    pid_t pid7=fork();
    switch(pid7)
    {
        case 0: //we are in P7
            info(BEGIN, 7, 0);
            process7();
            info(END, 7, 0);
            exit(0);
        default: //we are in P5
            waitpid(pid7, &statusP7,0);
            break;
    }
    return 0;
}

void* p2thread10(void* arg)
{
    long id =(long)arg;
    sem_wait(&semaphoreP2);
    info(BEGIN, 2, id);
    pthread_mutex_lock(&mutexT10in);
    T10in=1;
    pthread_cond_broadcast(&cond_T10_in);
    pthread_mutex_unlock(&mutexT10in);

    pthread_mutex_lock(&mutexIn4threads);
    In4threads++;
    if(In4threads==4)
            pthread_cond_broadcast(&cond_in_4threads);
    while(In4threads!=4)
        pthread_cond_wait(&cond_in_4threads, &mutexIn4threads);
    pthread_mutex_unlock(&mutexIn4threads);

    info(END,2, id);

    pthread_mutex_lock(&mutexT10in);
    T10in=2;
    pthread_mutex_unlock(&mutexT10in);
    pthread_cond_broadcast(&cond_T10_out);

    sem_post(&semaphoreP2);
    return NULL;
}

void* p2threads(void* arg)
{
    long id =(long)arg;

    //wait for t10 to enter
    pthread_mutex_lock(&mutexT10in);
    while(T10in==0)//not in yet
        pthread_cond_wait(&cond_T10_in,&mutexT10in);
    pthread_mutex_unlock(&mutexT10in);

    sem_wait(&semaphoreP2);
    info(BEGIN, 2, id);

    int val;
    pthread_mutex_lock(&mutexT10in);
    val=T10in;
    pthread_mutex_unlock(&mutexT10in);
    if(val==1)
    {
        pthread_mutex_lock(&mutexIn4threads);
        In4threads++;
        if(In4threads==4)
            pthread_cond_broadcast(&cond_in_4threads);
        while(In4threads!=4)
            pthread_cond_wait(&cond_in_4threads, &mutexIn4threads);
        pthread_mutex_unlock(&mutexIn4threads);

        pthread_mutex_lock(&mutexT10in);
        while(T10in!=2)
            pthread_cond_wait(&cond_T10_out,&mutexT10in);
        pthread_mutex_unlock(&mutexT10in);
    }

    info(END, 2, id);
    sem_post(&semaphoreP2);
    return NULL;
}

int process2()
{
    int statusP5;
    pid_t pid5=fork();
    switch(pid5)
    {
        case 0: //we are in P5
            info(BEGIN, 5, 0);
            process5();
            info(END, 5, 0);
            exit(0);
        default: //we are in P2
            break;
    }

    T10in=0;
    pthread_t threads[50];
    sem_init(&semaphoreP2, 0, 4);

    for(long i=1;i<=50;i++)
    {
        if(i==10)
            pthread_create(&(threads[i-1]),NULL, p2thread10, (void*)i);
        else 
        pthread_create(&(threads[i-1]),NULL, p2threads, (void*)i);
    }

    for(long i=0;i<50;i++)
        pthread_join(threads[i], NULL); 

    waitpid(pid5, &statusP5,0);
    return 0;
}

void* p4threads(void* arg)
{
    long id =(long)arg;
    switch(id)
    {
        case 5:
            pthread_mutex_lock(&mutexP4_t1);
            while(P4T1start==0)
                pthread_cond_wait(&cond_P4_t1_start, &mutexP4_t1);
            pthread_mutex_unlock(&mutexP4_t1);
            info(BEGIN, 4, id);
            info(END, 4, id);
            pthread_mutex_lock(&mutexP4_t5);
            P4T5end=1;
            pthread_cond_signal(&cond_P4_t5_end);
            pthread_mutex_unlock(&mutexP4_t5);
            return NULL;
        case 1:
            info(BEGIN, 4, id);
            pthread_mutex_lock(&mutexP4_t1);
            P4T1start=1;
            pthread_cond_signal(&cond_P4_t1_start);
            pthread_mutex_unlock(&mutexP4_t1);

            pthread_mutex_lock(&mutexP4_t5);
            while(P4T5end==0)
                pthread_cond_wait(&cond_P4_t5_end, &mutexP4_t5);
            pthread_mutex_unlock(&mutexP4_t5);
            info(END, 4, id);
            return NULL;
        case 3:
            sem_wait(semaphore_7_1);
            info(BEGIN, 4, id);
            info(END, 4, id);
            sem_post(semaphore_4_3);
            return NULL;
        default:
            info(BEGIN, 4, id);
            info(END, 4, id);
            return NULL;
    }

}

int process4()
{
    int statusP6;
    pid_t pid6=fork();
    switch(pid6)
    {
        case 0: //we are in P6
            info(BEGIN, 6, 0);
            info(END, 6, 0);
            exit(0);
        default: //we are in P4
            break;
    }

    pthread_t threads[5];
    P4T1start=0;
    P4T5end=0;

    semaphore_7_1=sem_open("sem_7_1",O_RDWR);
    semaphore_4_3=sem_open("sem_4_3",O_RDWR);

    for(long i=1;i<=5;i++)
        pthread_create(&(threads[i-1]),NULL, p4threads, (void*)i);
    for(long i=0;i<5;i++)
        pthread_join(threads[i], NULL); 

    waitpid(pid6, &statusP6,0);
    return 0;
}

int main(){
    init();

    semaphore_7_1 = sem_open("sem_7_1",O_CREAT, 0770,0);
    semaphore_4_3 = sem_open("sem_4_3",O_CREAT, 0770,0);

    info(BEGIN, 1, 0);
    int statusP3, statusP4, statusP2;
    pid_t pid2, pid3, pid4;
    pid2=fork();
    switch(pid2)
    {
        case 0: //we are in P2
            info(BEGIN, 2, 0);
            process2();
            info(END, 2, 0);
            exit(0);
        default: //we are in P1
            pid3=fork();
            switch(pid3)
            {
                case 0: //we are in P3
                    info(BEGIN, 3, 0);
                    info(END, 3, 0);
                    exit(0);
                default: //we are in P1
                    pid4=fork();
                    switch(pid4)
                    {
                        case 0: //we are in P4
                            info(BEGIN, 4, 0);
                            process4();
                            info(END, 4, 0);
                            exit(0);
                        default://we are in P1
                            waitpid(pid2, &statusP2,0);
                            waitpid(pid3, &statusP3,0);
                            waitpid(pid4, &statusP4,0);
                            info(END, 1, 0);
                            return 0;
                    }
            }
    }
}
