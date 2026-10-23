#include "ScreenChooser.h"
#include "StartMenuScreen.h"
#include "SelectWorldScreen.h"
#include "JoinGameScreen.h"
#include "PauseScreen.h"
#include "RenameMPLevelScreen.h"
#include "ConsoleScreen.h"
#include "IngameBlockSelectionScreen.h"
#include "JoinByIPScreen.h"
#include "touch/TouchStartMenuScreen.h"
#include "touch/TouchSelectWorldScreen.h"
#include "touch/TouchJoinGameScreen.h"
#include "touch/TouchIngameBlockSelectionScreen.h"
#include "desktop/DesktopCreativeScreen.h"
#include "../../gamemode/GameMode.h"

#include "../../Minecraft.h"

#include "UsernameScreen.h"

Screen* ScreenChooser::createScreen( ScreenId id )
{
	Screen* screen = NULL;

	// :sob:
	if (_mc->options.getIntValue(OPTIONS_MENU_STYLE) == 0) {
		switch (id) {
		case SCREEN_STARTMENU:	     screen = new Touch::StartMenuScreen();	break;
		case SCREEN_SELECTWORLD:     screen = new Touch::SelectWorldScreen();break;
		case SCREEN_JOINGAME:	     screen = new Touch::JoinGameScreen();	break;
		case SCREEN_PAUSE:	         screen = new PauseScreen(false); break;
		case SCREEN_PAUSEPREV:	     screen = new PauseScreen(true);	 break;
		case SCREEN_BLOCKSELECTION:
			// 创造模式同样走 PC 式创造背包（所有物品含钓鱼竿都会列出）；
			// 旧版 "Select blocks" 方块选择器只列方块，钓鱼竿等工具永远不出现
			if (_mc->gameMode && _mc->gameMode->isCreativeType())
				screen = new DesktopCreativeScreen();
			else
				screen = new Touch::IngameBlockSelectionScreen();
			break;
		case SCREEN_JOINBYIP:        screen = new JoinByIPScreen(); break;
		case SCREEN_CONSOLE:		 screen = new ConsoleScreen(); break;
		case SCREEN_NONE:
		default:
			// Do nothing
			break;
		}
	} else {
		switch (id) {
		case SCREEN_STARTMENU:	     screen = new StartMenuScreen();  break;
		case SCREEN_SELECTWORLD:     screen = new SelectWorldScreen();break;
		case SCREEN_JOINGAME:	     screen = new JoinGameScreen();   break;
		case SCREEN_PAUSE:	         screen = new PauseScreen(false); break;
		case SCREEN_PAUSEPREV:	     screen = new PauseScreen(true);	 break;
		case SCREEN_BLOCKSELECTION:
			// In creative we open the PC-style palette (every item, tabs,
			// scrolling, mouse + keyboard). The old touch-style hotbar
			// picker stays as the fallback for survival's "more..." slot.
			if (_mc->gameMode && _mc->gameMode->isCreativeType())
				screen = new DesktopCreativeScreen();
			else
				screen = new IngameBlockSelectionScreen();
			break;
		case SCREEN_JOINBYIP:        screen = new JoinByIPScreen(); break;
		case SCREEN_CONSOLE:		 screen = new ConsoleScreen(); break;
		case SCREEN_NONE:
		default:
			// Do nothing
			break;
		}
	}
	return screen;
}

Screen* ScreenChooser::setScreen(ScreenId id)
{
	Screen* screen = createScreen(id);
	_mc->setScreen(screen);
	return screen;
}
