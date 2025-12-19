#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_ 

#include "vpn_VpnNativeBridge.h"

#include <Windows.h>
#include <cstdio>
#include <cstdint>
#include <iphlpapi.h>
#include "wintun.h"

#ifndef WINTUN_API
#define WINTUN_API __stdcall
#endif

typedef WINTUN_ADAPTER_HANDLE(WINTUN_API* WINTUN_CREATE_ADAPTER_PTR)(const WCHAR* Name, const WCHAR* TunnelType, const GUID* RequestedGUID);
typedef void (WINTUN_API* WINTUN_DELETE_ADAPTER_PTR)(WINTUN_ADAPTER_HANDLE Adapter);
typedef WINTUN_ADAPTER_HANDLE(WINTUN_API* WINTUN_OPEN_ADAPTER_PTR)(const WCHAR* Name);
typedef void (WINTUN_API* WINTUN_CLOSE_ADAPTER_PTR)(WINTUN_ADAPTER_HANDLE Adapter);
typedef DWORD(WINTUN_API* WINTUN_GET_ADAPTER_LUID_PTR)(WINTUN_ADAPTER_HANDLE Adapter, NET_LUID* Luid);
typedef DWORD(WINTUN_API* WINTUN_GET_RUNNING_DRIVER_VERSION_PTR)(void);
typedef void (WINTUN_API* WINTUN_DELETE_DRIVER_PTR)(const WCHAR* PoolName);
typedef void (WINTUN_API* WINTUN_SET_LOGGER_PTR)(WINTUN_LOGGER_CALLBACK Logger);
typedef WINTUN_SESSION_HANDLE(WINTUN_API* WINTUN_START_SESSION_PTR)(WINTUN_ADAPTER_HANDLE Adapter, DWORD RingCapacity);
typedef void (WINTUN_API* WINTUN_END_SESSION_PTR)(WINTUN_SESSION_HANDLE Session);
typedef HANDLE(WINTUN_API* WINTUN_GET_READ_WAIT_EVT_PTR)(WINTUN_SESSION_HANDLE Session);
typedef BYTE* (WINTUN_API* WINTUN_RECEIVE_PACKET_PTR)(WINTUN_SESSION_HANDLE Session, DWORD* PacketSize);
typedef void (WINTUN_API* WINTUN_RELEASE_RECEIVE_PACKET_PTR)(WINTUN_SESSION_HANDLE Session, const BYTE* Packet);
typedef BYTE* (WINTUN_API* WINTUN_ALLOCATE_SEND_PACKET_PTR)(WINTUN_SESSION_HANDLE Session, DWORD PacketSize);
typedef void (WINTUN_API* WINTUN_SEND_PACKET_PTR)(WINTUN_SESSION_HANDLE Session, const BYTE* Packet);

static WINTUN_CREATE_ADAPTER_PTR WintunCreateAdapter = NULL;
static WINTUN_DELETE_ADAPTER_PTR WintunDeleteAdapter = NULL;
static WINTUN_OPEN_ADAPTER_PTR WintunOpenAdapter = NULL;
static WINTUN_CLOSE_ADAPTER_PTR WintunCloseAdapter = NULL;
static WINTUN_GET_ADAPTER_LUID_PTR WintunGetAdapterLUID = NULL;
static WINTUN_GET_RUNNING_DRIVER_VERSION_PTR WintunGetRunningDriverVersion = NULL;
static WINTUN_DELETE_DRIVER_PTR WintunDeleteDriver = NULL;
static WINTUN_SET_LOGGER_PTR WintunSetLogger = NULL;
static WINTUN_START_SESSION_PTR WintunStartSession = NULL;
static WINTUN_END_SESSION_PTR WintunEndSession = NULL;
static WINTUN_GET_READ_WAIT_EVT_PTR WintunGetReadWaitEvent = NULL;
static WINTUN_RECEIVE_PACKET_PTR WintunReceivePacket = NULL;
static WINTUN_RELEASE_RECEIVE_PACKET_PTR WintunReleaseReceivePacket = NULL;
static WINTUN_ALLOCATE_SEND_PACKET_PTR WintunAllocateSendPacket = NULL;
static WINTUN_SEND_PACKET_PTR WintunSendPacket = NULL;

#define WINTUN_MAX_IP_PACKET_SIZE 0xFFFF

static HMODULE g_WintunModule = NULL;
static bool g_WintunLoaded = false;

