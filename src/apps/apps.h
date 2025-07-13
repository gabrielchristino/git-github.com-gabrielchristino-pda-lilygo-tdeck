#pragma once

/**
 * Application Manager Header
 *
 * Este é o arquivo central para incluir todos os aplicativos do PDA.
 * O arquivo principal do projeto (ex: .ino) deve incluir apenas este header
 * para ter acesso a todos os aplicativos.
 *
 * Ele também contém um namespace 'AppManager' para orquestrar a inicialização
 * e, futuramente, o gerenciamento do ciclo de vida dos aplicativos.
 */

// --- AppManager Interface Definition ---
// Primeiro, definimos a "interface pública" do AppManager.
// Isso permite que os arquivos de aplicativos incluídos abaixo
// usem o AppManager sem causar dependências circulares.
namespace AppManager {

// Enum para identificar todos os aplicativos do sistema.
enum App {
    APP_MAIN_MENU,
    APP_CALCULATOR,
    APP_SETTINGS,
    APP_WEATHER,
    APP_NOTES,
    APP_CALENDAR
    // Adicione outros apps aqui no futuro
};

// Declarações de funções (a implementação vem depois dos includes)
void init();
void show_app(App app_to_show);
void handle();

} // namespace AppManager

// --- Application Includes ---
// Agora, incluímos todos os cabeçalhos dos aplicativos.
#include "mainmenu/mainmenu.h"
#include "calculator/calculator.h"
#include "settings/settings.h"
#include "weather/weather.h"
#include "notes/notes.h"
#include "calendar/calendar.h"
//#include "sketch/sketch.h" // Exemplo para futuros apps

// --- AppManager Implementation ---
// Com os aplicativos já incluídos, agora podemos definir a implementação
// das funções do AppManager, que dependem dos namespaces dos aplicativos.
namespace AppManager {

// Variável estática para rastrear qual aplicativo está ativo.
static App current_app = APP_MAIN_MENU;

inline void init() {
    MainMenu::init();
    Calculator::init();
    Settings::init();
    Weather::init();
    Notes::init();
    Calendar::init();
    // Mostra o menu principal ao iniciar
    show_app(APP_MAIN_MENU);
}

inline void show_app(App app_to_show) {
    current_app = app_to_show;
    switch (current_app) {
        case APP_MAIN_MENU:
            MainMenu::show();
            break;
        case APP_CALCULATOR:
            Calculator::show();
            break;
        case APP_SETTINGS:
            Settings::show();
            break;
        case APP_WEATHER:
            Weather::show();
            break;
        case APP_NOTES:
            Notes::show();
            break;
        case APP_CALENDAR:
            Calendar::show();
            break;
    }
}

inline void handle() {
    // O switch garante que apenas o handler do app ativo seja executado.
    if (current_app == APP_MAIN_MENU) MainMenu::handle();
    else if (current_app == APP_CALCULATOR) Calculator::handle();
    else if (current_app == APP_SETTINGS) Settings::handle();
    else if (current_app == APP_WEATHER) Weather::handle();
    else if (current_app == APP_NOTES) Notes::handle();
    else if (current_app == APP_CALENDAR) Calendar::handle();
}

} // namespace AppManager
