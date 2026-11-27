// ISAPI fixture for targeted integration tests; never installed.
#include "khttpext.h"
#include <string>
#include <cstring>
#include <algorithm>

DLL_PUBLIC BOOL WINAPI GetExtensionVersion(HSE_VERSION_INFO* info) {
    strcpy(info->lpszExtensionDesc, "API regression fixture");
    return TRUE;
}

static BOOL headers(EXTENSION_CONTROL_BLOCK* ecb, const char* text, const char* status = NULL) {
    return ecb->ServerSupportFunction(ecb->ConnID, HSE_REQ_SEND_RESPONSE_HEADER,
        (void*)status, NULL, (DWORD*)text);
}
static BOOL write_body(EXTENSION_CONTROL_BLOCK* ecb, const std::string& body) {
    DWORD len = (DWORD)body.size();
    return ecb->WriteClient(ecb->ConnID, (void*)body.data(), &len, 0);
}
static bool map(EXTENSION_CONTROL_BLOCK* ecb, const char* path) {
    char data[4096];
    strcpy(data, path);
    DWORD len = sizeof(data);
    if (!ecb->ServerSupportFunction(ecb->ConnID, HSE_REQ_MAP_URL_TO_PATH, data, &len, NULL)) return false;
    std::string mapped(data);
    std::replace(mapped.begin(), mapped.end(), '\\', '/');
    return mapped.find(path) != std::string::npos;
}

DLL_PUBLIC DWORD WINAPI HttpExtensionProc(EXTENSION_CONTROL_BLOCK* ecb) {
    const std::string mode = ecb->lpszQueryString ? ecb->lpszQueryString : "";
    if (mode == "packed") {
        return headers(ecb, "Content-Length: 6\r\n\r\npacked", "200 OK") ? HSE_STATUS_SUCCESS : HSE_STATUS_ERROR;
    }
    if (mode == "split") {
        if (!headers(ecb, "Content-Length: 5\r", "200 OK") || !headers(ecb, "\n") || !headers(ecb, "\r\n")) {
            return HSE_STATUS_ERROR;
        }
        // A second complete header must fail and must not alter the response.
        headers(ecb, "X-Duplicate: bad\r\n\r\n", "500 Bad");
        return write_body(ecb, "split") ? HSE_STATUS_SUCCESS : HSE_STATUS_ERROR;
    }
    const bool after = mode == "map-after";
    if (after && (!headers(ecb, "Cache-Control: no-store\r\n\r\n", "200 OK") || !write_body(ecb, "prefix:"))) {
        return HSE_STATUS_ERROR;
    }
    std::string body;
    bool mapped = map(ecb, "/api/map-target");
    char data[16384];
    while (body.size() < ecb->cbTotalBytes) {
        DWORD len = sizeof(data);
        if (!ecb->ReadClient(ecb->ConnID, data, &len) || len == 0) return HSE_STATUS_ERROR;
        body.append(data, len);
        mapped = map(ecb, "/api/map-second") && mapped;
    }
    if (!mapped) return HSE_STATUS_ERROR;
    if (!after && !headers(ecb, "Cache-Control: no-store\r\n\r\n", "200 OK")) return HSE_STATUS_ERROR;
    return write_body(ecb, body) ? HSE_STATUS_SUCCESS : HSE_STATUS_ERROR;
}
