/****************************************************************************
 * Copyright (C) 2012-2014
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <unistd.h>
#include <malloc.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include <ogc/machine/processor.h>
#include <wiiuse/wpad.h>

#include "menu.h"
#include "utils/video.h"
#include "Utils/FreeTypeGX.h"
#include "utils/filelist.h"

extern "C" {
    extern void __exception_setreload(int t);
}

int main()
{
	__exception_setreload(30);

    InitVideo();
    WPAD_Init();
    PAD_Init();
    InitFreeType((u8*)font_mwerks_ttf, font_mwerks_ttf_size);
    InitGUIThreads();

    draw_menu();
    return 0;
}
