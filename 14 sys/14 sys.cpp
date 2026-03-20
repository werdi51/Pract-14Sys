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
struct ClientInfo
{
    ClientRecord Records;
    std::string name;
};

struct ClubState {
    ClientInfo clients[MAX_CLIENTS];
    LONG currentVisitors;
    LONG maxVisitors;
    LONG servedCount;
    LONG timeoutCount;
};

ClubState club = { 0 };
HANDLE hSemaphore = NULL;
CRITICAL_SECTION csConsole;

DWORD WINAPI PC_Client(LPVOID lpParam) {
    int clientId = (int)lpParam;

    club.clients[clientId].Records.arriveTick = GetTickCount();
    club.clients[clientId].Records.threadId = GetCurrentThreadId();

    club.clients[clientId].Records.served = FALSE;
    club.clients[clientId].Records.timeout = FALSE;

    DWORD waitResult;

    if (useSemaphore) {
        waitResult = WaitForSingleObject(hSemaphore, 3000);
    }
    else {
        waitResult = WAIT_OBJECT_0;
    }

    if (waitResult == WAIT_OBJECT_0) {
        club.clients[clientId].Records.startTick = GetTickCount();

        EnterCriticalSection(&csConsole);
        club.currentVisitors++;
        if (club.currentVisitors > club.maxVisitors) {
            club.maxVisitors = club.currentVisitors;
        }
        LeaveCriticalSection(&csConsole);

        int workTime = 2000 + rand() % 3000;
        Sleep(workTime);

        club.clients[clientId].Records.endTick = GetTickCount();
        club.clients[clientId].Records.served = TRUE;

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
        club.clients[clientId].Records.timeout = TRUE;
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
        std::cout << "Занято мест : " << club.currentVisitors << std::endl;
        std::cout << "Обслужено: " << club.servedCount << std::endl;
        std::cout << "Не држдались: " << club.timeoutCount << std::endl;
        std::cout << "--------------------" << std::endl;
        LeaveCriticalSection(&csConsole);

    }
    return 0;
}

int main() {
    setlocale(2, "Rus");
    srand(0);

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
        threads[i] = CreateThread(NULL, 0, PC_Client, (LPVOID)i, 0, NULL);

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

    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (club.clients[i].Records.served) {
            totalWait += (club.clients[i].Records.startTick - club.clients[i].Records.arriveTick);
            totalWork += (club.clients[i].Records.endTick - club.clients[i].Records.startTick);
        }
    }

    if (club.servedCount > 0) {
        std::cout << "\nСреднее время ожидания: " << totalWait / club.servedCount << " мс" << std::endl;
        std::cout << "Среднее время обслуживания: " << totalWork / club.servedCount << " мс" << std::endl;
    }
    std::cout << "Макс занятых мест: " << club.maxVisitors << std::endl;

    DeleteCriticalSection(&csConsole);
    if (useSemaphore) CloseHandle(hSemaphore);
    for (int i = 0; i < MAX_CLIENTS; i++) CloseHandle(threads[i]);
    CloseHandle(hObserver);

    return 0;
}
