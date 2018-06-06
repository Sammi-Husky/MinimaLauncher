/*
 *  Copyright (C) 2008 Nuke (wiinuke@gmail.com)
 *
 *  this file is part of GeckoOS for USB Gecko
 *  http://www.usbgecko.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <sys/unistd.h>

#include "apploader.h"
#include "patchcode.h"
#include "memory.h"

u32 hooktype = 0x07;
u8 configbytes[2];

extern void patchhook(u32 address, u32 len);
extern void patchhook2(u32 address, u32 len);
extern void patchhook3(u32 address, u32 len);

extern void multidolhook(u32 address);

const u32 axnextframehooks[4] = {0x3800000E, 0x7FE3FB78, 0xB0050000, 0x38800080};

const u32 multidolhooks[4] = {0x7C0004AC, 0x4C00012C, 0x7FE903A6, 0x4E800420};
const u32 multidolchanhooks[4] = {0x4200FFF4, 0x48000004, 0x38800000, 0x4E800020};

bool dogamehooks(void *addr, u32 len, bool channel)
{
    void *addr_start = addr;
    void *addr_end = addr+len;
    bool hookpatched = false;

    while(addr_start < addr_end)
    {
        if(memcmp(addr_start, axnextframehooks, sizeof(axnextframehooks))==0)
        {
            patchhook((u32)addr_start, len);
            hookpatched = true;
        }
        if(memcmp(addr_start, multidolhooks, sizeof(multidolhooks))==0)
        {
            multidolhook((u32)addr_start+sizeof(multidolhooks)-4);
            hookpatched = true;
        }
        addr_start += 4;
    }
    return hookpatched;
}

s32 BlockIOSReload(void)
{
    /* Open ES Module */
    s32 ESHandle = IOS_Open("/dev/es", 0);
    /* IOS Reload Block */
    static ioctlv block_vector[2] ATTRIBUTE_ALIGN(32);
    static u32 mode ATTRIBUTE_ALIGN(32);
    static u32 ios ATTRIBUTE_ALIGN(32);
    mode = 2;
    block_vector[0].data = &mode;
    block_vector[0].len  = sizeof(u32);
    ios = IOS_GetVersion();
    block_vector[1].data = &ios;
    block_vector[1].len  = sizeof(u32);
    s32 ret = IOS_Ioctlv(ESHandle, 0xA0, 2, 0, block_vector);
    /* Close ES Module */
    IOS_Close(ESHandle);
    return ret;
}

void Https_Patch(void *addr, u32 len)
{
    char *cur = (char *)addr;
    const char *end = (char *)addr + len;

    do
    {
        if (memcmp(cur, "https://", 8) == 0 && cur[8] != 0)
        {
            int len = strlen(cur);
            memmove(cur + 4, cur + 5, len - 5);
            cur[len - 1] = 0;
            DCFlushRange((void *)((unsigned) cur & ~0x1F), ((unsigned) len + 0x3F) & ~0x1F);
            cur += len;
        }
    } while (++cur < end);
}
