#include "pda/intent_pt.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

namespace pda { namespace ai {
namespace {
uint32_t read32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
float readFloat(const uint8_t *p) { float value; memcpy(&value, p, sizeof(value)); return value; }
uint32_t hash(const char *p) {
    uint32_t value = 2166136261u;
    while (*p) value = (value ^ uint8_t(*p++)) * 16777619u;
    return value;
}
void add(float *x, const char *term, float value) { x[hash(term) & (IntentPt::inputs - 1)] += value; }
bool normalize(const char *text, char *out) {
    const unsigned char *p = reinterpret_cast<const unsigned char *>(text);
    size_t n = 0; bool number = false;
    while (*p) {
        if ((p[0] == 0xCC && p[1] >= 0x80 && p[1] <= 0xBF) ||
            (p[0] == 0xCD && p[1] >= 0x80 && p[1] <= 0xAF)) { p += 2; continue; }
        char c = ' ';
        if (*p >= '0' && *p <= '9') {
            ++p; if (number) continue; number = true; c = '#';
        } else {
            number = false;
            if (*p < 128) { c = char(*p++); if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; }
            else if (p[0] == 0xC3 && p[1]) {
                unsigned b = p[1]; if (b < 0xA0) b += 0x20;
                const char fold[] = {'a','a','a','a','a','a',0,'c','e','e','e','e','i','i','i','i',0,'n','o','o','o','o','o',0,0,'u','u','u','u',0,0,0};
                if (b >= 0xA0 && b <= 0xBF && fold[b - 0xA0]) c = fold[b - 0xA0];
                p += 2;
            } else { ++p; while ((*p & 0xC0) == 0x80) ++p; }
        }
        if (c >= 'a' && c <= 'z') out[n++] = c;
        else if (c == '#' || c == '+' || c == '-' || c == '*' || c == '/' || c == '=') {
            out[n++] = ' '; out[n++] = c; out[n++] = ' ';
        } else out[n++] = ' ';
    }
    out[n] = 0;
    return n > 0;
}
bool member(const char *word, const char *list) {
    const size_t n = strlen(word);
    while (*list) {
        const char *end = strchr(list, ' ');
        const size_t size = end ? size_t(end - list) : strlen(list);
        if (size == n && !memcmp(list, word, n)) return true;
        if (!end) break;
        list = end + 1;
    }
    return false;
}
size_t prepareWords(const char **words, size_t count) {
    const char *commands = "abra abre abrir entre acesse inicie iniciar crie cria criar anote anota anotar escreva escreva salve salva salvar guarde registre mostre mostra consulte diga veja ligue desligue apague exclua remova mande envie calcule calcula some conte conta marque programe ative reinicie cancele pare pause mude altere";
    for (size_t i = 0; i < count; ++i) if (member(words[i], "e depois entao tambem")) {
        for (size_t j = i + 1; j < count && j < i + 4; ++j) if (member(words[j], commands)) return count;
    }
    bool createSeen = false;
    for (size_t i = 0; i < count; ++i) {
        if (member(words[i], "anote anota anotar")) { words[i + 1] = "texto"; return i + 2; }
        if (member(words[i], "escreva escreve escrever") && i + 1 < count && !strcmp(words[i + 1], "que")) {
            words[i + 2] = "texto"; return i + 3;
        }
        if (createSeen && member(words[i], "nota notas anotacao anotacoes anotado escrito frase")) {
            words[i + 1] = "texto"; return i + 2;
        }
        if (member(words[i], "crie cria criar anote anota anotar escreva escreve escrever registre registrar salve salva salvar guarde guarda guardar faca faz fazer deixe deixa deixar coloque coloca adicionar adicione acrescentar acrescente quero preciso")) createSeen = true;
    }
    for (size_t i = 0; i + 1 < count; ++i) {
        if (!member(words[i], "telefone celular email contato numero ficha cadastro dados")) continue;
        bool negate = false;
        for (size_t j = i + 1; j < count; ++j) if (member(words[j], "nao sem desista cancele")) negate = true;
        if (negate) continue;
        if (member(words[i + 1], "de da do das dos para")) { words[i + 2] = "pessoa"; return i + 3; }
        if (member(words[i], "contato ficha") && !member(words[i + 1], "atual salvo cadastrado telefonico")) {
            words[i + 1] = "pessoa"; return i + 2;
        }
    }
    return count;
}
#include "intent_pt_folds.inc"
size_t prepareV4(const char **words, size_t count) {
    const char *const prefixes[][3] = {{"por","favor",nullptr},{"no","meu","pda"},
        {"voce","pode",nullptr},{"voce","poderia",nullptr},{"ei",nullptr,nullptr},
        {"pode",nullptr,nullptr},{"poderia",nullptr,nullptr},{"consegue",nullptr,nullptr}};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &prefix : prefixes) {
            size_t n = 0; while (n < 3 && prefix[n]) ++n;
            if (count < n) continue;
            bool matches = true;
            for (size_t i = 0; i < n; ++i) if (strcmp(words[i], prefix[i])) matches = false;
            if (matches) {
                for (size_t i = n; i < count; ++i) words[i - n] = words[i];
                count -= n; changed = true; break;
            }
        }
    }
    const char *const suffixes[][2] = {{"por","favor"},{"pra","mim"},{"para","mim"}};
    for (const auto &suffix : suffixes) if (count >= 2 && !strcmp(words[count-2],suffix[0]) && !strcmp(words[count-1],suffix[1])) count -= 2;
    bool list = false;
    for (size_t i = 0; i < count; ++i) if (!strcmp(words[i], "lista")) list = true;
    if (!list) count = prepareWords(words, count);
    if (count > 1 && !strcmp(words[0], "telefone") && !strcmp(words[1], "para")) words[0] = "telefonar";
    for (size_t i = 0; i < count; ++i) for (const auto &fold : ptFolds) if (!strcmp(words[i], fold[0])) {
        words[i] = fold[1]; break;
    }
    return count;
}
#include "intent_pt_short.inc"
size_t expandV6(const char **words, size_t count) {
    if (count>=2 && !strcmp(words[0],"q") && !strcmp(words[1],"q")) { words[0]="o";words[1]="que"; }
    bool timer=false;
    for (size_t i=0;i<count;++i) if (member(words[i],"timer temporizador")) timer=true;
    for (size_t i=0;i<count;++i) {
        if (timer && !strcmp(words[i],"m")) {words[i]="minutos";continue;}
        if (timer && !strcmp(words[i],"s")) {words[i]="segundos";continue;}
        if (timer && !strcmp(words[i],"solta")) {words[i]="inicie";continue;}
        for (const auto &pair:ptShort) if (!strcmp(words[i],pair[0])) {words[i]=pair[1];break;}
    }
    if (count>1 && member(words[0],"numero telefone celular") && member(words[1],"de do da")) {
        for (size_t i=count;i>0;--i) words[i+1]=words[i-1];
        words[0]="qual";words[1]="o";count+=2;
    }
    if (count && !strcmp(words[0],"configuracoes")) {
        for (size_t i=count;i>0;--i) words[i]=words[i-1];
        words[0]="abrir";++count;
    }
    return count;
}
size_t prepareV6(const char **words,size_t count) {
    count=expandV6(words,count);
    bool special=false;
    for (size_t i=0;i<count;++i) if (member(words[i],"fiscal musical promissoria")) special=true;
    if (count && !strcmp(words[0],"nota") && !special) words[0]="anote";
    bool timer=false;
    for (size_t i=0;i<count;++i) if (member(words[i],"timer temporizador")) timer=true;
    if (timer && count && member(words[0],"timer temporizador coloque inicie crie cria iniciar")) {
        for (size_t i=0;i<count;++i) if (member(words[i],"minuto minutos segundo segundos hora horas")) {count=i+1;break;}
    }
    return prepareV4(words,count);
}
bool policyWords(const char **words, size_t count, int label) {
    if (label == 0) return true;
    const char *canonical[82];
    for (size_t i = 0; i < count; ++i) {
        canonical[i] = words[i];
        for (const auto &fold : ptFolds) if (!strcmp(words[i], fold[0])) { canonical[i] = fold[1]; break; }
    }
    size_t body = count;
    if (label == 7) for (size_t i = 0; i < count; ++i) {
        if (member(words[i], "anote anota anotar") ||
            (member(words[i], "escreva escreve escrever") && i+1<count && !strcmp(words[i+1],"que")) ||
            member(words[i], "nota anotacao anotacoes anotado escrito frase")) { body = i; break; }
    }
    const size_t prefix = label == 7 ? body : count;
    const bool exception = label == 7 && count >= 3 && !strcmp(words[0],"nao") && !strcmp(words[1],"posso") && !strcmp(words[2],"esquecer");
    if (!exception) for (size_t i = 0; i < prefix; ++i) if (member(canonical[i], "nao sem evite desista")) return false;
    for (size_t i = 0; i < count; ++i) if (member(canonical[i],"e depois entao tambem")) {
        for (size_t j = i+1; j<count && j<i+4; ++j)
            if (member(canonical[j],"abrir entrar acessar iniciar criar anotar escrever salvar guardar registrar mostrar consultar dizer ver telefonar desligar excluir enviar calcular somar contar marcar programar ativar reiniciar cancelar parar alterar")) return false;
    }
    bool hasTimer = false, hasPara = false;
    for (size_t i = 0; i < count; ++i) { if (!strcmp(words[i],"temporizador")) hasTimer = true; if (!strcmp(words[i],"para")) hasPara = true; }
    for (size_t i = 0; i < prefix; ++i) if (member(canonical[i],"excluir alterar enviar telefonar parar cancelar desligar reiniciar instalar atualizar")) {
        if (label == 6 && !strcmp(words[i],"ligue") && hasTimer && !hasPara) continue;
        return false;
    }
    if (label == 6) for (size_t i = 0; i < count; ++i)
        if (member(words[i],"resta restam restou restante sobrou sobraram faltam falta faltando andamento rodando estado")) return false;
    if (label == 4) for (size_t i = 0; i < count; ++i)
        if (member(words[i],"rua avenida praca endereco localizacao rota travessa")) return false;
    if (label == 8) {
        bool app = false, tool = false, math = false;
        for (size_t i = 0; i < count; ++i) {
            if (member(words[i],"agenda calendario notas anotacoes contatos telefonica calculadora mapa jogos relogio desenho desenhos planilha planilhas diagnostico ajustes configuracoes bluetooth")) app = true;
            if (!strcmp(words[i],"ferramenta")) tool = true;
            if (!strcmp(words[i],"contas")) math = true;
        }
        if (!app && !(tool && math)) return false;
    }
    if (label == 5) {
        bool op = false;
        for (size_t i = 0; i < count; ++i) {
            if (member(words[i],"raiz seno cosseno tangente logaritmo log integral derivada matriz cotacao dolares dolar euros euro libras potencia elevado fatorial exponencial")) return false;
            if (member(canonical[i],"mais menos vezes dividido divisao soma somar multiplicado multiplicacao multiplicar dividir subtracao subtraia subtrair tire diferenca produto quociente + - * /")) op = true;
        }
        if (!op) return false;
    }
    if (label == 7) {
        bool list = false, explicitNote = false;
        for (size_t i = 0; i < count; ++i) {
            if (!strcmp(words[i],"lista")) list = true;
            if (i <= body && member(words[i],"nota anotacao anotacoes anotado escrito frase")) explicitNote = true;
        }
        const bool quoted = body+1<count && !strcmp(words[body+1],"que");
        if (list && !explicitNote && !quoted) return false;
    }
    return true;
}
bool policyV6Words(const char **words,size_t count,int label) {
    if (!label) return true;
    const char *canonical[82];
    for (size_t i=0;i<count;++i) {
        canonical[i]=words[i];
        for (const auto &fold:ptFolds) if (!strcmp(words[i],fold[0])) {canonical[i]=fold[1];break;}
    }
    size_t end=count;
    if (label==7) for (size_t i=0;i<count;++i) {
        if (member(words[i],"anote anota anotar nota anotacao anotacoes anotado escrito frase") ||
            (member(words[i],"escreva escreve escrever") && i+1<count && !strcmp(words[i+1],"que"))) {end=i;break;}
    }
    bool timer=false,status=false,contact=false,createContact=false,agenda=false,createAgenda=false;
    for (size_t i=0;i<end;++i) {
        if (member(words[i],"timer temporizador contagem")) timer=true;
        if (member(words[i],"resta restam restou restante sobrou sobraram faltam falta faltando termina terminar acaba acabar estado rodando andamento")) status=true;
        if (member(words[i],"contato contatos telefone celular numero cadastro")) contact=true;
        if (member(canonical[i],"salvar guardar registrar adicionar criar incluir inclua")) createContact=true;
        if (member(words[i],"agenda calendario reuniao evento compromisso lembrete")) agenda=true;
        if (member(canonical[i],"agende agendar adicionar adicione criar marcar registrar inclua incluir salvar")) createAgenda=true;
    }
    if ((timer&&status)||(contact&&createContact)||(agenda&&createAgenda)) return false;
    bool mutationContact=false,mutationAgenda=false,evidence=false;
    for (size_t i=0;i<count;++i) {
        if (member(canonical[i],"salvar guardar registrar adicionar criar incluir inclua")) mutationContact=true;
        if (member(canonical[i],"agende agendar adicionar adicione criar marcar registrar inclua incluir salvar")) mutationAgenda=true;
        if (label==1 && member(words[i],"bateria carga energia porcentagem percentagem percentual")) evidence=true;
        if (label==2 && member(words[i],"hora horas horario relogio")) evidence=true;
        if (label==4 && member(words[i],"telefone celular numero contato contatos email ficha cadastro dados")) evidence=true;
        if (label==6 && member(words[i],"timer temporizador contagem minuto minutos segundo segundos hora horas")) evidence=true;
    }
    if ((label==4&&mutationContact)||(label==3&&mutationAgenda)) return false;
    if ((label==1||label==2||label==4||label==6)&&!evidence) return false;
    return policyWords(words,count,label);
}
}
const char *IntentPt::labelName(int label) {
    static const char *names[] = {"RECUSAR", "BATERIA", "HORA", "AGENDA", "CONTATO", "CALCULO", "TIMER", "NOTA", "ABRIR_APP"};
    return label >= 0 && label < labels ? names[label] : "INVALID";
}
bool IntentPt::begin(const uint8_t *data, size_t length) {
    data_ = nullptr;
    if (!data || length < 32) return false;
    const bool perClass = !memcmp(data, "PDAINT02", 8);
    if (length != (perClass ? perClassModelBytes : modelBytes) ||
        (!perClass && memcmp(data, "PDAINT01", 8)) || read32(data + 8) != inputs ||
        read32(data + 12) != hidden || read32(data + 16) != labels) return false;
    profile_ = read32(data + 28);
    if (profile_ < 1 || profile_ > 5) return false;
    weightsOffset_ = perClass ? 104 : 32;
    for (int i = 0; i < labels; ++i) {
        thresholds_[i] = readFloat(data + (perClass ? 32 + 4*i : 20));
        margins_[i] = readFloat(data + (perClass ? 32 + 4*labels + 4*i : 24));
        if (!isfinite(thresholds_[i]) || !isfinite(margins_[i]) || thresholds_[i] < 0 || thresholds_[i] > 1.01f || margins_[i] < 0 || margins_[i] > 1) return false;
    }
    data_ = data; return true;
}
float IntentPt::weight(size_t index) const { return readFloat(data_ + weightsOffset_ + 4 * index); }
bool IntentPt::features(const char *text, float *output, uint32_t profile) {
    if (!text || !output || !*text || strlen(text) > 80 || profile < 1 || profile > 5) return false;
    memset(output, 0, inputs * sizeof(float));
    char normalized[256], previous[81] = {}, term[170];
    if (!normalize(text, normalized)) return false;
    const char *words[82]; size_t count = 0;
    char *word = normalized;
    while (*word) {
        while (*word == ' ') ++word;
        if (!*word) break;
        char *end = word; while (*end && *end != ' ') ++end;
        bool more = *end != 0; *end = 0;
        words[count++] = word;
        if (!more) break;
        word = end + 1;
    }
    if (profile == 2) count = prepareWords(words, count);
    if (profile==3 || profile==4) count = prepareV4(words, count);
    if (profile==5) count = prepareV6(words,count);
    for (size_t index = 0; index < count; ++index) {
        const char *word = words[index];
        snprintf(term, sizeof(term), "w:%s", word); add(output, term, 2);
        if (*previous) { snprintf(term, sizeof(term), "b:%s %s", previous, word); add(output, term, 1); }
        char padded[83]; const size_t length = strlen(word);
        padded[0] = '^'; memcpy(padded + 1, word, length); padded[length + 1] = '$';
        for (size_t size = 3; size <= 5; ++size) for (size_t offset = 0; offset + size <= length + 2; ++offset) {
            term[0] = 'c'; term[1] = ':'; memcpy(term + 2, padded + offset, size); term[size + 2] = 0;
            add(output, term, .25f);
        }
        memcpy(previous, word, length + 1);
    }
    float total = 0;
    for (int i = 0; i < inputs; ++i) { if (output[i] > 4) output[i] = 4; total += output[i] * output[i]; }
    if (total) { const float scale = 1 / sqrtf(total); for (int i = 0; i < inputs; ++i) output[i] *= scale; }
    return true;
}
bool IntentPt::policyAllows(const char *text, int label, uint32_t profile) {
    if (!text || !*text || strlen(text)>80 || label<0 || label>=labels || (profile!=4&&profile!=5)) return false;
    char normalized[256]; const char *words[82]; size_t count = 0;
    if (!normalize(text,normalized)) return false;
    char *word = normalized;
    while (*word) {
        while (*word == ' ') ++word;
        if (!*word) break;
        char *end = word; while (*end && *end!=' ') ++end;
        const bool more = *end != 0; *end = 0; words[count++] = word;
        if (!more) break;
        word = end+1;
    }
    if (profile==5) {count=expandV6(words,count);return policyV6Words(words,count,label);}
    return policyWords(words,count,label);
}
bool IntentPt::predict(const char *text, Prediction &result) const {
    result = Prediction{};
    if (!data_) return false;
    float x[inputs], h[hidden];
    if (!features(text, x, profile_)) return false;
    for (int row = 0; row < hidden; ++row) {
        float value = weight(inputs * hidden + row);
        for (int i = 0; i < inputs; ++i) value += x[i] * weight(row * inputs + i);
        h[row] = tanhf(value);
    }
    constexpr size_t outputOffset = inputs * hidden + hidden;
    for (int row = 0; row < labels; ++row) {
        float value = weight(outputOffset + labels * hidden + row);
        for (int i = 0; i < hidden; ++i) value += h[i] * weight(outputOffset + row * hidden + i);
        result.probabilities[row] = value;
    }
    int best = 0;
    for (int i = 1; i < labels; ++i) if (result.probabilities[i] > result.probabilities[best]) best = i;
    const float maximum = result.probabilities[best]; float sum = 0;
    for (int i = 0; i < labels; ++i) { result.probabilities[i] = expf(result.probabilities[i] - maximum); sum += result.probabilities[i]; }
    for (int i = 0; i < labels; ++i) result.probabilities[i] /= sum;
    float second = 0;
    for (int i = 0; i < labels; ++i) if (i != best && result.probabilities[i] > second) second = result.probabilities[i];
    result.rawLabel = best; result.score = result.probabilities[best]; result.margin = result.score - second;
    if (result.score >= thresholds_[best] && result.margin >= margins_[best]) result.label = best;
    if (profile_ >= 4 && best != 0 && !policyAllows(text,best,profile_)) {
        result.policyRejected = true; result.label = 0;
    }
    return true;
}
} }
