// tools/installer/payload_ids.h - the resource ids of the embedded payload.
// Included by payload.rc (rc.exe) and by sys/resources.cpp, so the two can
// never disagree about what number is which file.
#pragma once
#define IDR_RELEASE_NOTES    110
#define IDR_D3D9             101
#define IDR_SHIM             102
#define IDR_OPENVR           103
#define IDR_INI              104
#define IDR_COLLECT_SUPPORT  105
#define IDR_HOWTO            106
#define IDR_TROUBLESHOOTING  107
#define IDR_CONTROLLER_GUIDE 109
#define IDR_KNOWN_ISSUES     108
// dvr_dlss\ (DLAA/DLSS/FSR helper), embedded only when DVR_SETUP_DLSS is defined
#define IDR_DLSS_HOST        111
#define IDR_DLSS_NGX         112
#define IDR_FFX_LOADER       113
#define IDR_FFX_UPSCALER     114
#define IDR_DLSS_NGX_LICENSE 115
#define IDR_FFX_LICENSE      116
#define IDR_DLSS_NOTICE      117

#define IDR_RESHADE_BRIDGE 118
#define IDR_INSTALL_RESHADE 119
