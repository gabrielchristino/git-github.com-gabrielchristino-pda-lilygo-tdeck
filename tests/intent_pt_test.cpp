#include "pda/intent_pt.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstring>
#include <iostream>
#include <cmath>

int main() {
    using pda::ai::IntentPt;
    std::ifstream f("data/ai/intent_pt.bin", std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    IntentPt model; IntentPt::Prediction result;
    assert(!model.predict("bateria", result));
    assert(!model.begin(data.data(), data.size() - 1));
    auto changed = data; changed[8] ^= 1;
    assert(!model.begin(changed.data(), changed.size()));
    assert(model.begin(data.data(), data.size()));
    assert(!model.predict("", result));
    char longText[82]; memset(longText, 'a', 81); longText[81] = 0;
    assert(!model.predict(longText, result));
    char boundary[81]; memset(boundary, '+', 80); boundary[80] = 0;
    std::vector<float> workspace(IntentPt::inputs + 2, 12345);
    assert(IntentPt::features(boundary, workspace.data() + 1));
    assert(workspace.front() == 12345 && workspace.back() == 12345);
    assert(model.predict("Quanto tenho de bateria?", result) && result.label == 1);
    assert(model.predict("Quanto e 23 vezes 17?", result) && result.label == 5);
    assert(model.predict("Nao anote comprar pilhas", result) && result.label == 0);
    assert(model.predict("Qual a capital do Brasil?", result) && result.label == 0);
    float a[IntentPt::inputs], b[IntentPt::inputs];
    assert(IntentPt::features("qual o telefone da Marcela", a, 2));
    assert(IntentPt::features("qual o telefone da pessoa", b, 2));
    assert(!memcmp(a, b, sizeof(a)));
    assert(IntentPt::features("anote que nao devo esquecer de comprar material", a, 2));
    assert(IntentPt::features("anote texto", b, 2));
    assert(!memcmp(a, b, sizeof(a)));
    assert(IntentPt::features("nao anote comprar material", a, 2));
    assert(memcmp(a, b, sizeof(a)));
    assert(IntentPt::features("anote comprar material e apague todas as notas", a, 2));
    assert(memcmp(a, b, sizeof(a)));
    assert(!IntentPt::features("telefone", a, 6));
    assert(IntentPt::features(boundary, workspace.data() + 1, 2));
    assert(workspace.front() == 12345 && workspace.back() == 12345);
    changed = data; changed[28] = 2;
    assert(model.begin(changed.data(), changed.size()) && model.featureProfile() == 2);
    assert(model.predict("anote texto", result));
    changed[28] = 3;
    assert(model.begin(changed.data(), changed.size()));
    assert(IntentPt::features("por favor anote comprar material", a, 3));
    assert(IntentPt::features("anotar texto", b, 3));
    assert(!memcmp(a,b,sizeof(a)));
    assert(IntentPt::features("anote comprar material na lista", a, 3));
    assert(memcmp(a,b,sizeof(a)));
    assert(IntentPt::features("remova o contato de Joana", a, 3));
    assert(IntentPt::features("excluir o contato de Joana", b, 3));
    assert(!memcmp(a,b,sizeof(a)));
    assert(IntentPt::features(boundary, workspace.data() + 1, 3));
    assert(workspace.front() == 12345 && workspace.back() == 12345);
    changed[28] = 4;
    assert(model.begin(changed.data(), changed.size()));
    assert(!IntentPt::policyAllows("Sem iniciar contagem de trinta segundos", 6));
    assert(!IntentPt::policyAllows("Quanto tempo restou no temporizador", 6));
    assert(!IntentPt::policyAllows("Localize o numero da avenida Paulista", 4));
    assert(!IntentPt::policyAllows("Abra aplicativo de mensagens", 8));
    assert(!IntentPt::policyAllows("Qual o logaritmo de 20", 5));
    assert(!IntentPt::policyAllows("Remova o contato de Ana", 4));
    assert(!IntentPt::policyAllows("Anote ovos na minha lista", 7));
    assert(IntentPt::policyAllows("Anote que minha lista esta na geladeira", 7));
    assert(IntentPt::policyAllows("Escreva que nao devo ligar o forno", 7));
    assert(IntentPt::policyAllows("Crie uma nota: nao esquecer a chave", 7));
    assert(IntentPt::policyAllows("Qual o celular da minha mae", 4));
    assert(IntentPt::policyAllows("Crie um timer de meia hora", 6));
    assert(!IntentPt::policyAllows("Diga a hora e mostre a agenda", 2));
    assert(!IntentPt::policyAllows("", 1));
    changed[28] = 5;
    assert(model.begin(changed.data(), changed.size()));
    assert(IntentPt::features("qual o cel do leo",a,5));
    assert(IntentPt::features("qual o celular do Rui",b,5));
    assert(!memcmp(a,b,sizeof(a)));
    assert(IntentPt::features("timer 12m bolo",a,5));
    assert(IntentPt::features("timer 12 minutos",b,5));
    assert(!memcmp(a,b,sizeof(a)));
    assert(IntentPt::features("nota: trocar oleo do carro",a,5));
    assert(IntentPt::features("anote texto",b,5));
    assert(!memcmp(a,b,sizeof(a)));
    assert(IntentPt::features(boundary,workspace.data()+1,5));
    assert(workspace.front()==12345 && workspace.back()==12345);
    assert(!IntentPt::policyAllows("salva o contato da oficina 12345",4,5));
    assert(!IntentPt::policyAllows("add contato oficina 12345",4,5));
    assert(!IntentPt::policyAllows("Quando termina a contagem",1,5));
    assert(!IntentPt::policyAllows("Q horas o timer termina",2,5));
    assert(!IntentPt::policyAllows("Agende uma reuniao quarta",3,5));
    assert(IntentPt::policyAllows("Qual o cel do leo",4,5));
    assert(IntentPt::policyAllows("nota: trocar oleo do carro",7,5));
    assert(IntentPt::policyAllows("anote que devo salvar o contato da oficina",7,5));
    changed[28] = 6;
    assert(!model.begin(changed.data(), changed.size()));
    std::vector<uint8_t> perClass(IntentPt::perClassModelBytes);
    memcpy(perClass.data(), data.data(), 32);
    perClass[7] = '2';
    for (int i = 0; i < IntentPt::labels; ++i) {
        memcpy(perClass.data() + 32 + 4*i, data.data() + 20, 4);
        memcpy(perClass.data() + 32 + 4*IntentPt::labels + 4*i, data.data() + 24, 4);
    }
    memcpy(perClass.data() + 104, data.data() + 32, data.size() - 32);
    assert(model.begin(perClass.data(), perClass.size()));
    assert(model.predict("Quanto tenho de bateria?", result) && result.label == 1);
    assert(!model.begin(perClass.data(), perClass.size() - 1));
    const float invalid = NAN;
    memcpy(perClass.data() + 32, &invalid, sizeof(invalid));
    assert(!model.begin(perClass.data(), perClass.size()));
    std::cout << "intent pt: model bounds, input limits, feature sentinels and basic intent/refusal fixtures OK\n";
}
