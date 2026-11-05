"""Scope guard for neural labels, independent from validated PDA action APIs."""
from text_features import FOLDS, expand

MUTATIONS = set('excluir alterar enviar telefonar parar cancelar desligar reiniciar instalar atualizar'.split())
COMMANDS = set('abrir entrar acessar iniciar criar anotar escrever salvar guardar registrar mostrar consultar dizer ver telefonar desligar excluir enviar calcular somar contar marcar programar ativar reiniciar cancelar parar alterar'.split())
APPS = set('agenda calendario notas anotacoes contatos telefonica calculadora mapa jogos relogio desenho desenhos planilha planilhas diagnostico ajustes configuracoes bluetooth'.split())
ADVANCED = set('raiz seno cosseno tangente logaritmo log integral derivada matriz cotacao dolares dolar euros euro libras potencia elevado fatorial exponencial'.split())
OPERATORS = set('mais menos vezes dividido divisao soma somar multiplicado multiplicacao multiplicar dividir subtracao subtraia subtrair tire diferenca produto quociente + - * /'.split())


def allowed(words, label):
    if label == 0:
        return True
    canonical = [FOLDS.get(w, w) for w in words]
    body = len(words)
    if label == 7:
        for i,w in enumerate(words):
            if w in ('anote','anota','anotar'):
                body = i; break
            if w in ('escreva','escreve','escrever') and i+1<len(words) and words[i+1]=='que':
                body = i; break
            if w in ('nota','anotacao','anotacoes','anotado','escrito','frase'):
                body = i; break
    prefix = canonical[:body] if label == 7 else canonical
    negative = ('nao','sem','evite','desista')
    exception = label == 7 and words[:3] == ['nao','posso','esquecer']
    if not exception and any(w in negative for w in prefix):
        return False
    for i,w in enumerate(canonical):
        if w in ('e','depois','entao','tambem') and any(x in COMMANDS for x in canonical[i+1:i+4]):
            return False
    for i,w in enumerate(prefix):
        if w in MUTATIONS:
            if label == 6 and words[i] == 'ligue' and 'temporizador' in words and 'para' not in words:
                continue
            return False
    if label == 6:
        if any(w in ('resta','restam','restou','restante','sobrou','sobraram','faltam','falta','faltando','andamento','rodando','estado') for w in words):
            return False
    if label == 4 and any(w in ('rua','avenida','praca','endereco','localizacao','rota','travessa') for w in words):
        return False
    if label == 8:
        if not any(w in APPS for w in words) and not ('ferramenta' in words and 'contas' in words):
            return False
    if label == 5 and (any(w in ADVANCED for w in words) or not any(w in OPERATORS for w in canonical)):
        return False
    if label == 7 and 'lista' in words:
        explicit_note = any(w in ('nota','anotacao','anotacoes','anotado','escrito','frase') for w in words[:body+1])
        quoted = body+1<len(words) and words[body+1]=='que'
        if not explicit_note and not quoted:
            return False
    return True

def permits(words,label):
    words=expand(words)
    if label==0: return True
    canonical=[FOLDS.get(w,w) for w in words]
    end=len(words)
    if label==7:
        for i,w in enumerate(words):
            if w in ('anote','anota','anotar','nota','anotacao','anotacoes','anotado','escrito','frase') or (w in ('escreva','escreve','escrever') and i+1<len(words) and words[i+1]=='que'):
                end=i;break
    scope=words[:end]; actions=canonical[:end]
    if any(w in ('timer','temporizador','contagem') for w in scope) and any(w in ('resta','restam','restou','restante','sobrou','sobraram','faltam','falta','faltando','termina','terminar','acaba','acabar','estado','rodando','andamento') for w in scope):
        return False
    if any(w in ('contato','contatos','telefone','celular','numero','cadastro') for w in scope) and any(w in ('salvar','guardar','registrar','adicionar','criar','incluir','inclua') for w in actions):
        return False
    if any(w in ('agenda','calendario','reuniao','evento','compromisso','lembrete') for w in scope) and any(w in ('agende','agendar','adicionar','adicione','criar','marcar','registrar','inclua','incluir','salvar') for w in actions):
        return False
    if label==4 and any(w in ('salvar','guardar','registrar','adicionar','criar','incluir','inclua') for w in canonical):
        return False
    if label==3 and any(w in ('agende','agendar','adicionar','adicione','criar','marcar','registrar','inclua','incluir','salvar') for w in canonical):
        return False
    if label==1 and not any(w in ('bateria','carga','energia','porcentagem','percentagem','percentual') for w in words):
        return False
    if label==2 and not any(w in ('hora','horas','horario','relogio') for w in words):
        return False
    if label==4 and not any(w in ('telefone','celular','numero','contato','contatos','email','ficha','cadastro','dados') for w in words):
        return False
    if label==6 and not any(w in ('timer','temporizador','contagem','minuto','minutos','segundo','segundos','hora','horas') for w in words):
        return False
    return allowed(words,label)
