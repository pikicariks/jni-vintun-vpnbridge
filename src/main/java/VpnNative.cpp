#include "vpn_VpnNativeBridge.h"

#include <Windows.h>
#include <cstdio>
#include <cstdint>

#include "wintun.h"

typedef struct WintunFunctions {
	WINTUN_CREATE_ADAPTER_FUNC* WintunCreateAdapter;
	WINTUN_OPEN_ADAPTER_FUNC* WintunOpenAdapter;
	WINTUN_CLOSE_ADAPTER_FUNC* WintunCloseAdapter;
	WINTUN_GET_ADAPTER_LUID_FUNC* WintunGetAdapterLUID;
	WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC* WintunGetRunningDriverVersion;
	WINTUN_SET_LOGGER_FUNC* WintunSetLogger;

	WINTUN_START_SESSION_FUNC* WintunStartSession;
	WINTUN_END_SESSION_FUNC* WintunEndSession;
	WINTUN_GET_READ_WAIT_EVENT_FUNC* WintunGetReadWaitEvent;

	WINTUN_RECEIVE_PACKET_FUNC* WintunReceivePacket;
	WINTUN_RELEASE_RECEIVE_PACKET_FUNC* WintunReleaseReceivePacket;

	WINTUN_SEND_PACKET_FUNC* WintunSendPacket;
	WINTUN_ALLOCATE_SEND_PACKET_FUNC* WintunAllocateSendPacket;

	HMODULE WintunModule;

} WintunFunctions;

#define WINTUN_MAX_IP_PACKET_SIZE 0xFFFF

static WintunFunctions g_wintun;
static bool g_WintunLoaded = false;

#define RESOLVE_FUNC(name) \
    g_wintun.name = (name##_FUNC*)GetProcAddress(g_wintun.WintunModule, #name); \
    if (!g_wintun.name) { \
        fprintf(stderr, "JNI Error: Could not resolve Wintun function: %s\n", #name); \
        FreeLibrary(g_wintun.WintunModule); \
        g_wintun.WintunModule = NULL; \
        return FALSE; \
    }

BOOL LoadWintunFunctions(void) {
	
    if (g_WintunLoaded) {
        return TRUE;
    }

    memset(&g_wintun, 0, sizeof(WintunFunctions));

    g_wintun.WintunModule = LoadLibraryW(L"wintun.dll");

    if (!g_wintun.WintunModule) {
        fprintf(stderr, "JNI Error: Could not load wintun.dll. Last Error: %lu\n", GetLastError());
        return FALSE;
    }

        RESOLVE_FUNC(WintunCreateAdapter)
        RESOLVE_FUNC(WintunOpenAdapter)
        RESOLVE_FUNC(WintunCloseAdapter)
        RESOLVE_FUNC(WintunGetAdapterLUID)
        RESOLVE_FUNC(WintunGetRunningDriverVersion)
        RESOLVE_FUNC(WintunSetLogger)

        RESOLVE_FUNC(WintunStartSession)
        RESOLVE_FUNC(WintunEndSession)
        RESOLVE_FUNC(WintunGetReadWaitEvent)

        RESOLVE_FUNC(WintunReceivePacket)
        RESOLVE_FUNC(WintunReleaseReceivePacket)

        RESOLVE_FUNC(WintunAllocateSendPacket)
        RESOLVE_FUNC(WintunSendPacket)

        g_WintunLoaded = true;
        return TRUE;
}

void UnloadWintunFunctions(void) {
	


}