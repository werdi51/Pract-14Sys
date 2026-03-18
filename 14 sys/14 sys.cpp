#include <windows.h>
#include <iostream>
#include <vector>
#include <ctime>

#define MAX_CLIENTS 20
#define CLUB_CAPACITY 4

bool useSemaphore = false;

struct ClientRecord {
    DWORD threadId;
    DWORD arriveTick;
    DWORD startTick;
    DWORD endTick;
    BOOL served;
    BOOL timeout;
};

struct ClubState {
    ClientRecord clients[MAX_CLIENTS];
    LONG currentVisitors;
    LONG maxVisitors;
    LONG servedCount;
    LONG timeoutCount;
};

ClubState club = { 0 };
HANDLE hSemaphore = NULL;
CRITICAL_SECTION csConsole;

DWORD WINAPI PC_Client(LPVOID lpParam) {
    int clientId = (int)(uintptr_t)lpParam;

    club.clients[clientId].arriveTick = GetTickCount();
    club.clients[clientId].threadId = GetCurrentThreadId();
    club.clients[clientId].served = FALSE;
    club.clients[clientId].timeout = FALSE;

    DWORD waitResult;

    if (useSemaphore) {
        waitResult = WaitForSingleObject(hSemaphore, 3000);
    }
    else {
        waitResult = WAIT_OBJECT_0;
    }

    if (waitResult == WAIT_OBJECT_0) {
        club.clients[clientId].startTick = GetTickCount();

        EnterCriticalSection(&csConsole);
        club.currentVisitors++;
        if (club.currentVisitors > club.maxVisitors) {
            club.maxVisitors = club.currentVisitors;
        }
        LeaveCriticalSection(&csConsole);

        int workTime = 2000 + rand() % 3001;
        Sleep(workTime);

        club.clients[clientId].endTick = GetTickCount();
        club.clients[clientId].served = TRUE;

        EnterCriticalSection(&csConsole);
        club.currentVisitors--;
        club.servedCount++;
        LeaveCriticalSection(&csConsole);

        if (useSemaphore) {
            ReleaseSemaphore(hSemaphore, 1, NULL);
        }
    }
    else {
        EnterCriticalSection(&csConsole);
        club.clients[clientId].timeout = TRUE;
        club.timeoutCount++;
        LeaveCriticalSection(&csConsole);
    }

    return 0;
}

DWORD WINAPI Viewer(LPVOID lpParam) {
    HANDLE* clientThreads = (HANDLE*)lpParam;

    while (WaitForMultipleObjects(MAX_CLIENTS, clientThreads, TRUE, 500) == WAIT_TIMEOUT) {
        EnterCriticalSection(&csConsole);
        std::cout << "\nпк" << std::endl;
        std::cout << "Занято мест (сейчас): " << club.currentVisitors << std::endl;
        std::cout << "Обслужено: " << club.servedCount << std::endl;
        std::cout << "Таймаут: " << club.timeoutCount << std::endl;
        std::cout << "--------------------" << std::endl;
        LeaveCriticalSection(&csConsole);
    }
    return 0;
}

int main() {
    setlocale(LC_ALL, "Russian");
    srand((unsigned)time(0));

    std::cout << "Выберите режим:\n1. С семафором\n2. Без семафора\n";
    int answer;
    std::cin >> answer;
    useSemaphore = (answer == 1);

    InitializeCriticalSection(&csConsole);

    if (useSemaphore) {
        hSemaphore = CreateSemaphore(NULL, CLUB_CAPACITY, CLUB_CAPACITY, NULL);
        std::cout << "Запуск семафор" << std::endl;
    }
    else {
        std::cout << "Запуск без семафора" << std::endl;
    }

    HANDLE threads[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        threads[i] = CreateThread(NULL, 0, PC_Client, (LPVOID)(uintptr_t)i, 0, NULL);

        if (i < 8) SetThreadPriority(threads[i], THREAD_PRIORITY_NORMAL);
        else if (i < 16) SetThreadPriority(threads[i], THREAD_PRIORITY_BELOW_NORMAL);
        else SetThreadPriority(threads[i], THREAD_PRIORITY_HIGHEST);
    }

    HANDLE hObserver = CreateThread(NULL, 0, Viewer, threads, 0, NULL);
    SetThreadPriority(hObserver, THREAD_PRIORITY_LOWEST);

    WaitForMultipleObjects(MAX_CLIENTS, threads, TRUE, INFINITE);
    WaitForSingleObject(hObserver, INFINITE);

    double totalWait = 0, totalWork = 0;
    std::cout << "\nИтоги";

    if (useSemaphore) {
        std::cout << "Потоки, не севшие за комп:\n";
        bool anyoneTimeout = false;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (club.clients[i].timeout) {
                std::cout << "ID: " << club.clients[i].threadId << std::endl;
                anyoneTimeout = true;
            }
        }
        if (!anyoneTimeout) std::cout << "Все дождались.\n";
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (club.clients[i].served) {
            totalWait += (club.clients[i].startTick - club.clients[i].arriveTick);
            totalWork += (club.clients[i].endTick - club.clients[i].startTick);
        }
    }

    if (club.servedCount > 0) {
        std::cout << "\nСреднее время ожидания: " << totalWait / club.servedCount << " мс" << std::endl;
        std::cout << "Среднее время обслуживания: " << totalWork / club.servedCount << " мс" << std::endl;
    }
    std::cout << "Макс. число одновременно занятых мест: " << club.maxVisitors << std::endl;

    DeleteCriticalSection(&csConsole);
    if (useSemaphore) CloseHandle(hSemaphore);
    for (int i = 0; i < MAX_CLIENTS; i++) CloseHandle(threads[i]);
    CloseHandle(hObserver);

    return 0;
}