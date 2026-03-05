/ 
// UNSYNCHRONIZED story-style demo (Salary +20000 vs Withdrawal -3000)
//
// Build: gcc -O2 -pthread bank_unsync_story.c -o unsync_story
// Run : ./unsync_story

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define INITIAL_BALANCE 100000
#define SALARY_AMOUNT    20000
#define WITHDRAW_AMOUNT   3000

static long long balance = INITIAL_BALANCE;

static void ctx(const char *from, const char *to) {
    printf("   [Context Switch] %s -> %s\n", from, to);
}

void *salary_process(void *arg) {
    (void)arg;

    long long local;

    printf("[P1 Salary Batch] START\n");

    // Step 1: Read
    local = balance;
    printf("[P1 Salary Batch] READ  balance=%lld\n", local);

    // Step 2: Compute (but does not write yet)
    local = local + SALARY_AMOUNT;
    printf("[P1 Salary Batch] CALC  new_balance=%lld  (adds +%d)\n", local, SALARY_AMOUNT);

    // Give CPU to P2 BEFORE writing
    ctx("P1 Salary Batch", "P2 Customer Tx");
    usleep(200000);

    // Step 5: Write (late write)
    balance = local;
    printf("[P1 Salary Batch] WRITE balance=%lld\n", balance);

    printf("[P1 Salary Batch] END\n");
    return NULL;
}

void *withdraw_process(void *arg) {
    (void)arg;

    // Ensure P1 reads+calculates first, then we run
    usleep(100000);

    long long local;

    printf("[P2 Customer Tx ] START\n");

    // Step 3: Read (likely still old balance)
    local = balance;
    printf("[P2 Customer Tx ] READ  balance=%lld\n", local);

    // Step 4: Compute (but does not write yet)
    local = local - WITHDRAW_AMOUNT;
    printf("[P2 Customer Tx ] CALC  new_balance=%lld  (deducts -%d)\n", local, WITHDRAW_AMOUNT);

    // Give CPU back to P1 so P1 writes first
    ctx("P2 Customer Tx", "P1 Salary Batch");
    usleep(250000);

    // Step 6: Write (overwrites P1 result)
    ctx("P1 Salary Batch", "P2 Customer Tx");
    balance = local;
    printf("[P2 Customer Tx ] WRITE balance=%lld  (OVERWRITES previous update)\n", balance);

    printf("[P2 Customer Tx ] END\n");
    return NULL;
}

int main(void) {
    printf("============================================================\n");
    printf("UNSYNCHRONIZED RACE CONDITION (Story Trace)\n");
    printf("Initial Balance: %d | Salary: +%d | Withdrawal: -%d\n",
           INITIAL_BALANCE, SALARY_AMOUNT, WITHDRAW_AMOUNT);
    printf("Expected (serial): %d\n", INITIAL_BALANCE + SALARY_AMOUNT - WITHDRAW_AMOUNT);
    printf("------------------------------------------------------------\n");

    pthread_t p1, p2;
    pthread_create(&p1, NULL, salary_process, NULL);
    pthread_create(&p2, NULL, withdraw_process, NULL);

    pthread_join(p1, NULL);
    pthread_join(p2, NULL);

    printf("------------------------------------------------------------\n");
    printf("FINAL BALANCE (unsync): %lld\n", balance);
    printf("Result: %s\n",
           (balance == (INITIAL_BALANCE + SALARY_AMOUNT - WITHDRAW_AMOUNT))
             ? "CONSISTENT (unexpected here)"
             : "INCONSISTENT (lost update confirmed)");
    printf("============================================================\n");
    return 0;
}
