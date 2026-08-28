#pragma once
#include <cstdint>
#define EDSCALLBACK
using EdsVoid=void; using EdsBool=int; using EdsError=std::uint32_t; using EdsUInt32=std::uint32_t; using EdsUInt64=std::uint64_t; using EdsInt32=std::int32_t; using EdsObjectEvent=std::uint32_t;
struct Obj{}; using EdsBaseRef=Obj*; using EdsCameraListRef=Obj*; using EdsCameraRef=Obj*; using EdsDirectoryItemRef=Obj*; using EdsStreamRef=Obj*;
struct EdsDeviceInfo{char szDeviceDescription[256]{};};
struct EdsDirectoryItemInfo{EdsUInt64 size{}; char szFileName[256]{};};
struct EdsCapacity{EdsUInt32 numberOfFreeClusters{}; EdsUInt32 bytesPerSector{}; EdsBool reset{};};
struct EdsPropertyDesc{EdsInt32 form{}; EdsInt32 access{}; EdsInt32 numElements{}; EdsInt32 propDesc[128]{};};
static constexpr EdsError EDS_ERR_OK=0, EDS_ERR_NOT_SUPPORTED=7, EDS_ERR_DEVICE_BUSY=0x81, EDS_ERR_OPERATION_REFUSED=0xA005, EDS_ERR_TAKE_PICTURE_AF_NG=0x8D01;
static constexpr EdsObjectEvent kEdsObjectEvent_DirItemRequestTransfer=0x208, kEdsObjectEvent_All=0x200;
static constexpr EdsUInt32 kEdsSaveTo_Host=2, kEdsPropID_SaveTo=0xB, kEdsPropID_ISOSpeed=0x402, kEdsPropID_Tv=0x406;
static constexpr EdsUInt32 kEdsCameraCommand_BulbStart=2, kEdsCameraCommand_BulbEnd=3, kEdsCameraCommand_TakePicture=0, kEdsCameraCommand_PressShutterButton=4;
static constexpr EdsInt32 kEdsCameraCommand_ShutterButton_OFF=0, kEdsCameraCommand_ShutterButton_Completely_NonAF=0x00010003;
static constexpr EdsUInt32 kEdsCameraStatusCommand_UILock=0, kEdsCameraStatusCommand_UIUnLock=1;
static constexpr EdsUInt32 kEdsFileCreateDisposition_CreateAlways=1, kEdsAccess_ReadWrite=2;
using Handler=EdsError(*)(EdsObjectEvent,EdsBaseRef,EdsVoid*); using AddedHandler=EdsError(*)(EdsVoid*);
inline EdsError EdsInitializeSDK(){return 0;} inline EdsError EdsTerminateSDK(){return 0;} inline EdsError EdsGetEvent(){return 0;}
inline EdsError EdsSetCameraAddedHandler(AddedHandler,EdsVoid*){return 0;}
inline EdsError EdsGetCameraList(EdsCameraListRef*o){static Obj x;*o=&x;return 0;} inline EdsError EdsGetChildCount(EdsBaseRef,EdsUInt32*o){*o=0;return 0;}
inline EdsError EdsGetChildAtIndex(EdsBaseRef,EdsInt32,EdsBaseRef*o){*o=nullptr;return 0;} inline EdsError EdsGetDeviceInfo(EdsCameraRef,EdsDeviceInfo*){return 0;}
inline EdsError EdsOpenSession(EdsCameraRef){return 0;} inline EdsError EdsCloseSession(EdsCameraRef){return 0;} inline EdsUInt32 EdsRelease(EdsBaseRef){return 0;}
inline EdsError EdsSetObjectEventHandler(EdsCameraRef,EdsObjectEvent,Handler,EdsVoid*){return 0;} inline EdsError EdsSetPropertyData(EdsBaseRef,EdsUInt32,EdsInt32,EdsUInt32,const EdsVoid*){return 0;}
inline EdsError EdsGetPropertyDesc(EdsBaseRef,EdsUInt32,EdsPropertyDesc*o){o->numElements=1;o->propDesc[0]=0x38;return 0;}
inline EdsError EdsSetCapacity(EdsCameraRef,EdsCapacity){return 0;} inline EdsError EdsSendCommand(EdsCameraRef,EdsUInt32,EdsInt32){return 0;} inline EdsError EdsSendStatusCommand(EdsCameraRef,EdsUInt32,EdsInt32){return 0;}
inline EdsError EdsGetDirectoryItemInfo(EdsDirectoryItemRef,EdsDirectoryItemInfo*){return 0;} inline EdsError EdsCreateMemoryStream(EdsUInt64,EdsStreamRef*o){static Obj x;*o=&x;return 0;}
inline EdsError EdsDownload(EdsDirectoryItemRef,EdsUInt64,EdsStreamRef){return 0;} inline EdsError EdsDownloadComplete(EdsDirectoryItemRef){return 0;} inline EdsError EdsDownloadThumbnail(EdsDirectoryItemRef,EdsStreamRef){return 0;}
inline EdsError EdsGetPointer(EdsStreamRef,EdsVoid**p){*p=nullptr;return 0;} inline EdsError EdsGetLength(EdsStreamRef,EdsUInt64*l){*l=0;return 0;}
inline EdsError EdsCreateFileStream(const char*, EdsUInt32, EdsUInt32, EdsStreamRef*o){static Obj x;*o=&x;return 0;}
