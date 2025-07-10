#include "input/input.h"
#include "apps/apps.h"

void setup() {
    Serial.begin(115200);
    DisplayTouch::init();
    Trackball::init();
    // AppManager::init() inicializa todos os aplicativos, incluindo o MainMenu,
    // e exibe a tela inicial. Esta é a única chamada de inicialização de UI necessária.
    AppManager::init();
}

void loop() {
    AppManager::handle(); // O AppManager agora decide qual app gerenciar.
    delay(5); // Pequeno delay para evitar sobrecarga da CPU
}
