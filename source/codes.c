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
#include <ogc/ipc.h>
#include <malloc.h>
#include "codes.h"
#include "memory.h"
#include "patchcode.h"
#include "codehandler.h"
#include "codehandleronly.h"
#include "multidol.h"
#include "wdvd.h"

u8 *codelistend = NULL;
void *codelist = NULL;

u32 gameconfsize = 0;
u32 *gameconf = NULL;

u8 *code_buf = NULL;
u32 code_size = 0;

extern const u32 axnextframehooks[4];

void load_gameconfig(const char *path){
    FILE *f = NULL;
    size_t fsize = 0;

    WDVD_ReadDiskId((u8*)Disc_ID);

    f = fopen(path, "rb");
    if(f != NULL)
    {
        fseek(f, 0, SEEK_END);
        fsize = ftell(f);
        rewind(f);
        u8 *gameconfig = (u8*)malloc(fsize);
        fread(gameconfig, fsize, 1, f);
        fclose(f);
        do_gameconfig((char*)Disc_ID, gameconfig, fsize);
        free(gameconfig);
    }
}

void load_gct(const char *path){
    FILE *f = NULL;
    size_t fsize = 0;
    printf("%s\n", path);
    f = fopen(path, "rb");
    if(f != NULL)
    {
        printf("Opened gct\n");
        fseek(f, 0, SEEK_END);
        fsize = ftell(f);
        rewind(f);
        u8 *cheats = (u8*)malloc(fsize);
        fread(cheats, fsize, 1, f);
        fclose(f);
        set_codes((void*)0x800022A8, (u8*)0x80003000, cheats, fsize);
        free(cheats);
    }
}

