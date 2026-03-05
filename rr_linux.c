#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <stdint.h>

#define N 7
#define UNIT_MS 200
#define Q 2   // time quantum in "time units"

typedef struct {
    char pid[6];
    char task[40];
    int at, bt, pr;

    int ct, tat, wt;

    int rem;
    int started;
    int done;

    double pct_ms;
    double cpu_ms;

    long vctx;
    long ivctx;
} Proc;

static Proc p[N] = {
    {"P1", "Balance Inquiry",         0, 2, 3, 0,0,0, 0,0,0, 0,0,0,0, 0,0,0},
    {"P2", "Normal Transfer",         1, 4, 2, 0,0,0, 0,0,0, 0,0,0,0, 0,0,0},
    {"P3", "Salary Batch",            2, 8, 1, 0,0,0, 0,0,0, 0,0,0,0, 0,0,0},
    {"P4", "Fraud Check",             3, 3, 1, 0,0,0, 0,0,0, 0,0,0,0, 0,0,0},
    {"P5", "Audit Logging",           4, 2, 4, 0,0,0, 0,0,0, 0,0,0,0, 0,0,0},
    {"P6", "International Transfer",  5, 6, 2, 0,0,0, 0,0,0, 0,0,0,0, 0,0,0},
    {"P7", "OTP Verification",        6, 1, 1, 0,0,0, 0,0,0, 0,0,0,0, 0,0,0}
};

// ---- timing + rusage ----
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

// ---- simple queue of indexes ----
static int q[256];
static int qh = 0, qt = 0;

static int q_empty(void) { return qh == qt; }
static void enqueue(int x) { q[qt++ & 255] = x; }
static int dequeue(void) { return q[qh++ & 255]; }

// Add all arrivals at current_time that are not enqueued yet.
// We use "started" as "seen/enqueued at least once" marker, but arrivals can be re-enqueued later anyway.
static void add_arrivals(int current_time) {
    for (int i = 0; i < N; i++) {
        if (!p[i].done && !p[i].started && p[i].at <= current_time) {
            p[i].started = 1;
            enqueue(i);
        }
    }
}

// Check if any unfinished process exists
static int remaining_any(void) {
    for (int i = 0; i < N; i++) if (!p[i].done) return 1;
    return 0;
}

// Find next arrival time among not started and not done
static int next_arrival_time(void) {
    int t = -1;
    for (int i = 0; i < N; i++) {
        if (!p[i].done && !p[i].started) {
            if (t == -1 || p[i].at < t) t = p[i].at;
        }
    }
    return t;
}

int main(void) {
    // init remaining time
    for (int i = 0; i < N; i++) p[i].rem = p[i].bt;

    int current_time = 0;
    int finished = 0;

    int sim_ctx = 0;      // simulated context switches (PID changes)
    int last_pid = -1;    // track last executed index

    double total_tat = 0, total_wt = 0;
    double total_cpu = 0;

    long prog_v0=0, prog_iv0=0, prog_v1=0, prog_iv1=0;
    double prog_cpu0=0, prog_cpu1=0;

    double program_start = now_ms();
    read_rusage(&prog_v0, &prog_iv0, &prog_cpu0);

    printf("===== Round Robin Scheduling (Linux) =====\n");
    printf("Time Quantum Q = %d\n\n", Q);

    printf("Gantt Chart:\n");
    printf("0 ");

    // start by enqueuing arrivals at time 0
    add_arrivals(current_time);

    while (finished < N) {
        if (q_empty()) {
            // idle until next arrival
            int na = next_arrival_time();
            if (na == -1) break; // should not happen
            if (na > current_time) {
                printf("| IDLE %d ", na);
                current_time = na;
            }
            add_arrivals(current_time);
            continue;
        }

        int idx = dequeue();
        if (p[idx].done) continue; // safety

        // context switch if switching to a different PID
        if (last_pid != -1 && last_pid != idx) sim_ctx++;
        last_pid = idx;

        // slice
        int slice = (p[idx].rem > Q) ? Q : p[idx].rem;

        // real OS metrics before slice
        long v_before=0, iv_before=0, v_after=0, iv_after=0;
        double cpu_before=0, cpu_after=0;
        read_rusage(&v_before, &iv_before, &cpu_before);

        double start = now_ms();
        busy_work_ms(slice * UNIT_MS);
        double end = now_ms();

        read_rusage(&v_after, &iv_after, &cpu_after);

        p[idx].pct_ms += (end - start);
        p[idx].cpu_ms += (cpu_after - cpu_before);
        p[idx].vctx   += (v_after - v_before);
        p[idx].ivctx  += (iv_after - iv_before);

        // update simulated time + remaining
        current_time += slice;
        p[idx].rem -= slice;

        // after advancing time, add newly arrived processes
        add_arrivals(current_time);

        // print slice on Gantt
        printf("| %s %d ", p[idx].pid, current_time);

        if (p[idx].rem > 0) {
            enqueue(idx); // not finished, go back to queue
        } else {
            p[idx].done = 1;
            finished++;

            p[idx].ct  = current_time;
            p[idx].tat = p[idx].ct - p[idx].at;
            p[idx].wt  = p[idx].tat - p[idx].bt;
        }
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
    printf("Simulated Context Switches (PID changes): %d\n", sim_ctx);

    double program_end = now_ms();
    read_rusage(&prog_v1, &prog_iv1, &prog_cpu1);

    printf("\nOperating System Metrics (this simulator process):\n");
    printf("Total Program Completion Time: %.2f ms\n", program_end - program_start);
    printf("Total CPU Execution Time: %.2f ms\n", total_cpu);
    printf("Real Voluntary Context Switches: %ld\n", (prog_v1 - prog_v0));
    printf("Real Involuntary Context Switches: %ld\n", (prog_iv1 - prog_iv0));

    return 0;
}
