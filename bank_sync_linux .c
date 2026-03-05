// 
// SYNCHRONIZED story-style demo (mutex protects the critical section)
//
// Build: gcc -O2 -pthread bank_sync_story.c -o sync_story
// Run : ./sync_story

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define INITIAL_BALANCE 100000
#define SALARY_AMOUNT    20000
#define WITHDRAW_AMOUNT   3000

static long long balance = INITIAL_BALANCE;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

void *salary_process(void *arg) {
    (void)arg;

    printf("[P1 Salary Batch] START\n");
    printf("[P1 Salary Batch] LOCK  request\n");
    pthread_mutex_lock(&mtx);
    printf("[P1 Salary Batch] LOCK  acquired\n");

    long long before = balance;
    printf("[P1 Salary Batch] READ  balance=%lld\n", before);

    usleep(200000); // simulate processing inside critical section

    long long after = before + SALARY_AMOUNT;
    balance = after;
    printf("[P1 Salary Batch] WRITE balance=%lld  (adds +%d)\n", after, SALARY_AMOUNT);

    pthread_mutex_unlock(&mtx);
    printf("[P1 Salary Batch] LOCK  released\n");
    printf("[P1 Salary Batch] END\n");
    return NULL;
}

void *withdraw_process(void *arg) {
    (void)arg;

    usleep(100000); // try to overlap with P1

    printf("[P2 Customer Tx ] START\n");
    printf("[P2 Customer Tx ] LOCK  request\n");
    pthread_mutex_lock(&mtx);
    printf("[P2 Customer Tx ] LOCK  acquired\n");

    long long before = balance;
    printf("[P2 Customer Tx ] READ  balance=%lld\n", before);

    usleep(200000); // simulate processing inside critical section

    long long after = before - WITHDRAW_AMOUNT;
    balance = after;
    printf("[P2 Customer Tx ] WRITE balance=%lld  (deducts -%d)\n", after, WITHDRAW_AMOUNT);

    pthread_mutex_unlock(&mtx);
    printf("[P2 Customer Tx ] LOCK  released\n");
    printf("[P2 Customer Tx ] END\n");
    return NULL;
}

int main(void) {
    printf("============================================================\n");
    printf("SYNCHRONIZED (Mutex) (Story Trace)\n");
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
    printf("FINAL BALANCE (sync): %lld\n", balance);
    printf("Result: %s\n",
           (balance == (INITIAL_BALANCE + SALARY_AMOUNT - WITHDRAW_AMOUNT))
             ? "CONSISTENT (correct)"
             : "INCONSISTENT (should not happen)");
    printf("============================================================\n");
    return 0;
}