int do_gameconfig(const char *discid, u8 *tempgameconf, u32 tempgameconfsize)
{
    if (gameconf == NULL)
    {
        gameconf = (u32*) malloc(65536);
        if (gameconf == NULL)
            return -1;
    }
    u32 ret;
    s32 gameidmatch, maxgameidmatch = -1, maxgameidmatch2 = -1;
    u32 i, numnonascii, parsebufpos;
    u32 codeaddr, codeval, codeaddr2, codeval2, codeoffset;
    u32 temp, tempoffset = 0;
    char parsebuffer[18];

    // Remove non-ASCII characters
    numnonascii = 0;
    for (i = 0; i < tempgameconfsize; i++)
    {
        if (tempgameconf[i] < 9 || tempgameconf[i] > 126)
            numnonascii++;
        else
            tempgameconf[i - numnonascii] = tempgameconf[i];
    }
    tempgameconfsize -= numnonascii;

    *(tempgameconf + tempgameconfsize) = 0;
    //gameconf = (tempgameconf + tempgameconfsize) + (4 - (((u32) (tempgameconf + tempgameconfsize)) % 4));

    for (maxgameidmatch = 0; maxgameidmatch <= 6; maxgameidmatch++)
    {
        i = 0;
        while (i < tempgameconfsize)
        {
            maxgameidmatch2 = -1;
            while (maxgameidmatch != maxgameidmatch2)
            {
                while (i != tempgameconfsize && tempgameconf[i] != ':')
                    i++;
                if (i == tempgameconfsize) break;
                while ((tempgameconf[i] != 10 && tempgameconf[i] != 13) && (i != 0))
                    i--;
                if (i != 0) i++;
                parsebufpos = 0;
                gameidmatch = 0;
                while (tempgameconf[i] != ':')
                {
                    if (tempgameconf[i] == '?')
                    {
                        parsebuffer[parsebufpos] = discid[parsebufpos];
                        parsebufpos++;
                        gameidmatch--;
                        i++;
                    }
                    else if (tempgameconf[i] != 0 && tempgameconf[i] != ' ')
                        parsebuffer[parsebufpos++] = tempgameconf[i++];
                    else if (tempgameconf[i] == ' ')
                        break;
                    else i++;
                    if (parsebufpos == 8) break;
                }
                parsebuffer[parsebufpos] = 0;
                if (strncasecmp("DEFAULT", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 7)
                {
                    gameidmatch = 0;
                    goto idmatch;
                }
                if (strncasecmp(discid, parsebuffer, strlen(parsebuffer)) == 0)
                {
                    gameidmatch += strlen(parsebuffer);
                    idmatch: if (gameidmatch > maxgameidmatch2)
                    {
                        maxgameidmatch2 = gameidmatch;
                    }
                }
                while ((i != tempgameconfsize) && (tempgameconf[i] != 10 && tempgameconf[i] != 13))
                    i++;
            }
            while (i != tempgameconfsize && tempgameconf[i] != ':')
            {
                parsebufpos = 0;
                while ((i != tempgameconfsize) && (tempgameconf[i] != 10 && tempgameconf[i] != 13))
                {
                    if (tempgameconf[i] != 0 && tempgameconf[i] != ' ' && tempgameconf[i] != '(' && tempgameconf[i]
                            != ':')
                        parsebuffer[parsebufpos++] = tempgameconf[i++];
                    else if (tempgameconf[i] == ' ' || tempgameconf[i] == '(' || tempgameconf[i] == ':')
                        break;
                    else i++;
                    if (parsebufpos == 17) break;
                }
                parsebuffer[parsebufpos] = 0;
                if (strncasecmp("codeliststart", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer)
                        == 13)
                {
                    sscanf((char *) (tempgameconf + i), " = %x", (unsigned int *) &codelist);
                }
                if (strncasecmp("codelistend", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 11)
                {
                    sscanf((char *) (tempgameconf + i), " = %x", (unsigned int *) &codelistend);
                }
                if (strncasecmp("poke", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 4)
                {
                    ret = sscanf((char *) tempgameconf + i, "( %x , %x", &codeaddr, &codeval);
                    if (ret == 2)
                    {
                        *(gameconf + (gameconfsize / 4)) = 0;
                        gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = 0;
                        gameconfsize += 8;
                        *(gameconf + (gameconfsize / 4)) = codeaddr;
                        gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = codeval;
                        gameconfsize += 4;
                        DCFlushRange((void *) (gameconf + (gameconfsize / 4) - 5), 20);
                    }
                }
                if (strncasecmp("pokeifequal", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer) == 11)
                {
                    ret = sscanf((char *) (tempgameconf + i), "( %x , %x , %x , %x", &codeaddr, &codeval,
                            &codeaddr2, &codeval2);
                    if (ret == 4)
                    {
                        *(gameconf + (gameconfsize / 4)) = 0;
                        gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = codeaddr;
                        gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = codeval;
                        gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = codeaddr2;
                        gameconfsize += 4;
                        *(gameconf + (gameconfsize / 4)) = codeval2;
                        gameconfsize += 4;
                        DCFlushRange((void *) (gameconf + (gameconfsize / 4) - 5), 20);
                    }
                }
                if (strncasecmp("searchandpoke", parsebuffer, strlen(parsebuffer)) == 0 && strlen(parsebuffer)
                        == 13)
                {
                    ret = sscanf((char *) (tempgameconf + i), "( %x%n", &codeval, &tempoffset);
                    if (ret == 1)
                    {
                        gameconfsize += 4;
                        temp = 0;
                        while (ret == 1)
                        {
                            *(gameconf + (gameconfsize / 4)) = codeval;
                            gameconfsize += 4;
                            temp++;
                            i += tempoffset;
                            ret = sscanf((char *) (tempgameconf + i), " %x%n", &codeval, &tempoffset);
                        }
                        *(gameconf + (gameconfsize / 4) - temp - 1) = temp;
                        ret = sscanf((char *) (tempgameconf + i), " , %x , %x , %x , %x", &codeaddr, &codeaddr2,
                                &codeoffset, &codeval2);
                        if (ret == 4)
                        {
                            *(gameconf + (gameconfsize / 4)) = codeaddr;
                            gameconfsize += 4;
                            *(gameconf + (gameconfsize / 4)) = codeaddr2;
                            gameconfsize += 4;
                            *(gameconf + (gameconfsize / 4)) = codeoffset;
                            gameconfsize += 4;
                            *(gameconf + (gameconfsize / 4)) = codeval2;
                            gameconfsize += 4;
                            DCFlushRange((void *) (gameconf + (gameconfsize / 4) - temp - 5), temp * 4 + 20);
                        }
                        else gameconfsize -= temp * 4 + 4;
                    }

                }

                if (tempgameconf[i] != ':')
                {
                    while ((i != tempgameconfsize) && (tempgameconf[i] != 10 && tempgameconf[i] != 13))
                        i++;
                    if (i != tempgameconfsize) i++;
                }
            }
            if (i != tempgameconfsize) while ((tempgameconf[i] != 10 && tempgameconf[i] != 13) && (i != 0))
                i--;
        }
    }
    return 0;
}

void gameconfig_set(u32 *gameconfig, u32 tempgameconfsize)
{
    if(gameconfig == NULL)
        return;
    gameconfsize = tempgameconfsize;
    gameconf = malloc(gameconfsize); //internal copy
    memcpy(gameconf, gameconfig, gameconfsize);
}

void app_pokevalues()
{
    u32 i, *codeaddr, *codeaddr2, *addrfound = NULL;

    if (gameconfsize != 0)
    {
        for (i = 0; i < gameconfsize/4; i++)
        {
            if (*(gameconf + i) == 0)
            {
                if (((u32 *) (*(gameconf + i + 1))) == NULL ||
                    *((u32 *) (*(gameconf + i + 1))) == *(gameconf + i + 2))
                {
                    *((u32 *) (*(gameconf + i + 3))) = *(gameconf + i + 4);
                    DCFlushRange((void *) *(gameconf + i + 3), 4);
                }
                i += 4;
            }
            else
            {
                codeaddr = (u32 *)*(gameconf + i + *(gameconf + i) + 1);
                codeaddr2 = (u32 *)*(gameconf + i + *(gameconf + i) + 2);
                if (codeaddr == 0 && addrfound != NULL)
                    codeaddr = addrfound;
                else if (codeaddr == 0 && codeaddr2 != 0)
                    codeaddr = (u32 *) ((((u32) codeaddr2) >> 28) << 28);
                else if (codeaddr == 0 && codeaddr2 == 0)
                {
                    i += *(gameconf + i) + 4;
                    continue;
                }
                if (codeaddr2 == 0)
                    codeaddr2 = codeaddr + *(gameconf + i);
                addrfound = NULL;
                while (codeaddr <= (codeaddr2 - *(gameconf + i)))
                {
                    if (memcmp(codeaddr, gameconf + i + 1, (*(gameconf + i)) * 4) == 0)
                    {
                        *(codeaddr + ((*(gameconf + i + *(gameconf + i) + 3)) / 4)) = *(gameconf + i + *(gameconf + i) + 4);
                        if (addrfound == NULL) addrfound = codeaddr;
                    }
                    codeaddr++;
                }
                i += *(gameconf + i) + 4;
            }
        }
    }
}

void load_handler()
{
    printf("Ocarina: No Debugger selected.\n");
    memcpy((void*)0x80001800, codehandleronly, codehandleronly_size);
    if(code_size > 0 && code_buf)
    {
        printf("Ocarina: Codes found.\n");
        memcpy((void*)0x80001906, &codelist, 2);
        memcpy((void*)0x8000190A, ((u8*) &codelist) + 2, 2);
    }
    else
    {
        printf("Ocarina: No Codes found.\n");
    }
    DCFlushRange((void*)0x80001800, codehandleronly_size);
    ICInvalidateRange((void*)0x80001800, codehandleronly_size);

    // Load multidol handler
    memcpy((void*)0x80001000, multidol, multidol_size);
    DCFlushRange((void*)0x80001000, multidol_size);
    ICInvalidateRange((void*)0x80001000, multidol_size);

    // Load AXNextFrame Hooks
    memcpy((void*)0x8000119C,axnextframehooks,12);
    memcpy((void*)0x80001198,axnextframehooks+3,4);
    DCFlushRange((void*)0x80001198, 16);
    ICInvalidateRange((void*)0x80001198, 16);
}

void set_codes(void *list, u8 *listend, u8 *cheats, u32 cheatSize)
{
    if(codelist == NULL)
        codelist = list;
    if(codelistend == NULL)
        codelistend = listend;
    if(cheatSize <= 0 || cheats == NULL)
    {
        printf("Ocarina: No codes found\n");
        code_buf = NULL;
        code_size = 0;
        return;
    }
    if (cheatSize > (u32)codelistend - (u32)codelist)
    {
        printf("Ocarina: Too many codes found.\n");
        code_buf = NULL;
        code_size = 0;
        return;
    }
    code_size = cheatSize;
    code_buf = malloc(code_size); //internal copy
    memcpy(code_buf, cheats, code_size);
    printf("Ocarina: Codes found.\n");
}

int do_codes()
{
    load_handler();

    if(codelist)
        memset(codelist, 0, (u32)codelistend - (u32)codelist);

    //Copy the codes
    if(code_size > 0 && code_buf)
    {
        memcpy(codelist, code_buf, code_size);
        DCFlushRange(codelist, (u32)codelistend - (u32)codelist);
        free(code_buf);
        code_buf = NULL;
    }

    //This needs to be done after loading the .dol into memory
    app_pokevalues();

    // enable codes
    *(vu8*)0x80001807 = 0x01;

    return 1;
}
