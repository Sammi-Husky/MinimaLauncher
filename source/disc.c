#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <unistd.h>
#include <malloc.h>
#include <ogc/lwp_watchdog.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>

#include "apploader.h"
#include "codes.h"
#include "disc.h"
#include "memory.h"
#include "defines.h"
#include "types.h"
#include "wdvd.h"
#include "patchcode.h"

struct discHdr wii_hdr ATTRIBUTE_ALIGN(32);
struct gc_discHdr gc_hdr ATTRIBUTE_ALIGN(32);

/* Boot Variables */
u32 GameIOS = 0;
u32 vmode_reg = 0;
GXRModeObj *vmode = NULL;

u32 AppEntrypoint = 0;

extern void __exception_closeall();
extern void __exception_setreload(int t);

/* Constants */
#define PART_INFO_OFFSET    0x10000

s32 Disc_Open()
{
    /* Reset drive */
    s32 ret = WDVD_Reset();
    if(ret < 0)
        return ret;

    /* Read disc ID */
    ret = WDVD_ReadDiskId((u8*)Disc_ID);
    return ret;
}

void Disc_Boot(){
    /* Setup Low Memory */
    Disc_SetLowMemPre();

    /* Get Disc Status */
    WDVD_Init();
    u32 disc_check = 0;
    WDVD_GetCoverStatus(&disc_check);
    if(disc_check & 0x2)
    {
        /* Open up Disc */
        Disc_Open();
        if(Disc_IsWii() == 0)
        {

            /* read in cheats */
            WDVD_ReadDiskId((u8*)Disc_ID);
            const DISC_INTERFACE *sd = &__io_wiisd;

            sd->startup();
            fatMountSimple("sd",sd);

            load_gameconfig("sd:/gameconfig.txt");
            char gamepath[22];
            sprintf(gamepath, "sd:/codes/%.6s.gct", (char*)Disc_ID);
            load_gct(gamepath);


            fatUnmount("sd");
            sd->shutdown();

            /* Find our Partition */
            u32 offset = 0;
            Disc_FindPartition(&offset);
            WDVD_OpenPartition(offset, &GameIOS);
            printf("Using IOS: %i\n", GameIOS);
            WDVD_Close();

            IOS_ReloadIOS(58); //for SDHC support

            /* Re-Init after IOS Reload */
            WDVD_Init();
            WDVD_ReadDiskId((u8*)Disc_ID);
            WDVD_OpenPartition(offset, &GameIOS);

            /* Run Apploader */
            AppEntrypoint = Apploader_Run();
            printf("Entrypoint: %08x\n", AppEntrypoint);
            WDVD_Close();

            /* Setup Low Memory */
            Disc_SetLowMem(GameIOS);

            /* Set an appropriate video mode */
            vmode = Disc_SelectVMode(&vmode_reg);
            Disc_SetVMode(vmode, vmode_reg);

            /* Set time */
            Disc_SetTime();

            /* Shutdown IOS subsystems */
            u32 level = IRQ_Disable();
            __IOS_ShutdownSubsystems();
            __exception_closeall();

            /* Originally from tueidj - taken from NeoGamma (thx) */
            *(vu32*)0xCC003024 = 1;

            /* Boot no hook*/
            if(hooktype == 0)
            {
                asm volatile (
                    "lis %r3, AppEntrypoint@h\n"
                    "ori %r3, %r3, AppEntrypoint@l\n"
                    "lwz %r3, 0(%r3)\n"
                    "mtlr %r3\n"
                    "blr\n"
                );
            }
            else
            {
                /* Run codehandler and then return to app entry */
                asm volatile(
                    "lis %r3, AppEntrypoint@h\n"
                    "ori %r3, %r3, AppEntrypoint@l\n"
                    "lwz %r3, 0(%r3)\n"
                    "mtlr %r3\n"
                    "lis %r3, 0x8000\n"
                    "ori %r3, %r3, 0x18A8\n"
                    "mtctr %r3\n"
                    "bctr\n"
                );
            }
            /* Fail */
            IRQ_Restore(level);
        }
    }
    /* Fail, init chan launching */
    WII_Initialize();
    /* goto HBC */
    WII_LaunchTitle(HBC_LULZ);
    WII_LaunchTitle(HBC_108);
    WII_LaunchTitle(HBC_JODI);
    WII_LaunchTitle(HBC_HAXX);
    /* Fail, goto System Menu */
    WII_LaunchTitle(SYSTEM_MENU);
}

void Disc_SetLowMemPre()
{
    /* Setup low memory before Apploader */
    *BI2                = 0x817E5480; // BI2
    *(vu32*)0xCD00643C    = 0x00000000; // 32Mhz on Bus

    /* Clear Disc ID */
    memset((u8*)Disc_ID, 0, 32);

    /* Flush everything */
    DCFlushRange((void*)0x80000000, 0x3f00);
}

void Disc_SetLowMem(u32 IOS)
{
    /* Setup low memory */
    *Sys_Magic            = 0x0D15EA5E; // Standard Boot Code
    *Sys_Version        = 0x00000001; // Version
    *Arena_L            = 0x00000000; // Arena Low
    *Bus_Speed            = 0x0E7BE2C0; // Console Bus Speed
    *CPU_Speed            = 0x2B73A840; // Console CPU Speed
    *Assembler            = 0x38A00040; // Assembler
    *OS_Thread            = 0x80431A80; // Thread Init
    *Dev_Debugger        = 0x81800000; // Dev Debugger Monitor Address
    *Simulated_Mem        = 0x01800000; // Simulated Memory Size
    *GameID_Address        = 0x80000000; // Fix for Sam & Max (WiiPower)

    /* Copy Disc ID */
    memcpy((void*)Online_Check, (void*)Disc_ID, 4);

    /* Error 002 Fix (thanks WiiPower and uLoader) */
    *Current_IOS = (IOS << 16) | 0xffff;
    *Apploader_IOS = (IOS << 16) | 0xffff;

    /* Flush everything */
    DCFlushRange((void*)0x80000000, 0x3f00);
}

