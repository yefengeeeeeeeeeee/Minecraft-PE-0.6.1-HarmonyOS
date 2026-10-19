#include "ConsoleScreen.h"
#include "../Gui.h"
#include "../../Minecraft.h"
#include "../../player/LocalPlayer.h"
#include "../../../AppPlatform.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../world/level/Level.h"
#include "../../../network/RakNetInstance.h"
#include "../../../network/ServerSideNetworkHandler.h"
#include "../../../network/packet/ChatPacket.h"
#include "../../../platform/log.h"

ConsoleScreen::ConsoleScreen()
:   _input(""),
    _cursorBlink(0)
{
}

void ConsoleScreen::init()
{
    // 聊天框一打开就请求屏幕键盘。
    // 触摸端的聊天按钮（TouchscreenInput 的 AREA_CHAT）已经自己调过一次，
    // 但 T 键 / 其他入口打开聊天框时没人调，这里补上（showKeyboard 可重复调用）。
    minecraft->platform()->showKeyboard();
}

void ConsoleScreen::removed()
{
    // 聊天框被关掉（回车发送 / ESC / 切到别的界面）时收起屏幕键盘，
    // 否则键盘会一直压在画面上，把整个游戏界面顶上去。
    minecraft->platform()->hideKeyboard();
}

void ConsoleScreen::tick()
{
    _cursorBlink++;
}

bool ConsoleScreen::handleBackEvent(bool /*isDown*/)
{
    minecraft->setScreen(NULL);
    return true;
}

void ConsoleScreen::keyPressed(int eventKey)
{
    if (eventKey == Keyboard::KEY_ESCAPE) {
        minecraft->setScreen(NULL);
    } else if (eventKey == Keyboard::KEY_RETURN) {
        execute();
    } else if (eventKey == Keyboard::KEY_BACKSPACE) {
        if (!_input.empty())
            _input.erase(_input.size() - 1, 1);
    } else {
        super::keyPressed(eventKey);
    }
}

void ConsoleScreen::charPressed(char inputChar)
{
    if (inputChar >= 32 && inputChar < 127)
        _input += inputChar;
}

// ---------------------------------------------------------------------------
// execute: send _input as a chat message, then close the screen
// ---------------------------------------------------------------------------
void ConsoleScreen::execute()
{
    if (_input.empty()) {
        minecraft->setScreen(NULL);
        return;
    }

    // Chat message: <name> message
    // 客户端不再解析 /command —— 以 '/' 开头的内容也按普通聊天文本发出去
    std::string msg = std::string("<") + minecraft->player->name + "> " + _input;
    if (minecraft->netCallback && minecraft->raknetInstance->isServer()) {
        // Hosting a LAN game: displayGameMessage shows locally + broadcasts MessagePacket to clients
        static_cast<ServerSideNetworkHandler*>(minecraft->netCallback)->displayGameMessage(msg);
    } else if (minecraft->netCallback) {
        // Connected client: send ChatPacket to server; server echoes it back as MessagePacket
        ChatPacket chatPkt(msg);
        minecraft->raknetInstance->send(chatPkt);
    } else {
        // Singleplayer: show locally only
        minecraft->gui.addMessage(msg);
    }

    minecraft->setScreen(NULL);
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------
void ConsoleScreen::render(int /*xm*/, int /*ym*/, float /*a*/)
{
    // Dim the game world slightly
    fillGradient(0, 0, width, height, 0x00000000, 0x40000000);

    const int boxH  = 12;
    const int boxY  = height - boxH - 2;
    const int boxX0 = 2;
    const int boxX1 = width - 2;

    // Input box background
    fill(boxX0, boxY, boxX1, boxY + boxH, 0xc0000000);
    // Border
    fill(boxX0,     boxY,          boxX1, boxY + 1,   0xff808080);
    fill(boxX0,     boxY + boxH - 1, boxX1, boxY + boxH, 0xff808080);
    fill(boxX0,     boxY,          boxX0 + 1, boxY + boxH, 0xff808080);
    fill(boxX1 - 1, boxY,          boxX1,     boxY + boxH, 0xff808080);

    // Input text + blinking cursor
    std::string displayed = _input;
    if ((_cursorBlink / 10) % 2 == 0)
        displayed += '_';

    // Placeholder hint when empty
    font->drawShadow(displayed, (float)(boxX0 + 2), (float)(boxY + 2), 0xffffffff);
}
