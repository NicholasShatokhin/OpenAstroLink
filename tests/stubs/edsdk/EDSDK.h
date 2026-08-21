#pragma once
#include <cstdint>
#define EDSCALLBACK
using EdsVoid=void; using EdsBool=int; using EdsError=std::uint32_t; using EdsUInt32=std::uint32_t; using EdsUInt64=std::uint64_t; using EdsInt32=std::int32_t; using EdsObjectEvent=std::uint32_t;
struct Obj{}; using EdsBaseRef=Obj*; using EdsCameraListRef=Obj*; using EdsCameraRef=Obj*; using EdsDirectoryItemRef=Obj*; using EdsStreamRef=Obj*;
struct EdsDeviceInfo{char szDeviceDescription[256]{};};
struct EdsDirectoryItemInfo{EdsUInt64 size{}; char szFileName[256]{};};
struct EdsCapacity{EdsUInt32 numberOfFreeClusters{}; EdsUInt32 bytesPerSector{}; EdsBool reset{};};
static constexpr EdsError EDS_ERR_OK=0;
static constexpr EdsObjectEvent kEdsObjectEvent_DirItemRequestTransfer=0x208, kEdsObjectEvent_All=0x200;
static constexpr EdsUInt32 kEdsSaveTo_Host=2, kEdsPropID_SaveTo=0xB, kEdsCameraCommand_BulbStart=2, kEdsCameraCommand_BulbEnd=3, kEdsCameraCommand_TakePicture=0, kEdsFileCreateDisposition_CreateAlways=1, kEdsAccess_ReadWrite=2;
using Handler=EdsError(*)(EdsObjectEvent,EdsBaseRef,EdsVoid*);
inline EdsError EdsInitializeSDK(){return 0;} inline EdsError EdsTerminateSDK(){return 0;}
inline EdsError EdsGetCameraList(EdsCameraListRef*o){static Obj x;*o=&x;return 0;} inline EdsError EdsGetChildCount(EdsBaseRef,EdsUInt32*o){*o=0;return 0;}
inline EdsError EdsGetChildAtIndex(EdsBaseRef,EdsInt32,EdsBaseRef*o){*o=nullptr;return 0;} inline EdsError EdsGetDeviceInfo(EdsCameraRef,EdsDeviceInfo*){return 0;}
inline EdsError EdsOpenSession(EdsCameraRef){return 0;} inline EdsError EdsCloseSession(EdsCameraRef){return 0;} inline EdsUInt32 EdsRelease(EdsBaseRef){return 0;}
inline EdsError EdsSetObjectEventHandler(EdsCameraRef,EdsObjectEvent,Handler,EdsVoid*){return 0;} inline EdsError EdsSetPropertyData(EdsBaseRef,EdsUInt32,EdsInt32,EdsUInt32,const EdsVoid*){return 0;}
inline EdsError EdsSetCapacity(EdsCameraRef,EdsCapacity){return 0;} inline EdsError EdsSendCommand(EdsCameraRef,EdsUInt32,EdsInt32){return 0;}
inline EdsError EdsGetDirectoryItemInfo(EdsDirectoryItemRef,EdsDirectoryItemInfo*){return 0;} inline EdsError EdsCreateMemoryStream(EdsUInt64,EdsStreamRef*o){static Obj x;*o=&x;return 0;}
inline EdsError EdsDownload(EdsDirectoryItemRef,EdsUInt64,EdsStreamRef){return 0;} inline EdsError EdsDownloadComplete(EdsDirectoryItemRef){return 0;} inline EdsError EdsDownloadThumbnail(EdsDirectoryItemRef,EdsStreamRef){return 0;}
inline EdsError EdsGetPointer(EdsStreamRef,EdsVoid**p){*p=nullptr;return 0;} inline EdsError EdsGetLength(EdsStreamRef,EdsUInt64*l){*l=0;return 0;}

inline EdsError EdsCreateFileStream(const char*, EdsUInt32, EdsUInt32, EdsStreamRef*o){static Obj x;*o=&x;return 0;}
