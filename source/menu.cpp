#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gccore.h>
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include <sys/unistd.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/lwp.h>
#include <fat.h>
#include <sys/dir.h>
#include <malloc.h>

#include "menu.h"
#include "libwiigui/gui.h"
#include "utils/filelist.h"

#define THREAD_SLEEP 60

#define BOUNDS_TOP 240
//#define BOUNDS_BOTTOM
//#define BOUNDS_LEFT
#define BOUNDS_RIGHT 375
//#define BOUNDS_BOTTOM

GuiImage *bgImg = NULL;
GuiSound *bgMusic = NULL;
GuiWindow *mainWindow = NULL;

const char *descriptionText = "Some Example Text";
const char *selectedVersion = "Unknown";

u8 currentMenu = MENU_ROOT;
lwp_t guithread = LWP_THREAD_NULL;
bool guiHalt = true;

void draw_menu()
{
    mainWindow = new GuiWindow(screenwidth, screenheight);
    bgImg = new GuiImage(new GuiImageData(background_png));
    mainWindow->Append(bgImg);


    //Draws background and fades in from black
    for(int i = 255; i >= 0; i -= 7)
    {
        mainWindow->Draw();
        Menu_DrawRectangle(0,0,screenwidth, screenheight,(GXColor){0, 0, 0, (u8)i}, 1);
        Menu_Render();
    }

    GuiText descTxt(descriptionText, 18, (GXColor){255, 255, 255, 255});
    GuiWindow descWindow(566, 47);
    descTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
    descTxt.SetWrap(true, 566);
    descWindow.Append(new GuiImage(new GuiImageData(rect_desc_png)));
    descWindow.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
    descWindow.SetPosition(0, -18);

    descWindow.Append(&descTxt);
    mainWindow->Append(&descWindow);
    ResumeGui();

    while (currentMenu != MENU_EXIT)
    {
        switch (currentMenu)
		{
			case MENU_ROOT:
				currentMenu = MenuRoot();
				break;
			case MENU_UPDATE:
				currentMenu = MenuUpdate();
				break;
			case MENU_CHANGE_VERSION:
				currentMenu = MenuChangeVersion();
				break;
                case MENU_SETTINGS:
                currentMenu = MenuSettings();
			default:
				currentMenu = MenuRoot();
				break;
		}
    }
}

