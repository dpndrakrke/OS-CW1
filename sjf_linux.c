// sjf_linux.c
// Compile: gcc sjf_linux.c -O2 -o sjf_linux
// Run:     ./sjf_linux

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <stdint.h>

#define N 7
#define UNIT_MS 200

typedef struct {
    char pid[6];
    char task[40];
    int at, bt, pr;

    int ct, tat, wt;

    double pct_ms;
    double cpu_ms;

    long vctx;   // voluntary context switches during this job (real OS)
    long ivctx;  // involuntary context switches during this job (real OS)

    int done;
} Proc;

static Proc p[N] = {
    {"P1", "Balance Inquiry",         0, 2, 3, 0,0,0, 0,0, 0,0,0},
    {"P2", "Normal Transfer",         1, 4, 2, 0,0,0, 0,0, 0,0,0},
    {"P3", "Salary Batch",            2, 8, 1, 0,0,0, 0,0, 0,0,0},
    {"P4", "Fraud Check",             3, 3, 1, 0,0,0, 0,0, 0,0,0},
    {"P5", "Audit Logging",           4, 2, 4, 0,0,0, 0,0, 0,0,0},
    {"P6", "International Transfer",  5, 6, 2, 0,0,0, 0,0, 0,0,0},
    {"P7", "OTP Verification",        6, 1, 1, 0,0,0, 0,0, 0,0,0}
};

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static void read_rusage(long *vcsw, long *ivcsw, double *cpu_ms) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);

    if (vcsw)  *vcsw  = ru.ru_nvcsw;
    if (ivcsw) *ivcsw = ru.ru_nivcsw;

    double u = (double)ru.ru_utime.tv_sec * 1000.0 + (double)ru.ru_utime.tv_usec / 1000.0;
    double s = (double)ru.ru_stime.tv_sec * 1000.0 + (double)ru.ru_stime.tv_usec / 1000.0;
    if (cpu_ms) *cpu_ms = u + s;
}

static void busy_work_ms(int ms) {
    double start = now_ms();
    volatile uint64_t x = 0;
    while ((now_ms() - start) < (double)ms) {
        x ^= 0x9e3779b97f4a7c15ULL;
        __asm__ __volatile__("" ::: "memory");
    }
}

// ---- SJF selector (same algorithm logic) ----
static int pick_sjf(int current_time) {
    int idx = -1;
    int best_bt = 0;

    for (int i = 0; i < N; i++) {
        if (!p[i].done && p[i].at <= current_time) {
            if (idx == -1 || p[i].bt < best_bt) {
                idx = i;
                best_bt = p[i].bt;
            } else if (p[i].bt == best_bt) {
                if (p[i].at < p[idx].at) idx = i;
            }
        }
    }
    return idx;
}

static int next_arrival_time(void) {
    int t = -1;
    for (int i = 0; i < N; i++) {
        if (!p[i].done) {
            if (t == -1 || p[i].at < t) t = p[i].at;
        }
    }
    return t;
}

int main(void) {
    int completed = 0;
    int current_time = 0;
    int sim_ctx = 0;
    int first_job = 1;

    double total_tat = 0, total_wt = 0;
    double total_cpu = 0;

    long prog_v0=0, prog_iv0=0, prog_v1=0, prog_iv1=0;
    double prog_cpu0=0, prog_cpu1=0;

    double program_start = now_ms();
    read_rusage(&prog_v0, &prog_iv0, &prog_cpu0);

    printf("===== SJF Scheduling (Linux, Non-Preemptive) =====\n\n");

    printf("Gantt Chart:\n");
    printf("0 ");

    while (completed < N) {
        int idx = pick_sjf(current_time);

        if (idx == -1) {
            int na = next_arrival_time();
            if (na > current_time) {
                printf("| IDLE %d ", na);
                current_time = na;
            }
            continue;
        }

        if (!first_job) sim_ctx++;
        first_job = 0;

        printf("| %s ", p[idx].pid);

        long v_before=0, iv_before=0, v_after=0, iv_after=0;
        double cpu_before=0, cpu_after=0;
        read_rusage(&v_before, &iv_before, &cpu_before);

        double start = now_ms();
        busy_work_ms(p[idx].bt * UNIT_MS);
        double end = now_ms();

        read_rusage(&v_after, &iv_after, &cpu_after);

        p[idx].pct_ms = end - start;
        p[idx].cpu_ms = cpu_after - cpu_before;
        p[idx].vctx   = v_after - v_before;
        p[idx].ivctx  = iv_after - iv_before;

        current_time += p[idx].bt;
        p[idx].ct  = current_time;
        p[idx].tat = p[idx].ct - p[idx].at;
        p[idx].wt  = p[idx].tat - p[idx].bt;

        p[idx].done = 1;
        completed++;

        printf("%d ", current_time);
    }

    printf("|\n\n");

    printf("PID\tAT\tBT\tCT\tTAT\tWT\tPCT(ms)\tCPU(ms)\tVCSW\tIVCSW\n");
    for (int i = 0; i < N; i++) {
        printf("%s\t%d\t%d\t%d\t%d\t%d\t%.2f\t%.2f\t%ld\t%ld\n",
               p[i].pid, p[i].at, p[i].bt,
               p[i].ct, p[i].tat, p[i].wt,
               p[i].pct_ms, p[i].cpu_ms,
               p[i].vctx, p[i].ivctx);

        total_tat += p[i].tat;
        total_wt  += p[i].wt;
        total_cpu += p[i].cpu_ms;
    }

    printf("\nAlgorithm Metrics:\n");
    printf("Average Turnaround Time: %.2f\n", total_tat / N);
    printf("Average Waiting Time: %.2f\n", total_wt / N);
    printf("Simulated Context Switches (job transitions): %d\n", sim_ctx);

    double program_end = now_ms();
    read_rusage(&prog_v1, &prog_iv1, &prog_cpu1);

    printf("\nOperating System Metrics (this simulator process):\n");
    printf("Total Program Completion Time: %.2f ms\n", program_end - program_start);
    printf("Total CPU Execution Time: %.2f ms\n", total_cpu);
    printf("Real Voluntary Context Switches: %ld\n", (prog_v1 - prog_v0));
    printf("Real Involuntary Context Switches: %ld\n", (prog_iv1 - prog_iv0));

    return 0;
}
