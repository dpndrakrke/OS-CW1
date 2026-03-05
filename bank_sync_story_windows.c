// bank_sync_story_win.c
// Windows Native SYNCHRONIZED demo using CRITICAL_SECTION
//
// Build (MSYS2 MinGW): gcc -O2 bank_sync_story_win.c -o sync_win.exe
// Build (Visual Studio): cl /O2 bank_sync_story_win.c

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <windows.h>

#define INITIAL_BALANCE 100000
#define SALARY_AMOUNT    20000
#define WITHDRAW_AMOUNT   3000

static LONG64 balance = INITIAL_BALANCE;
static CRITICAL_SECTION cs;

DWORD WINAPI SalaryThread(LPVOID arg) {
    (void)arg;
    printf("[P1 Salary Batch] START\n");
    printf("[P1 Salary Batch] LOCK  request\n");
    EnterCriticalSection(&cs);
    printf("[P1 Salary Batch] LOCK  acquired\n");

    LONG64 before = balance;
    printf("[P1 Salary Batch] READ  balance=%lld\n", (long long)before);
    Sleep(200);
    LONG64 after = before + SALARY_AMOUNT;
    balance = after;
    printf("[P1 Salary Batch] WRITE balance=%lld  (adds +%d)\n", (long long)after, SALARY_AMOUNT);

    LeaveCriticalSection(&cs);
    printf("[P1 Salary Batch] LOCK  released\n");
    printf("[P1 Salary Batch] END\n");
    return 0;
}

DWORD WINAPI WithdrawThread(LPVOID arg) {
    (void)arg;

    Sleep(100); // attempt overlap

    printf("[P2 Customer Tx ] START\n");
    printf("[P2 Customer Tx ] LOCK  request\n");
    EnterCriticalSection(&cs);
    printf("[P2 Customer Tx ] LOCK  acquired\n");

    LONG64 before = balance;
    printf("[P2 Customer Tx ] READ  balance=%lld\n", (long long)before);

    Sleep(200); // simulate work

    LONG64 after = before - WITHDRAW_AMOUNT;
    balance = after;
    printf("[P2 Customer Tx ] WRITE balance=%lld  (deducts -%d)\n", (long long)after, WITHDRAW_AMOUNT);

    LeaveCriticalSection(&cs);
    printf("[P2 Customer Tx ] LOCK  released\n");
    printf("[P2 Customer Tx ] END\n");
    return 0;
}

int main(void) {
    InitializeCriticalSection(&cs);

    printf("============================================================\n");
    printf("WINDOWS NATIVE: SYNCHRONIZED (CRITICAL_SECTION) (Story Trace)\n");
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
    printf("FINAL BALANCE (sync): %lld\n", (long long)balance);
    printf("Result: %s\n",
        ((long long)balance == (INITIAL_BALANCE + SALARY_AMOUNT - WITHDRAW_AMOUNT))
        ? "CONSISTENT (correct)"
        : "INCONSISTENT (should not happen)");
    printf("============================================================\n");

    DeleteCriticalSection(&cs);
    return 0;
}
