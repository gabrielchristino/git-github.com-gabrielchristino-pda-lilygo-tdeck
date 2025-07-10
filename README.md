# LilyGO T-Deck PDA Project

Este projeto implementa uma PDA (Personal Digital Assistant) baseada no LilyGO T-Deck, com drivers modulares e padronizados para entrada (trackball, teclado), utilitários e fácil expansão para novos periféricos.

## Estrutura do Projeto

```
lilygo-tdeck-pda
├── src
│   ├── main.cpp
│   ├── hardware/
│   │   └── TDECK_PINS.h
│   ├── input/
│   │   ├── keyboard.h
│   │   ├── trackball.h
│   │   └── trackball.cpp
│   └── utils/
│       └── utils.h
├── lib/
│   └── README.md
├── platformio.ini
└── README.md
```

## Padrão de Código

- **Namespaces**: Cada driver ou utilitário está em seu próprio namespace (`Trackball`, `Keyboard`, `Utils`), facilitando manutenção e evitando conflitos.
- **Documentação**: Todos os headers possuem exemplos de uso e instruções claras.
- **Funções Inline**: Funções simples são implementadas como `inline` nos headers para eficiência e simplicidade.

## Exemplos de Uso

### Trackball

```cpp
#include "input/trackball.h"
#include "utils/utils.h"

int16_t cursor_x = 0, cursor_y = 0;

void setup() {
    Serial.begin(115200);
    Utils::enablePeripherals();
    Trackball::init();
}

void loop() {
    // Modo 1: Movimentação livre (ex: cursor, desenho)
    Trackball::Delta delta = Trackball::read_delta();
    if (delta.dx != 0 || delta.dy != 0 || delta.clicks > 0) {
        cursor_x += delta.dx;
        cursor_y += delta.dy;
        if (delta.clicks > 0) {
            Serial.println("Trackball CLICK!");
        }
        Serial.printf("Cursor X: %d | Y: %d\n", cursor_x, cursor_y);
    }

    // Modo 2: Navegação por menus (direções)
    // OBS: Não use read_delta() e funções de direção no mesmo loop!
    if (Trackball::moved_up())    { Serial.println("Menu: CIMA"); }
    if (Trackball::moved_down())  { Serial.println("Menu: BAIXO"); }
    if (Trackball::moved_left())  { Serial.println("Menu: ESQUERDA"); }
    if (Trackball::moved_right()) { Serial.println("Menu: DIREITA"); }
    if (Trackball::clicked())     { Serial.println("Menu: SELECIONAR"); }
    delay(10);
}
```

### Teclado

```cpp
#include "input/keyboard.h"
#include "utils/utils.h"

void setup() {
    Serial.begin(115200);
    Utils::enablePeripherals();
    Keyboard::init();
}

void loop() {
    char key = Keyboard::get_key();
    if (key > 0) {
        Serial.print("Tecla pressionada: ");
        Serial.println(key);
    }
    delay(50);
}
```

## Boas Práticas

- **Escolha uma abordagem por loop:** Use `Trackball::read_delta()` **ou** as funções de direção (`moved_up()`, etc.), nunca ambas no mesmo loop.
- **Expansão:** Para novos módulos (ex: display, touchscreen), siga o padrão de namespace e documentação.
- **Utilitários:** Use funções do namespace `Utils` para inicialização e delays padronizados.

## Setup

1. Clone o repositório:
   ```
   git clone https://github.com/yourusername/lilygo-tdeck-pda.git
   ```
2. Navegue até o diretório do projeto:
   ```
   cd lilygo-tdeck-pda
   ```
3. Instale as bibliotecas necessárias conforme `lib/README.md`.
4. Abra o projeto no seu IDE e compile com PlatformIO.

## Contribuindo

Contribuições são bem-vindas! Envie issues ou pull requests para melhorias.

---

**Observação:**  
Para dúvidas sobre o padrão de código ou expansão do projeto, consulte os exemplos nos próprios headers dos drivers.