static BOOL LoadWintunFunctions(void) {
    if (g_WintunLoaded) {
        return TRUE;
    }

    HMODULE Wintun =
        LoadLibraryExW(L"wintun.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (!Wintun) {
        Wintun = LoadLibraryW(L".\\native\\wintun.dll");
    }

    if (!Wintun) {
        Wintun = LoadLibraryW(L"native\\wintun.dll");
    }
    
    if (!Wintun) {
        DWORD error = GetLastError();
        fprintf(stderr, "ERROR: Failed to load wintun.dll. Error code: %lu\n", error);
        fprintf(stderr, "Tried: standard paths, .\\native\\wintun.dll, native\\wintun.dll\n");
        fprintf(stderr, "Make sure wintun.dll is accessible from the application directory.\n");
        return false;
    }

#define X(Name) ((*(FARPROC *)&Name = GetProcAddress(Wintun, #Name)) == NULL)
    if (X(WintunCreateAdapter) || X(WintunCloseAdapter) || X(WintunOpenAdapter) || X(WintunGetAdapterLUID) ||
        X(WintunGetRunningDriverVersion) || X(WintunDeleteDriver) || X(WintunSetLogger) || X(WintunStartSession) ||
        X(WintunEndSession) || X(WintunReceivePacket) || X(WintunReleaseReceivePacket) ||
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

extern "C" {

    JNIEXPORT jlong JNICALL Java_vpn_VpnNativeBridge_openDevice(JNIEnv* env, jobject thiz, jstring deviceName) {
        if (!LoadWintunFunctions()) {
            DWORD error = GetLastError();
            fprintf(stderr, "ERROR: Failed to load wintun.dll. Error code: %lu\n", error);
            fprintf(stderr, "Make sure wintun.dll is in the same directory as VpnNative.dll or in System32\n");
            return 0;
        }

        const jchar* device = env->GetStringChars(deviceName, NULL);
        if (device == NULL)
            return 0;

        const WCHAR* wdevice = reinterpret_cast<const wchar_t*>(device);

        WINTUN_ADAPTER_HANDLE adapterHandle = WintunOpenAdapter(wdevice);

        if (adapterHandle == NULL) {
            adapterHandle = WintunCreateAdapter(wdevice, L"tunnel", NULL);

            if (adapterHandle == NULL) {
                DWORD error = GetLastError();
                fprintf(stderr, "Wintun Error: Failed to create new adapter. Error code: %lu\n", error);
                if (error == ERROR_ACCESS_DENIED || error == 5) {
                    fprintf(stderr, "This operation requires Administrator privileges. Please run as Administrator.\n");
                }
                env->ReleaseStringChars(deviceName, device);
                return 0;
            }
        }
        WINTUN_SESSION_HANDLE session_handle = WintunStartSession(adapterHandle, 2097152 /* 2MB */);

        if (session_handle == NULL)
        {
            DWORD error = GetLastError();
            WintunCloseAdapter(adapterHandle);
            fprintf(stderr, "Wintun Error: Failed to start session on adapter. Error code: %lu\n", error);
            if (error == ERROR_ACCESS_DENIED || error == 5) {
                fprintf(stderr, "This operation requires Administrator privileges. Please run as Administrator.\n");
            }
            env->ReleaseStringChars(deviceName, device);
            return 0;
        }

        jlong openedHandle = (jlong)session_handle;
        env->ReleaseStringChars(deviceName, device); // JNI Cleanup

        return openedHandle;

    }

    JNIEXPORT jint JNICALL Java_vpn_VpnNativeBridge_readPacket(JNIEnv* env, jobject thiz, jlong handle, jobject obj) {
        if (obj == NULL) {
            printf("Buffer is NULL\n");
            return -1;
        }

        WINTUN_SESSION_HANDLE sessionHandle = (WINTUN_SESSION_HANDLE)handle;
        void* bufferAddress = env->GetDirectBufferAddress(obj);
        if (!bufferAddress) return -1;

        jlong bufferCapacity = env->GetDirectBufferCapacity(obj);
        DWORD packetSize = 0;
        BYTE* packetData = WintunReceivePacket(sessionHandle, &packetSize);

        if (!packetData) return 0;

        if (packetSize > bufferCapacity) {
            WintunReleaseReceivePacket(sessionHandle, packetData);
            return -1;
        }

        memcpy(bufferAddress, packetData, packetSize);
        WintunReleaseReceivePacket(sessionHandle, packetData);

        return (jint)packetSize;


    }

    JNIEXPORT jint JNICALL Java_vpn_VpnNativeBridge_writePacket(JNIEnv* env, jobject obj, jlong handle, jobject buffer, jint size) {
        if (!g_WintunLoaded || handle == 0) return -1;
        if (size <= 0 || size > WINTUN_MAX_IP_PACKET_SIZE) return -1;

        if (buffer == NULL) {
            fprintf(stderr, "JNI Error: writePacket requires a Direct ByteBuffer.\n");
            return -1;
        }

        WINTUN_SESSION_HANDLE sessionHandle = (WINTUN_SESSION_HANDLE)handle;
        void* bufferAddress = env->GetDirectBufferAddress(buffer);
        if (!bufferAddress) return -1;

        BYTE* packetData = WintunAllocateSendPacket(sessionHandle, (DWORD)size);

        if (!packetData) {
            return 0;
        }

        memcpy(packetData, bufferAddress, (size_t)size);

        WintunSendPacket(sessionHandle, packetData);

        return size;
    }



    JNIEXPORT void JNICALL Java_vpn_VpnNativeBridge_closeDevice(JNIEnv* env, jobject thiz, jlong handle) {
        if (handle == 0)
            return;

        WINTUN_SESSION_HANDLE session_handle = (WINTUN_SESSION_HANDLE)handle;

        if (g_WintunLoaded) {
            WintunEndSession(session_handle);
        }
    }
}
