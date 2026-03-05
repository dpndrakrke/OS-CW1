#include <stdio.h>
#include <time.h>
#include <sys/resource.h>

#define N 7
#define UNIT_MS 200

typedef struct {
    char pid[6];
    char task[40];
    int at, bt, pr;

    int ct, tat, wt;

    double pct_ms;  // elapsed run time (per task)
    double cpu_ms;  // CPU time used (per task)
} Proc;

static Proc p[N] = {
    {"P1", "Balance Inquiry",         0, 2, 3},
    {"P2", "Normal Transfer",         1, 4, 2},
    {"P3", "Salary Batch",            2, 8, 1},
    {"P4", "Fraud Check",             3, 3, 1},
    {"P5", "Audit Logging",           4, 2, 4},
    {"P6", "International Transfer",  5, 6, 2},
    {"P7", "OTP Verification",        6, 1, 1}
};

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

// CPU time used by *this process* so far (user+sys) in ms
static double read_process_cpu_ms(void) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    double user = (double)ru.ru_utime.tv_sec * 1000.0 + (double)ru.ru_utime.tv_usec / 1000.0;
    double sys  = (double)ru.ru_stime.tv_sec * 1000.0 + (double)ru.ru_stime.tv_usec / 1000.0;
    return user + sys;
}

static void busy_work_ms(int ms) {
    double start = now_ms();
    while ((now_ms() - start) < (double)ms) {
        // burn CPU
        __asm__ __volatile__("" ::: "memory");
    }
}

int main(void) {
    int current_time = 0;
    int sim_ctx = 0;

    double total_tat = 0, total_wt = 0;
    double total_cpu = 0;

    double program_start = now_ms();

    printf("===== FCFS Scheduling (Linux) =====\n\n");

    // ---------------- Gantt Chart ----------------
    printf("Gantt Chart:\n");
    printf("0 ");

    for (int i = 0; i < N; i++) {
        if (current_time < p[i].at) {
            printf("| IDLE ");
            current_time = p[i].at;
            printf("%d ", current_time);
        }

        printf("| %s ", p[i].pid);

        double cpu_before = read_process_cpu_ms();
        double start = now_ms();

        busy_work_ms(p[i].bt * UNIT_MS);

        double end = now_ms();
        double cpu_after = read_process_cpu_ms();

        p[i].pct_ms = end - start;
        p[i].cpu_ms = cpu_after - cpu_before;

        current_time += p[i].bt;

        p[i].ct  = current_time;
        p[i].tat = p[i].ct - p[i].at;
        p[i].wt  = p[i].tat - p[i].bt;

        printf("%d ", current_time);

        if (i < N - 1) sim_ctx++;
    }

    printf("|\n\n");

    // ---------------- Results Table ----------------
    printf("PID\tTask Type\t\t\tAT\tBT\tCT\tTAT\tWT\tPCT(ms)\tCPU(ms)\n");

    for (int i = 0; i < N; i++) {
        printf("%s\t%-24s\t%d\t%d\t%d\t%d\t%d\t%.2f\t%.2f\n",
               p[i].pid,
               p[i].task,
               p[i].at, p[i].bt,
               p[i].ct, p[i].tat, p[i].wt,
               p[i].pct_ms,
               p[i].cpu_ms);

        total_tat += p[i].tat;
        total_wt  += p[i].wt;
        total_cpu += p[i].cpu_ms;
    }

    // ---------------- Algorithm Metrics ----------------
    printf("\nAlgorithm Metrics:\n");
    printf("Average Turnaround Time: %.2f\n", total_tat / N);
    printf("Average Waiting Time: %.2f\n", total_wt / N);
    printf("Simulated Context Switches: %d\n", sim_ctx);

    // ---------------- OS Metrics ----------------
    double program_end = now_ms();
    printf("\nOperating System Metrics:\n");
    printf("Total Program Completion Time: %.2f ms\n", program_end - program_start);
    printf("Total CPU Execution Time: %.2f ms\n", total_cpu);

    return 0;
}
