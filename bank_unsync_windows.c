// 
// Windows Native UNSYNCHRONIZED demo (2 operations total)
//
// Build (MSYS2 MinGW): gcc -O2 bank_unsync_story_win.c -o unsync_win.exe
// Build (Visual Studio): cl /O2 bank_unsync_story_win.c

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <windows.h>

#define INITIAL_BALANCE 100000
#define SALARY_AMOUNT    20000
#define WITHDRAW_AMOUNT   3000

static volatile LONG64 balance = INITIAL_BALANCE; // volatile to discourage over-optimizing

static void ctx(const char *from, const char *to) {
    printf("   [Context Switch] %s -> %s\n", from, to);
}

DWORD WINAPI SalaryThread(LPVOID arg) {
    (void)arg;

    LONG64 local;
    printf("[P1 Salary Batch] START\n");
    local = balance;
    printf("[P1 Salary Batch] READ  balance=%lld\n", (long long)local);
    local = local + SALARY_AMOUNT;
    printf("[P1 Salary Batch] CALC  new_balance=%lld  (adds +%d)\n", (long long)local, SALARY_AMOUNT);
    ctx("P1 Salary Batch", "P2 Customer Tx");
    Sleep(200);
    balance = local;
    printf("[P1 Salary Batch] WRITE balance=%lld\n", (long long)balance);

    printf("[P1 Salary Batch] END\n");
    return 0;
}

DWORD WINAPI WithdrawThread(LPVOID arg) {
    (void)arg;

    Sleep(100); 
    LONG64 local;

    printf("[P2 Customer Tx ] START\n");
    local = balance;
    printf("[P2 Customer Tx ] READ  balance=%lld\n", (long long)local);
    local = local - WITHDRAW_AMOUNT;
    printf("[P2 Customer Tx ] CALC  new_balance=%lld  (deducts -%d)\n", (long long)local, WITHDRAW_AMOUNT);
    ctx("P2 Customer Tx", "P1 Salary Batch");
    Sleep(250);
    ctx("P1 Salary Batch", "P2 Customer Tx");
    balance = local;
    printf("[P2 Customer Tx ] WRITE balance=%lld  (OVERWRITES previous update)\n", (long long)balance);

    printf("[P2 Customer Tx ] END\n");
    return 0;
}

int main(void) {
    printf("============================================================\n");
    printf("WINDOWS NATIVE: UNSYNCHRONIZED RACE CONDITION (Story Trace)\n");
    printf("Initial Balance: %d | Salary: +%d | Withdrawal: -%d\n",
           INITIAL_BALANCE, SALARY_AMOUNT, WITHDRAW_AMOUNT);
    printf("Expected (serial): %d\n", INITIAL_BALANCE + SALARY_AMOUNT - WITHDRAW_AMOUNT);
    printf("------------------------------------------------------------\n");

    HANDLE t1 = CreateThread(NULL, 0, SalaryThread, NULL, 0, NULL);
    HANDLE t2 = CreateThread(NULL, 0, WithdrawThread, NULL, 0, NULL);

    WaitForSingleObject(t1, INFINITE);
    WaitForSingleObject(t2, INFINITE);

    CloseHandle(t1);
    CloseHandle(t2);

    printf("------------------------------------------------------------\n");
    printf("FINAL BALANCE (unsync): %lld\n", (long long)balance);
    printf("Result: %s\n",
        ((long long)balance == (INITIAL_BALANCE + SALARY_AMOUNT - WITHDRAW_AMOUNT))
        ? "CONSISTENT (unexpected here)"
        : "INCONSISTENT (lost update confirmed)");
    printf("============================================================\n");
    return 0;
}
