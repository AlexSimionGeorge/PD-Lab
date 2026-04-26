// Tema 3 - Serviciu Windows care afiseaza "Hello World!" la initializare.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>

static const char  kServiceName[] = "Tema3Service";
static const char  kDisplayName[] = "Tema 3 - Hello World Service";
static const char  kLogFile[]     = "C:\\Temp\\tema3_hello.txt";

static SERVICE_STATUS_HANDLE g_hStatus    = NULL;
static SERVICE_STATUS        g_status     = {};
static HANDLE                g_hStopEvent = NULL;

// ---------- Scriere mesaj ----------

static void WriteToFile(const char* msg) {
    CreateDirectoryA("C:\\Temp", NULL);
    HANDLE hFile = CreateFileA(kLogFile, FILE_APPEND_DATA, FILE_SHARE_READ,
                               NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteFile(hFile, msg, static_cast<DWORD>(strlen(msg)), &written, NULL);
    CloseHandle(hFile);
}

static void WriteToEventLog(const char* msg) {
    HANDLE hEvt = RegisterEventSourceA(NULL, kServiceName);
    if (!hEvt) return;
    const char* msgs[] = { msg };
    ReportEventA(hEvt, EVENTLOG_INFORMATION_TYPE, 0, 0,
                 NULL, 1, 0, msgs, NULL);
    DeregisterEventSource(hEvt);
}

static void Log(const char* msg) {
    WriteToFile(msg);
    WriteToEventLog(msg);
}

// ---------- SCM: actualizare stare ----------

static void SetStatus(DWORD state, DWORD exitCode = NO_ERROR, DWORD waitHint = 0) {
    g_status.dwCurrentState  = state;
    g_status.dwWin32ExitCode = exitCode;
    g_status.dwWaitHint      = waitHint;
    g_status.dwControlsAccepted =
        (state == SERVICE_START_PENDING) ? 0
                                         : (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
    SetServiceStatus(g_hStatus, &g_status);
}

// ---------- Handler comenzi SCM (stop/shutdown) ----------

static DWORD WINAPI ControlHandler(DWORD ctrl, DWORD, LPVOID, LPVOID) {
    switch (ctrl) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            SetStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
            Log("[Tema3] Serviciu oprit.\r\n");
            SetEvent(g_hStopEvent);
            return NO_ERROR;
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

// ---------- Punct de intrare serviciu (apelat de SCM) ----------

static VOID WINAPI ServiceMain(DWORD, LPSTR*) {
    g_hStatus = RegisterServiceCtrlHandlerExA(kServiceName, ControlHandler, NULL);
    if (!g_hStatus) return;

    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

    SetStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    g_hStopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!g_hStopEvent) {
        SetStatus(SERVICE_STOPPED, GetLastError());
        return;
    }

    // *** INITIALIZARE: afisare "Hello World!" ***
    Log("Hello World!\r\n");
    Log("[Tema3] Serviciu initializat si pornit cu succes.\r\n");

    SetStatus(SERVICE_RUNNING);

    // Asteptam semnalul de oprire de la SCM.
    WaitForSingleObject(g_hStopEvent, INFINITE);
    CloseHandle(g_hStopEvent);

    SetStatus(SERVICE_STOPPED);
}

// ---------- Instalare / Dezinstalare ----------

static void Install() {
    SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        printf("OpenSCManager a esuat (cod: %lu).\n"
               "Rulati cmd.exe ca Administrator si reincercati.\n", GetLastError());
        return;
    }

    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    SC_HANDLE hSvc = CreateServiceA(
        hSCM, kServiceName, kDisplayName,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,          // pornire manuala
        SERVICE_ERROR_NORMAL,
        exePath,
        NULL, NULL, NULL, NULL, NULL);

    if (hSvc) {
        printf("Serviciu instalat cu succes: \"%s\"\n", kDisplayName);
        printf("Porniti-l cu:  sc start %s\n", kServiceName);
        printf("Sau din:       services.msc\n");
        CloseServiceHandle(hSvc);
    } else {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS)
            printf("Serviciul exista deja. Rulati /uninstall mai intai.\n");
        else
            printf("CreateService a esuat (cod: %lu).\n", err);
    }
    CloseServiceHandle(hSCM);
}

static void Uninstall() {
    SC_HANDLE hSCM = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCM) {
        printf("OpenSCManager a esuat (cod: %lu).\n", GetLastError());
        return;
    }
    SC_HANDLE hSvc = OpenServiceA(hSCM, kServiceName, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (!hSvc) {
        printf("Serviciu \"%s\" negasit.\n", kServiceName);
        CloseServiceHandle(hSCM);
        return;
    }

    // Oprire daca ruleaza
    SERVICE_STATUS ss;
    if (ControlService(hSvc, SERVICE_CONTROL_STOP, &ss)) {
        printf("Se opreste serviciul...\n");
        Sleep(1500);
    }

    if (DeleteService(hSvc))
        printf("Serviciu dezinstalat cu succes.\n");
    else
        printf("DeleteService a esuat (cod: %lu).\n", GetLastError());

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
}

// ---------- Main ----------

int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (_stricmp(argv[1], "/install") == 0)   { Install();   return 0; }
        if (_stricmp(argv[1], "/uninstall") == 0) { Uninstall(); return 0; }
        if (_stricmp(argv[1], "/debug") == 0) {
            // Ruleaza logica de initializare fara SCM — util pentru testare rapida.
            printf("=== DEBUG MODE (fara SCM) ===\n");
            printf("Hello World!\n");
            WriteToFile("Hello World!\r\n");
            WriteToFile("[Tema3] Debug run - serviciu initializat.\r\n");
            printf("Mesaj scris in: %s\n", kLogFile);
            return 0;
        }
        printf("Argumente valide: /install  /uninstall  /debug\n");
        return 1;
    }

    // Pornit de SCM — inregistram ServiceMain.
    SERVICE_TABLE_ENTRYA table[] = {
        { const_cast<LPSTR>(kServiceName), ServiceMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcherA(table)) {
        if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            printf("Nu rulati direct acest executabil.\n");
            printf("Utilizati: tema3.exe /install   apoi   sc start %s\n", kServiceName);
            printf("Sau:        tema3.exe /debug     pentru test rapid\n");
        }
    }
    return 0;
}
