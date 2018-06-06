#include "libwiigui/gui.h"
#ifndef __MENU_H__
#define __MENU_H__

#ifdef __cplusplus
extern "C" {
#endif

// Globals
extern GuiWindow *mainWindow;
extern u8 currentMenu;
extern const char * descriptionText;

enum {
    MENU_NONE,
    MENU_ROOT,
    MENU_UPDATE,
    MENU_CHANGE_VERSION,
    MENU_SETTINGS,
    MENU_EXIT,
    MENU_LAUNCH
};

// Function prototypes
void draw_menu();
void InitGUIThreads();
void ResumeGui();
void HaltGui();

int MenuRoot();
int MenuUpdate();
int MenuChangeVersion();
int MenuSettings();
int MenuRoot();

#ifdef __cplusplus
}
#endif

#endif // __MENU_H__
