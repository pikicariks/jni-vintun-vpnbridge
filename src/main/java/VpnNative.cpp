#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_ 

#include "vpn_VpnNativeBridge.h"

#include <Windows.h>
#include <cstdio>
#include <cstdint>
#include <iphlpapi.h>
#include "wintun.h"

static WINTUN_CREATE_ADAPTER_FUNC* WintunCreateAdapter;
static WINTUN_CLOSE_ADAPTER_FUNC* WintunCloseAdapter;
static WINTUN_OPEN_ADAPTER_FUNC* WintunOpenAdapter;
static WINTUN_GET_ADAPTER_LUID_FUNC* WintunGetAdapterLUID;
static WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC* WintunGetRunningDriverVersion;
static WINTUN_DELETE_DRIVER_FUNC* WintunDeleteDriver;
static WINTUN_SET_LOGGER_FUNC* WintunSetLogger;
static WINTUN_START_SESSION_FUNC* WintunStartSession;
static WINTUN_END_SESSION_FUNC* WintunEndSession;
static WINTUN_GET_READ_WAIT_EVENT_FUNC* WintunGetReadWaitEvent;
static WINTUN_RECEIVE_PACKET_FUNC* WintunReceivePacket;
static WINTUN_RELEASE_RECEIVE_PACKET_FUNC* WintunReleaseReceivePacket;
static WINTUN_ALLOCATE_SEND_PACKET_FUNC* WintunAllocateSendPacket;
static WINTUN_SEND_PACKET_FUNC* WintunSendPacket;

#ifndef WINTUN_API
#define WINTUN_API __stdcall
#endif

typedef WINTUN_ADAPTER_HANDLE(WINTUN_API* WINTUN_CREATE_ADAPTER_FUNC)(
    const WCHAR* Name,
    const WCHAR* TunnelType,
    const GUID* RequestedGUID
    );

typedef void (WINTUN_API* WINTUN_DELETE_ADAPTER_FUNC)(
    WINTUN_ADAPTER_HANDLE Adapter
    );

typedef WINTUN_ADAPTER_HANDLE(WINTUN_API* WINTUN_OPEN_ADAPTER_FUNC)(
    const WCHAR* Name
    );

typedef void (WINTUN_API* WINTUN_CLOSE_ADAPTER_FUNC)(
    WINTUN_ADAPTER_HANDLE Adapter
    );

typedef ULONG64(WINTUN_API* WINTUN_GET_ADAPTER_LUID_FUNC)(
    WINTUN_ADAPTER_HANDLE Adapter,
    NET_LUID* Luid
    );

typedef DWORD(WINTUN_API* WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC)(
    void
    );

typedef void (WINTUN_API* WINTUN_DELETE_DRIVER_FUNC)(
    const WCHAR* PoolName
    );

//typedef void (WINTUN_API* WINTUN_SET_LOGGER_FUNC)(
//    WINTUN_LOGGER_TYPE* Logger
//    );

typedef WINTUN_SESSION_HANDLE(WINTUN_API* WINTUN_START_SESSION_FUNC)(
    WINTUN_ADAPTER_HANDLE Adapter,
    DWORD RingCapacity
    );

typedef void (WINTUN_API* WINTUN_END_SESSION_FUNC)(
    WINTUN_SESSION_HANDLE Session
    );

typedef HANDLE(WINTUN_API* WINTUN_GET_READ_WAIT_EVT_FUNC)(
    WINTUN_SESSION_HANDLE Session
    );

typedef BYTE* (WINTUN_API* WINTUN_RECEIVE_PACKET_FUNC)(
    WINTUN_SESSION_HANDLE Session,
    DWORD* PacketSize
    );

typedef void (WINTUN_API* WINTUN_RELEASE_RECEIVE_PACKET_FUNC)(
    WINTUN_SESSION_HANDLE Session,
    const BYTE* Packet
    );

typedef BYTE* (WINTUN_API* WINTUN_ALLOCATE_SEND_PACKET_FUNC)(
    WINTUN_SESSION_HANDLE Session,
    DWORD PacketSize
    );

typedef void (WINTUN_API* WINTUN_SEND_PACKET_FUNC)(
    WINTUN_SESSION_HANDLE Session,
    const BYTE* Packet
    );

#define WINTUN_MAX_IP_PACKET_SIZE 0xFFFF

static HMODULE g_WintunModule = NULL;
static bool g_WintunLoaded = false;

static BOOL LoadWintunFunctions(void) {
    if (g_WintunLoaded) {
        return TRUE;
    }

    HMODULE Wintun =
        LoadLibraryExW(L"wintun.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!Wintun)
        return false;

#define X(Name) ((*(FARPROC *)&Name = GetProcAddress(Wintun, #Name)) == NULL)
    if (X(WintunCreateAdapter) || X(WintunCloseAdapter) || X(WintunOpenAdapter) || X(WintunGetAdapterLUID) ||
        X(WintunGetRunningDriverVersion) || X(WintunDeleteDriver) || X(WintunSetLogger) || X(WintunStartSession) ||
        X(WintunEndSession) || X(WintunGetReadWaitEvent) || X(WintunReceivePacket) || X(WintunReleaseReceivePacket) ||
        X(WintunAllocateSendPacket) || X(WintunSendPacket))
#undef X
    {
        DWORD LastError = GetLastError();
        FreeLibrary(Wintun);
        SetLastError(LastError);
        return false;
    }

    g_WintunModule = Wintun;
    g_WintunLoaded = true;
    return true;
}

void UnloadWintunFunctions(void) {

    if (!g_WintunLoaded)
        return;

    if (g_WintunModule)
        FreeLibrary(g_WintunModule);

    g_WintunModule = NULL;
    g_WintunLoaded = false;
}

JNIEXPORT jlong JNICALL Java_vpn_vpnNativeBridge_openDevice(JNIEnv* env, jobject thiz, jstring deviceName) {
    if (!LoadWintunFunctions())
        return 0;

    const jchar* device = env->GetStringChars(deviceName, NULL);
    if (device == NULL)
        return 0;

    const WCHAR* wdevice = reinterpret_cast<const wchar_t*>(device);

    WINTUN_ADAPTER_HANDLE adapterHandle = WintunOpenAdapter(wdevice);

    if (adapterHandle == NULL) {
        adapterHandle = WintunCreateAdapter(wdevice, L"tunnel", NULL);

        if (adapterHandle == NULL) {
            DWORD error = GetLastError();
            fprintf(stderr, "Wintun Error: Failed to create new adapter. Last Error: %lu\n", error);
            env->ReleaseStringChars(deviceName, device);
            return 0;
        }
    }
    WINTUN_SESSION_HANDLE session_handle = WintunStartSession(adapterHandle, 2097152 /* 2MB */);

    if (session_handle == NULL)
    {
        WintunCloseAdapter(adapterHandle);
        fprintf(stderr, "Wintun Error: Failed to start session on adapter. Last Error: %lu\n", GetLastError());
        env->ReleaseStringChars(deviceName, device);
        return 0;
    }

    jlong openedHandle = (jlong)session_handle;
    env->ReleaseStringChars(deviceName, device); // JNI Cleanup

    return openedHandle;
    
}
