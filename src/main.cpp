#include "ChatRoom.h"
#include "ConnectScreen.h"
#include "EmbeddedServer.h"
#include "Game.h"
#include "Leaderboard.h"
#include "LocalMatch.h"
#include "LoginScreen.h"
#include "Menu.h"
#include "NetClient.h"
#include "Protocol.h"
#include "SoloMenu.h"
#include "Store.h"
#include "UiTheme.h"
#include "I18n.h"
#include <raylib.h>
int main(int , char** ) {
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    SetTraceLogLevel(LOG_WARNING);
    NetClient net;
    EmbeddedServer embedded;
    while (true) {
        ConnectScreen cs(net, &embedded);
        if (!cs.run()) {
            net.disconnect();
            embedded.stop();
            ui::unloadFonts();
            return 0;
        }
        bool loggedIn = false;
        while (net.isConnected() && !loggedIn) {
            LoginScreen login(net);
            if (!login.run()) {
                net.disconnect();
                break;
            }
            loggedIn = net.loggedIn;
        }
        while (net.isConnected() && loggedIn) {
            Menu menu(net);
            Menu::Result r = menu.run();
            switch (r.action) {
                case Menu::Action::Play: {
                    Game g(net, "");
                    g.run();
                    break;
                }
                case Menu::Action::Solo: {
                    SoloMenu sm(net);
                    SoloMenu::Result sr = sm.run();
                    if (sr == SoloMenu::Result::Practice) {
                        LocalMatch lm(net, LocalMatch::Mode::Practice);
                        lm.run();
                    } else if (sr == SoloMenu::Result::VsBots) {
                        LocalMatch lm(net, LocalMatch::Mode::VsBots);
                        lm.run();
                    }
                    break;
                }
                case Menu::Action::Leaderboard: {
                    Leaderboard l(net);
                    l.run();
                    break;
                }
                case Menu::Action::Chat: {
                    ChatRoom c(net);
                    c.run();
                    break;
                }
                case Menu::Action::Store: {
                    Store s(net);
                    s.run();
                    break;
                }
                case Menu::Action::Logout:
                    net.sendTcp({ proto::kT_Logout });
                    net.disconnect();
                    loggedIn = false;
                    break;
                case Menu::Action::Quit:
                    net.disconnect();
                    embedded.stop();
                    ui::unloadFonts();
                    return 0;
            }
        }
    }
}
