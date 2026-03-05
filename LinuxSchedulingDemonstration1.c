#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <string.h>
#include <time.h>
#include <sched.h>

#define NUM_TX 5
#define WORK_PER_BT 450000000ULL

typedef struct {
    const char *tx_id;
    int arrival_time;
    int burst_units;
    int nice_value;
} Transaction;

typedef struct {
    char tx_id[8];
    pid_t pid;
    int nice_after;
    int policy;
    double ct;
    double cpu_time;
    double wt;
    int vol_cs;
    int nonvol_cs;
} TxResult;

static double now_monotonic_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static double now_cpu_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void cpu_burst(int units) {
    volatile unsigned long long x = 0;
    unsigned long long total = (unsigned long long)units * WORK_PER_BT;
    for (unsigned long long i = 0; i < total; i++) {
        x += (i ^ (x << 1)) + 7;
    }
}

static void read_cs(int *vol, int *nonvol) {
    FILE *fp = fopen("/proc/self/status", "r");
    char line[256];
    *vol = 0;
    *nonvol = 0;

    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "voluntary_ctxt_switches: %d", vol);
        sscanf(line, "nonvoluntary_ctxt_switches: %d", nonvol);
    }
    fclose(fp);
}

static const char* policy_name(int p) {
    switch (p) {
        case SCHED_OTHER: return "SCHED_OTHER(CFS)";
        case SCHED_FIFO:  return "SCHED_FIFO";
        case SCHED_RR:    return "SCHED_RR";
        default:          return "UNKNOWN";
    }
}

int main(void) {

    Transaction tx[NUM_TX] = {
        {"T1", 0, 8, 19},
        {"T2", 0, 7,  5},
        {"T3", 0, 5,  0},
        {"T4", 0, 3, 15},
        {"T5", 0, 6, 10}
    };

    int pipefd[NUM_TX][2];

    printf("===== Linux Scheduling Demonstration =====\n\n");

    double start = now_monotonic_s();

    for (int i = 0; i < NUM_TX; i++) {

        pipe(pipefd[i]);

        pid_t pid = fork();

        if (pid == 0) {
            close(pipefd[i][0]);

            TxResult r;
            memset(&r, 0, sizeof(r));

            strncpy(r.tx_id, tx[i].tx_id, sizeof(r.tx_id)-1);
            r.pid = getpid();

            r.policy = sched_getscheduler(0);
            setpriority(PRIO_PROCESS, 0, tx[i].nice_value);
            r.nice_after = getpriority(PRIO_PROCESS, 0);

            double cpu_start = now_cpu_s();
            cpu_burst(tx[i].burst_units);
            double cpu_end = now_cpu_s();

            r.cpu_time = cpu_end - cpu_start;
            r.ct = now_monotonic_s() - start;
            r.wt = r.ct - r.cpu_time;

            read_cs(&r.vol_cs, &r.nonvol_cs);

            write(pipefd[i][1], &r, sizeof(r));
            close(pipefd[i][1]);

            _exit(0);
        }

        close(pipefd[i][1]);
    }

    printf("TX   PID    AT  BU  NI  POLICY             CT(s)   TAT(s)  CPU(s)  WT(s)   TotalCS\n");

    double sum_tat = 0;
    double sum_wt = 0;
    double sum_cs = 0;

    for (int i = 0; i < NUM_TX; i++) {

        TxResult r;
        read(pipefd[i][0], &r, sizeof(r));
        close(pipefd[i][0]);
        wait(NULL);

        int at = 0;                  // for this demo (all arrive at 0)
        int bu = tx[i].burst_units;  // burst units from your transaction array
        double tat = r.ct - at;      // formal definition (AT=0 => tat=r.ct)

        printf("%-4s %-6d %-3d %-3d %-3d %-18s %-7.3f %-7.3f %-7.3f %-7.3f %-7d\n",
                r.tx_id,
                r.pid,
                at,
                bu,
                r.nice_after,
                policy_name(r.policy),
                r.ct,
                tat,
                r.cpu_time,
                r.wt,
                r.vol_cs + r.nonvol_cs);


        sum_tat += r.ct;          // AT=0 → TAT = CT
        sum_wt  += r.wt;
        sum_cs  += (r.vol_cs + r.nonvol_cs);
    }

    printf("\n----- Averages -----\n");
    printf("Average Turnaround Time (ATT): %.3f s\n", sum_tat / NUM_TX);
    printf("Average Waiting Time (AWT):  %.3f s\n", sum_wt / NUM_TX);
    printf("Average Total Context Switches (per process): %.1f\n", sum_cs / NUM_TX);

    return 0;
}
