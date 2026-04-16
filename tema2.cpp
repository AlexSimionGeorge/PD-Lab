// Tema 2 - Listare metaparametri ai tuturor device-urilor conectate la PC
// folosind apelurile sistem Windows (SetupAPI).
// Compilare: cl /EHsc /W4 tema2.cpp setupapi.lib
// Utilizare: tema2.exe           (toate device-urile prezente)
//            tema2.exe > out.txt (redirectionare output)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>  // pentru MAX_DEVICE_ID_LEN
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

struct PropDesc {
    DWORD       id;
    const char* label;
    bool        isMultiSz;
};

static const PropDesc kProps[] = {
    { SPDRP_DEVICEDESC,                  "DeviceDesc",              false },
    { SPDRP_FRIENDLYNAME,                "FriendlyName",            false },
    { SPDRP_CLASS,                       "Class",                   false },
    { SPDRP_CLASSGUID,                   "ClassGuid",               false },
    { SPDRP_MFG,                         "Manufacturer",            false },
    { SPDRP_HARDWAREID,                  "HardwareID",              true  },
    { SPDRP_COMPATIBLEIDS,               "CompatibleIDs",           true  },
    { SPDRP_SERVICE,                     "Service",                 false },
    { SPDRP_DRIVER,                      "Driver",                  false },
    { SPDRP_ENUMERATOR_NAME,             "Enumerator",              false },
    { SPDRP_LOCATION_INFORMATION,        "LocationInfo",            false },
    { SPDRP_PHYSICAL_DEVICE_OBJECT_NAME, "PhysicalDeviceObjectName",false },
    { SPDRP_BUSNUMBER,                   "BusNumber",               false },
    { SPDRP_CAPABILITIES,                "Capabilities",            false },
    { SPDRP_UPPERFILTERS,                "UpperFilters",            true  },
    { SPDRP_LOWERFILTERS,                "LowerFilters",            true  },
    { SPDRP_INSTALL_STATE,               "InstallState",            false },
    { SPDRP_BASE_CONTAINERID,            "ContainerID",             false },
};

static void PrintMultiSzBuffer(const char* data, DWORD size) {
    const char* p = data;
    const char* end = data + size;
    bool first = true;
    while (p < end && *p) {
        if (!first) std::printf("; ");
        std::printf("%s", p);
        first = false;
        p += std::strlen(p) + 1;
    }
    if (first) std::printf("(empty)");
}

static void PrintProperty(HDEVINFO hDevInfo, SP_DEVINFO_DATA* devData, const PropDesc& pd) {
    DWORD needed = 0;
    DWORD type = 0;

    // Prima chemare determina dimensiunea buffer-ului necesar.
    SetupDiGetDeviceRegistryPropertyA(hDevInfo, devData, pd.id, &type, NULL, 0, &needed);
    DWORD err = GetLastError();
    if (err == ERROR_INVALID_DATA) {
        // Proprietatea nu exista pentru acest device; omitem.
        return;
    }
    if (needed == 0 && err != ERROR_INSUFFICIENT_BUFFER) {
        // Nimic de citit sau eroare neasteptata.
        return;
    }

    std::vector<BYTE> buf(needed > 0 ? needed : 1);
    if (!SetupDiGetDeviceRegistryPropertyA(hDevInfo, devData, pd.id, &type,
                                           buf.data(), static_cast<DWORD>(buf.size()),
                                           &needed)) {
        return;
    }

    std::printf("    %-28s : ", pd.label);
    if (pd.isMultiSz || type == REG_MULTI_SZ) {
        PrintMultiSzBuffer(reinterpret_cast<const char*>(buf.data()), needed);
    } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
        std::printf("%s", reinterpret_cast<const char*>(buf.data()));
    } else if (type == REG_DWORD && needed >= sizeof(DWORD)) {
        DWORD v = *reinterpret_cast<const DWORD*>(buf.data());
        std::printf("%lu (0x%08lX)", v, v);
    } else {
        std::printf("(%lu bytes, type=%lu)", needed, type);
    }
    std::printf("\n");
}

static void PrintInstanceId(HDEVINFO hDevInfo, SP_DEVINFO_DATA* devData) {
    char idBuf[MAX_DEVICE_ID_LEN];
    if (SetupDiGetDeviceInstanceIdA(hDevInfo, devData, idBuf,
                                    static_cast<DWORD>(sizeof(idBuf)), NULL)) {
        std::printf("    %-28s : %s\n", "DeviceInstanceID", idBuf);
    }
}

int main() {
    // DIGCF_ALLCLASSES: toate clasele.
    // DIGCF_PRESENT: doar device-urile prezente fizic (nu si cele fantoma).
    HDEVINFO hDevInfo = SetupDiGetClassDevsA(NULL, NULL, NULL,
                                             DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "SetupDiGetClassDevsA a esuat (cod: %lu)\n", GetLastError());
        return 1;
    }

    SP_DEVINFO_DATA devData;
    devData.cbSize = sizeof(SP_DEVINFO_DATA);

    DWORD index = 0;
    DWORD count = 0;
    while (SetupDiEnumDeviceInfo(hDevInfo, index, &devData)) {
        std::printf("====================== Device #%lu ======================\n", index);
        PrintInstanceId(hDevInfo, &devData);
        for (const auto& pd : kProps) {
            PrintProperty(hDevInfo, &devData, pd);
        }
        std::printf("\n");
        ++index;
        ++count;
    }

    DWORD enumErr = GetLastError();
    if (enumErr != ERROR_NO_MORE_ITEMS && count == 0) {
        std::fprintf(stderr, "SetupDiEnumDeviceInfo a esuat (cod: %lu)\n", enumErr);
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return 2;
    }

    std::printf("Total device-uri enumerate: %lu\n", count);
    SetupDiDestroyDeviceInfoList(hDevInfo);
    return 0;
}
