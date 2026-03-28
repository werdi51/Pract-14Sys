#include <windows.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <string>

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
int count = 0;

DWORD WINAPI PC_Client(LPVOID lpParam) {
    int clientId = (int)lpParam;

    count++;

    club.clients[clientId].Records.arriveTick = GetTickCount();
    club.clients[clientId].Records.threadId = GetCurrentThreadId();

    club.clients[clientId].Records.served = FALSE;
    club.clients[clientId].Records.timeout = FALSE;

    club.clients[clientId].name = "User " + std::to_string(count);

    DWORD wait;

    if (useSemaphore) {
        wait = WaitForSingleObject(hSemaphore, 3000);
    }
    else {
        wait = WAIT_OBJECT_0;
    }

    if (wait == WAIT_OBJECT_0) {
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
    HANDLE* ClientshTreads = (HANDLE*)lpParam;

    while (WaitForMultipleObjects(MAX_CLIENTS, ClientshTreads, TRUE, 500) == WAIT_TIMEOUT) {

        EnterCriticalSection(&csConsole);
        std::cout << "\nPc" << std::endl;
        std::cout << "Service: " << club.currentVisitors << std::endl;
        std::cout << "Served: " << club.servedCount << std::endl;
        std::cout << "not servered: " << club.timeoutCount << std::endl;
        LeaveCriticalSection(&csConsole);

    }
    return 0;
}

int main() {

    setlocale(0, "");

    srand(0);

    std::wcout << L"choise status:\n1. semaphor\n2. without semaphor\n";
    int answer;
    std::cin >> answer;
    useSemaphore = (answer == 1);

    InitializeCriticalSection(&csConsole);

    if (useSemaphore) {
        hSemaphore = CreateSemaphore(NULL, CLUB_CAPACITY, CLUB_CAPACITY, NULL);
        std::cout << "Semapor == Yes" << std::endl;
    }
    else {
        std::cout << "Semapor == No" << std::endl;
    }

    HANDLE Clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        Clients[i] = CreateThread(NULL, 0, PC_Client, (LPVOID)i, 0, NULL);

        if (i < 8) SetThreadPriority(Clients[i], THREAD_PRIORITY_NORMAL);
        else if (i < 16) SetThreadPriority(Clients[i], THREAD_PRIORITY_BELOW_NORMAL);
        else SetThreadPriority(Clients[i], THREAD_PRIORITY_HIGHEST);
    }

    HANDLE Viever = CreateThread(NULL, 0, Viewer, Clients, 0, NULL);
    SetThreadPriority(Viever, THREAD_PRIORITY_LOWEST);

    WaitForMultipleObjects(MAX_CLIENTS, Clients, TRUE, INFINITE);
    WaitForSingleObject(Viever, INFINITE);

    double totalWait = 0, totalWork = 0;
    std::cout << "\nTotal";

    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (club.clients[i].Records.served) {
            totalWait += (club.clients[i].Records.startTick - club.clients[i].Records.arriveTick);
            totalWork += (club.clients[i].Records.endTick - club.clients[i].Records.startTick);
        }
    }

    if (club.servedCount > 0) {
        std::cout << "\naverage wait time: " << totalWait / club.servedCount << " ms" << std::endl;
        std::cout << "average service time: " << totalWork / club.servedCount << " ms" << std::endl;
    }

    std::cout << "Served Clients" << std::endl;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (!club.clients[i].Records.served)
        {
            std::cout << club.clients[i].name << std::endl;
        }
    
    }

    std::cout << std::endl;

    std::cout << "not Served clients" << std::endl;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (club.clients[i].Records.served)
        {
            std::cout << club.clients[i].name << std::endl;
        }

    }

    std::cout << std::endl;

    std::cout << "Max count working PC's: " << club.maxVisitors << std::endl;

    DeleteCriticalSection(&csConsole);
    if (useSemaphore) CloseHandle(hSemaphore);
    for (int i = 0; i < MAX_CLIENTS; i++) CloseHandle(Clients[i]);
    CloseHandle(Viever);

    return 0;
}
