#define LOG_TAG "PrintfDeadlockAfterFork"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

int thread_init = 0;
pthread_mutex_t thread_init_mutex;
pthread_cond_t thread_init_cond;

void *thread_routine(void *)
{
    void *ptr;

    pthread_mutex_lock(&thread_init_mutex);
    thread_init++;
    pthread_cond_signal(&thread_init_cond);
    pthread_mutex_unlock(&thread_init_mutex);

    printf("%d:%d, %s is ready to call printf in busy loop\n", getpid(), gettid(), __func__);

    while (1) {
        printf("%d:%d, %s busy dump\n", getpid(), gettid(), __func__);
    }

    return NULL;
}

int main()
{
    pthread_t thread;
    int ret, status;
    pid_t pid;

    printf("%d:%d, printf-deadlock-after-fork starts\n", getpid(), gettid());
    pthread_mutex_init(&thread_init_mutex, NULL);
    pthread_cond_init(&thread_init_cond, NULL);

    ret = pthread_create(&thread, NULL, thread_routine, NULL);
    if (ret < 0) {
        printf("failed to pthread_create, error = %s\n", strerror(errno));
        return 1;
    }

    pthread_mutex_lock(&thread_init_mutex);
    while (thread_init < 1) {
        pthread_cond_wait(&thread_init_cond, &thread_init_mutex); } pthread_mutex_unlock(&thread_init_mutex);
    pthread_mutex_unlock(&thread_init_mutex);

    usleep(5000000);

    pid = fork();
    if (pid == 0) {
        void *ptr;
        char buf[4096];
        sprintf(buf, "%d:%d, child process after fork\n", getpid(), gettid());
        sprintf(buf, "%d:%d, is ready to _exit(1)\n", getpid(), gettid());
        write(1, buf, strlen(buf));
        _exit(1);
    }

    printf("%d:%d, parent process after fork\n", getpid(), gettid());

    pid_t wpid  = waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        printf("%d:%d, child pid %d exited with status %d\n", getpid(), gettid(), wpid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("%d:%d, child pid %d killed by signal %d\n", getpid(), gettid(), wpid, WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        printf("%d:%d, child pid %d stopped by signal %d\n", getpid(), gettid(), wpid, WSTOPSIG(status));
    } else {
        printf("%d:%d, child pid %d state changed\n", getpid(), gettid(), wpid);
    }

    pthread_cond_destroy(&thread_init_cond);
    pthread_mutex_destroy(&thread_init_mutex);
    printf("%d:%d, printf-deadlock-after-fork ends\n", getpid(), gettid());

    return 0;
}