int MenuRoot()
{
    int selected_menu = MENU_NONE;

    GuiImageData btnLeft(button_left_png);
    GuiImageData btnRight(button_right_png);

    GuiButton versionsBtn(btnRight.GetWidth(), btnRight.GetHeight());
    GuiImage versionsBtnImg(&btnRight);
    GuiText versionsBtnTxt("Versions", 50, (GXColor){255, 255, 255, 255});
    versionsBtnTxt.SetPosition(22, -10);
    versionsBtnTxt.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
    versionsBtnTxt.SetWrap(true, btnRight.GetWidth()-30);
    versionsBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
    versionsBtn.SetPosition(BOUNDS_RIGHT, BOUNDS_TOP);
    versionsBtn.SetLabel(&versionsBtnTxt);
    versionsBtn.SetImage(&versionsBtnImg);
    versionsBtn.SetEffectGrow();

    GuiButton exitBtn(btnRight.GetWidth(), btnRight.GetHeight());
    GuiImage exitBtnImg(&btnRight);
    GuiText exitBtnTxt("Exit", 50, (GXColor){255, 255, 255, 255});
    exitBtnTxt.SetPosition(22, -10);
    exitBtnTxt.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
    exitBtnTxt.SetWrap(true, btnRight.GetWidth()-30);
    exitBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
    exitBtn.SetPosition(BOUNDS_RIGHT, BOUNDS_TOP + versionsBtn.GetHeight() + 40);
    exitBtn.SetLabel(&exitBtnTxt);
    exitBtn.SetImage(&exitBtnImg);
    exitBtn.SetEffectGrow();

    GuiButton playBtn(btnLeft.GetWidth(), btnLeft.GetHeight());
    GuiImage playBtnImg(&btnLeft);
    GuiText playBtnTxt("Play", 50, (GXColor){255, 255, 255, 255});
    playBtnTxt.SetPosition(-32, -10);
    playBtnTxt.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
    playBtnTxt.SetWrap(true, btnLeft.GetWidth()-30);
    playBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
    playBtn.SetPosition(0, BOUNDS_TOP);
    playBtn.SetLabel(&playBtnTxt);
    playBtn.SetImage(&playBtnImg);
    playBtn.SetEffectGrow();

    GuiButton settingsBtn(btnLeft.GetWidth(), btnLeft.GetHeight());
    GuiImage settingsBtnImg(&btnLeft);
    GuiText settingsBtnTxt("Settings", 50, (GXColor){255, 255, 255, 255});
    settingsBtnTxt.SetPosition(-32, -10);
    settingsBtnTxt.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
    settingsBtnTxt.SetWrap(true, btnLeft.GetWidth()-30);
    settingsBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
    settingsBtn.SetPosition(0, BOUNDS_TOP + playBtn.GetHeight() + 40);
    settingsBtn.SetLabel(&settingsBtnTxt);
    settingsBtn.SetImage(&settingsBtnImg);
    settingsBtn.SetEffectGrow();

    GuiWindow w(screenwidth, screenheight);

    HaltGui();
    w.Append(&exitBtn);
    w.Append(&versionsBtn);
    w.Append(&playBtn);
    w.Append(&settingsBtn);

    mainWindow->Append(&w);

    ResumeGui();

    while (selected_menu == MENU_NONE)
    {
        VIDEO_WaitVSync();
        if(playBtn.GetState() == STATE_CLICKED)
        {
            selected_menu = MENU_LAUNCH;
        }
        if(exitBtn.GetState() == STATE_CLICKED)
        {
            selected_menu = MENU_EXIT;
        }
        if(versionsBtn.GetState() == STATE_CLICKED)
        {
            selected_menu = MENU_CHANGE_VERSION;
        }
        if(settingsBtn.GetState() == STATE_CLICKED)
        {
            selected_menu = MENU_SETTINGS;
        }
    }

    HaltGui();
    mainWindow->Remove(&w);
    ResumeGui();
    return selected_menu;
}

int MenuUpdate()
{
    return 0;
}

int MenuChangeVersion()
{
    return 0;
}

int MenuSettings()
{
    return 0;
}

/****************************************************************************
 * UpdateGUI
 *
 * Primary thread to allow GUI to respond to state changes, and draws GUI
 ***************************************************************************/

void * UpdateGUI (void *arg)
{
    int i;
	while(1)
	{
		if(guiHalt)
			LWP_SuspendThread(guithread);

        UpdatePads();
		mainWindow->Draw();
		Menu_Render();

        for(i=0; i < 4; i++)
			mainWindow->Update(&userInput[i]);

        usleep(THREAD_SLEEP);

	}
	return NULL;
}

/****************************************************************************
 * InitGUIThread
 *
 * Startup GUI threads
 ***************************************************************************/
void InitGUIThreads()
{
	LWP_CreateThread (&guithread, UpdateGUI, NULL, NULL, 0, 70);
}

/****************************************************************************
 * ResumeGui
 *
 * Signals the GUI thread to start, and resumes the thread. This is called
 * after finishing the removal/insertion of new elements, and after initial
 * GUI setup.
 ***************************************************************************/
void ResumeGui()
{
    guiHalt = false;
    LWP_ResumeThread (guithread);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the GUI thread to stop, and waits for GUI thread to stop
 * This is necessary whenever removing/inserting new elements into the GUI.
 * This eliminates the possibility that the GUI is in the middle of accessing
 * an element that is being changed.
 ***************************************************************************/
void HaltGui()
{
    if(guiHalt)
        return;

	guiHalt = true;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(guithread))
		usleep(THREAD_SLEEP);
}