/* Thanks Tinyload */
static struct {
    u32 offset;
    u32 type;
} partition_table[32] ATTRIBUTE_ALIGN(32);

static struct {
    u32 count;
    u32 offset;
    u32 pad[6];
} part_table_info ATTRIBUTE_ALIGN(32);

s32 Disc_FindPartition(u32 *outbuf)
{
    u32 offset = 0;
    u32 cnt = 0;

    /* Read partition info */
    s32 ret = WDVD_UnencryptedRead(&part_table_info, sizeof(part_table_info), PART_INFO_OFFSET);
    if(ret < 0)
        return ret;

    /* Read partition table */
    ret = WDVD_UnencryptedRead(&partition_table, sizeof(partition_table), part_table_info.offset);
    if(ret < 0)
        return ret;

    /* Find game partition */
    for(cnt = 0; cnt < part_table_info.count; cnt++)
    {
        /* Game partition */
        if(partition_table[cnt].type == 0)
        {
            offset = partition_table[cnt].offset;
            break;
        }
    }

    /* No game partition found */
    if(offset == 0)
        return -1;
    WDVD_Seek(offset);

    /* Set output buffer */
    *outbuf = offset;
    return 0;
}

void Disc_SetTime()
{
    /* Set proper time */
    settime(secs_to_ticks(time(NULL) - 946684800));
}

GXRModeObj *Disc_SelectVMode(u32 *rmode_reg)
{
    GXRModeObj *rmode = VIDEO_GetPreferredMode(0);

    /* Get video mode configuration */
    bool progressive = (CONF_GetProgressiveScan() > 0) && VIDEO_HaveComponentCable();

    /* Select video mode register */
    switch (CONF_GetVideo())
    {
        case CONF_VIDEO_PAL:
            if (CONF_GetEuRGB60() > 0)
            {
                *rmode_reg = VI_EURGB60;
                rmode = progressive ? &TVNtsc480Prog : &TVEurgb60Hz480IntDf;
            }
            else
                *rmode_reg = VI_PAL;
            break;

        case CONF_VIDEO_MPAL:
            *rmode_reg = VI_MPAL;
            break;

        case CONF_VIDEO_NTSC:
            *rmode_reg = VI_NTSC;
            break;
    }

    const char DiscRegion = ((u8*)Disc_ID)[3];

    /* Select video mode */
    switch(DiscRegion)
    {
        case 'W':
            break; // Don't overwrite wiiware video modes.
        // PAL
        case 'D':
        case 'F':
        case 'P':
        case 'X':
        case 'Y':
            if(CONF_GetVideo() != CONF_VIDEO_PAL)
            {
                *rmode_reg = VI_PAL;
                rmode = progressive ? &TVNtsc480Prog : &TVNtsc480IntDf;
            }
            break;
        // NTSC
        case 'E':
        case 'J':
        default:
            if(CONF_GetVideo() != CONF_VIDEO_NTSC)
            {
                *rmode_reg = VI_NTSC;
                rmode = progressive ? &TVNtsc480Prog : &TVEurgb60Hz480IntDf;
            }
            break;
    }
    return rmode;
}

void Disc_SetVMode(GXRModeObj *rmode, u32 rmode_reg)
{
    /* Set video mode register */
    *Video_Mode = rmode_reg;
    DCFlushRange((void*)Video_Mode, 4);

    /* Set video mode */
    if(rmode != 0)
        VIDEO_Configure(rmode);

    /* Setup video */
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
    else while(VIDEO_GetNextField())
        VIDEO_WaitVSync();

    /* Set black and flush */
    VIDEO_SetBlack(TRUE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();
    else while(VIDEO_GetNextField())
        VIDEO_WaitVSync();
}


s32 Disc_ReadHeader(void *outbuf)
{
    /* Read Wii disc header */
    return WDVD_UnencryptedRead(outbuf, sizeof(struct discHdr), 0);
}

s32 Disc_ReadGCHeader(void *outbuf)
{
    /* Read GC disc header */
    return WDVD_UnencryptedRead(outbuf, sizeof(struct gc_discHdr), 0);
}

s32 Disc_Type(bool gc)
{
    s32 ret = 0;
    u32 check = 0;
    u32 magic = 0;

    if(!gc)
    {
        check = WII_MAGIC;
        ret = Disc_ReadHeader(&wii_hdr);
        magic = wii_hdr.magic;
    }
    else
    {
        check = GC_MAGIC;
        ret = Disc_ReadGCHeader(&gc_hdr);
        if(memcmp(gc_hdr.id, "GCOPDV", 6) == 0)
            magic = GC_MAGIC;
        else
            magic = gc_hdr.magic;
    }

    if (ret < 0)
        return ret;

    /* Check magic word */
    if (magic != check) return -1;

    return 0;
}

s32 Disc_IsWii(void)
{
    return Disc_Type(0);
}

s32 Disc_IsGC(void)
{
    return Disc_Type(1);
}
