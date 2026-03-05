
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

#define BUF 256

static void die(const char *msg) { perror(msg); exit(1); }

int main() {
    int p_to_f[2], f_to_p[2], p_to_l[2];
    if (pipe(p_to_f) == -1) die("pipe p_to_f");
    if (pipe(f_to_p) == -1) die("pipe f_to_p");
    if (pipe(p_to_l) == -1) die("pipe p_to_l");

    // ---- Fork Fraud Detection Service ----
    pid_t fraud_pid = fork();
    if (fraud_pid < 0) die("fork fraud");

    if (fraud_pid == 0) {
        close(p_to_f[1]);   
        close(f_to_p[0]);   
        close(p_to_l[0]); close(p_to_l[1]); 

        char txn[BUF];
        read(p_to_f[0], txn, sizeof(txn));
        printf("[Fraud Engine] Received: %s\n", txn);

        int amount = 0;
        sscanf(txn, "TRANSFER %d", &amount);

        const char *decision = (amount > 15000) ? "FLAGGED" : "APPROVED";
        printf("[Fraud Engine] Risk Decision: %s\n", decision);

        write(f_to_p[1], decision, strlen(decision) + 1);

        close(p_to_f[0]);
        close(f_to_p[1]);
        exit(0);
    }

    // ---- Fork Audit Logging Service ----
    pid_t log_pid = fork();
    if (log_pid < 0) die("fork logger");

    if (log_pid == 0) {
        close(p_to_l[1]);
        close(p_to_f[0]); close(p_to_f[1]);
        close(f_to_p[0]); close(f_to_p[1]);
        char logline[BUF];
        read(p_to_l[0], logline, sizeof(logline));
        printf("[Audit Logger] Log Received: %s\n", logline);
        printf("[Audit Logger] Status: STORED\n");

        close(p_to_l[0]);
        exit(0);
    }

    // ---- Parent: Transaction Processor ----
    close(p_to_f[0]); // parent writes to fraud
    close(f_to_p[1]); // parent reads from fraud
    close(p_to_l[0]); // parent writes to logger

    printf("=================================================\n");
    printf("IPC Demo (Linux Pipes): Processor -> Fraud -> Logger\n");
    printf("=================================================\n");

    // Transaction request 
    const char *txn = "TRANSFER 20000 FROM AccountA TO AccountB";
    printf("[Processor] New Transaction: %s\n", txn);

    // Send to fraud
    printf("[Processor] Sending to Fraud Engine...\n");
    write(p_to_f[1], txn, strlen(txn) + 1);

    // Get fraud decision
    char decision[BUF];
    read(f_to_p[0], decision, sizeof(decision));
    printf("[Processor] Fraud Decision Received: %s\n", decision);

    // Decide and log
    char logline[BUF];
    snprintf(logline, sizeof(logline),
             "TXN=AccountA->AccountB AMOUNT=20000 DECISION=%s", decision);

    printf("[Processor] Sending audit log...\n");
    write(p_to_l[1], logline, strlen(logline) + 1);

    // Close pipes
    close(p_to_f[1]);
    close(f_to_p[0]);
    close(p_to_l[1]);

    // Wait for children
    wait(NULL);
    wait(NULL);

    printf("[Processor] Completed.\n");
    printf("=================================================\n");
    return 0;
}
