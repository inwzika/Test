#include <stdint.h>
#include <linux/list.h>
#include <linux/wait.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/syscall.h>

#include <litmus.h>

#define MAX_TASKS 10

typedef struct {
    int id;
    int start_time;
    int duration;
    int deadline;
    pthread_t thread;
} Task;

Task schedule[MAX_TASKS];
time_t start_time_global;

// تابع gettid برای گرفتن thread id
pid_t gettid() {
    return syscall(SYS_gettid);
}

void* task_function(void* arg) {
    Task* t = (Task*) arg;

    int wait_time = t->start_time - (int)(time(NULL) - start_time_global);
    if (wait_time > 0)
        sleep(wait_time);

    struct rt_task param;
    memset(&param, 0, sizeof(param));
    param.exec_cost = t->duration * 1000000;  // میکروثانیه
    param.period = (t->deadline - t->start_time) * 1000000;
    param.relative_deadline = param.period;
    param.budget_policy = NO_ENFORCEMENT;

    if (set_rt_task_param(gettid(), &param) != 0) {
        perror("set_rt_task_param");
        pthread_exit(NULL);
    }

    if (task_mode(LITMUS_RT_TASK) != 0) {
        perror("task_mode");
        pthread_exit(NULL);
    }

    if (attach_task_to_reservation(gettid(), 123) != 0) {
        perror("attach_task_to_reservation");
        pthread_exit(NULL);
    }

    time_t actual_start = time(NULL);
    printf("🔵 [T%d] started at %lds (should start at %ds)\n", t->id, actual_start - start_time_global, t->start_time);

    sleep(t->duration);

    time_t finish = time(NULL);
    int elapsed = (int)(finish - start_time_global);
    if (elapsed <= t->deadline) {
        printf("✅ [T%d] finished at %ds (on time)\n", t->id, elapsed);
    } else {
        printf("⚠️ [T%d] finished at %ds (MISSED DEADLINE!)\n", t->id, elapsed);
    }

    task_mode(BACKGROUND_TASK);
    pthread_exit(NULL);
}

int num_tasks = 5;

int main() {
    printf("📋 Starting Parallel Table-driven Scheduler using LitmusRT...\n");

    if (init_litmus() != 0) {
        perror("init_litmus");
        exit(EXIT_FAILURE);
    }

    start_time_global = time(NULL);

    schedule[0] = (Task){.id = 1, .start_time = 1, .duration = 2, .deadline = 5};
    schedule[1] = (Task){.id = 2, .start_time = 2, .duration = 3, .deadline = 7};
    schedule[2] = (Task){.id = 3, .start_time = 4, .duration = 2, .deadline = 8};
    schedule[3] = (Task){.id = 4, .start_time = 6, .duration = 1, .deadline = 9};
    schedule[4] = (Task){.id = 5, .start_time = 8, .duration = 3, .deadline = 12};

    for (int i = 0; i < num_tasks; i++) {
        pthread_create(&schedule[i].thread, NULL, task_function, &schedule[i]);
    }

    for (int i = 0; i < num_tasks; i++) {
        pthread_join(schedule[i].thread, NULL);
    }

    printf("✅ All real-time tasks completed.\n");
    return 0;
}
