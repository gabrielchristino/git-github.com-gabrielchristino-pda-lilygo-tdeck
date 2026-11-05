"""Canonical Portuguese profile-5 features. Never use masked tokens as action arguments."""
import re
import numpy as np

DIM = 1024
PROFILE = 5
LABELS = ['RECUSAR', 'BATERIA', 'HORA', 'AGENDA', 'CONTATO', 'CALCULO', 'TIMER', 'NOTA', 'ABRIR_APP']

COMMANDS = set('abra abre abrir entre acesse inicie iniciar crie cria criar anote anota anotar escreva escreva salve salva salvar guarde registre mostre mostra consulte diga veja ligue desligue apague exclua remova mande envie calcule calcula some conte conta marque programe ative reinicie cancele pare pause mude altere'.split())
CREATE = set('crie cria criar anote anota anotar escreva escreve escrever registre registrar salve salva salvar guarde guarda guardar faca faz fazer deixe deixa deixar coloque coloca adicionar adicione acrescentar acrescente quero preciso'.split())
NOTE_MARKERS = set('nota notas anotacao anotacoes anotado escrito frase'.split())
CONTACT = set('telefone celular email contato numero ficha cadastro dados'.split())


def prepare_words(words):
    compound = any(word in ('e', 'depois', 'entao', 'tambem') and
                   any(w in COMMANDS for w in words[i + 1:i + 4]) for i, word in enumerate(words))
    if compound:
        return words
    for i, word in enumerate(words):
        if word in ('anote', 'anota', 'anotar'):
            return words[:i + 1] + ['texto']
        if word in ('escreva', 'escreve', 'escrever') and i + 1 < len(words) and words[i + 1] == 'que':
            return words[:i + 2] + ['texto']
        if word in NOTE_MARKERS and any(w in CREATE for w in words[:i]):
            return words[:i + 1] + ['texto']
    for i, word in enumerate(words):
        if word not in CONTACT or i + 1 == len(words):
            continue
        if any(w in ('nao', 'sem', 'desista', 'cancele') for w in words[i + 1:]):
            continue
        if words[i + 1] in ('de', 'da', 'do', 'das', 'dos', 'para'):
            return words[:i + 2] + ['pessoa']
        if word in ('contato', 'ficha') and words[i + 1] not in ('atual', 'salvo', 'cadastrado', 'telefonico'):
            return words[:i + 1] + ['pessoa']
    return words


FOLDS = {
 'crie': 'criar', 'cria': 'criar', 'anote': 'anotar', 'anota': 'anotar',
 'abra': 'abrir', 'abre': 'abrir', 'entre': 'entrar', 'acesse': 'acessar',
 'inicie': 'iniciar', 'inicia': 'iniciar', 'comece': 'iniciar', 'comeca': 'iniciar',
 'mostre': 'mostrar', 'mostra': 'mostrar', 'exiba': 'mostrar', 'exibir': 'mostrar',
 'veja': 'ver', 've': 'ver', 'consulte': 'consultar',
 'diga': 'dizer', 'diz': 'dizer', 'informe': 'informar', 'informa': 'informar',
 'procure': 'buscar', 'procura': 'buscar', 'busque': 'buscar',
 'localize': 'buscar', 'localizar': 'buscar', 'encontre': 'buscar',
 'salve': 'salvar', 'salva': 'salvar', 'guarde': 'guardar', 'guarda': 'guardar',
 'escreva': 'escrever', 'escreve': 'escrever', 'registre': 'registrar',
 'conte': 'contar', 'conta': 'contar', 'marque': 'marcar', 'marca': 'marcar',
 'avise': 'avisar', 'avisa': 'avisar', 'programe': 'programar', 'programa': 'programar',
 'defina': 'definir', 'define': 'definir', 'coloque': 'colocar', 'coloca': 'colocar',
 'calcule': 'calcular', 'calcula': 'calcular', 'some': 'somar',
 'multiplique': 'multiplicar', 'multiplica': 'multiplicar',
 'divida': 'dividir', 'divide': 'dividir', 'resolva': 'resolver', 'resolve': 'resolver',
 'remova': 'excluir', 'remover': 'excluir', 'apague': 'excluir', 'apagar': 'excluir',
 'exclua': 'excluir', 'delete': 'excluir', 'deletar': 'excluir',
 'altere': 'alterar', 'modifique': 'alterar', 'modificar': 'alterar',
 'corrija': 'alterar', 'corrigir': 'alterar', 'atualize': 'alterar', 'atualizar': 'alterar',
 'edite': 'alterar', 'editar': 'alterar', 'troque': 'alterar', 'trocar': 'alterar',
 'ligue': 'telefonar', 'ligar': 'telefonar', 'disque': 'telefonar',
 'envie': 'enviar', 'mande': 'enviar', 'mandar': 'enviar',
 'pare': 'parar', 'pause': 'parar', 'pausar': 'parar', 'cancele': 'cancelar',
 'eventos': 'evento', 'compromissos': 'compromisso', 'lembretes': 'evento', 'lembrete': 'evento',
 'reunioes': 'reuniao', 'anotacoes': 'notas', 'emails': 'email',
}


def prepare(words):
    words = list(words)
    prefixes = [('por', 'favor'), ('no', 'meu', 'pda'), ('voce', 'pode'), ('voce', 'poderia'),
                ('ei',), ('pode',), ('poderia',), ('consegue',)]
    changed = True
    while changed:
        changed = False
        for prefix in prefixes:
            if words[:len(prefix)] == list(prefix):
                words = words[len(prefix):]; changed = True; break
    for suffix in [('por', 'favor'), ('pra', 'mim'), ('para', 'mim')]:
        if words[-len(suffix):] == list(suffix): words = words[:-len(suffix)]
    # Keep a list request visible instead of turning it into a free-form note.
    if 'lista' not in words:
        words = prepare_words(words)
    if len(words) > 1 and words[0] == 'telefone' and words[1] == 'para':
        words[0] = 'telefonar'
    return [FOLDS.get(word, word) for word in words]


SHORT = {'q':'que','qnt':'quanto','qto':'quanto','qnd':'quando','qdo':'quando',
         'hj':'hoje','prox':'proximo','cel':'celular','bat':'bateria','num':'numero',
         'calc':'calculadora','config':'configuracoes','min':'minutos','mins':'minutos',
         'seg':'segundos','segs':'segundos','hr':'hora','hrs':'horas','add':'adicionar',
         'poe':'coloque','bota':'coloque'}


def expand(words):
    words=list(words)
    if words[:2]==['q','q']: words[:2]=['o','que']
    timer=any(w in ('timer','temporizador') for w in words)
    words=[('minutos' if timer and w=='m' else 'segundos' if timer and w=='s' else
            'inicie' if timer and w=='solta' else SHORT.get(w,w)) for w in words]
    if len(words)>1 and words[0] in ('numero','telefone','celular') and words[1] in ('de','do','da'):
        words=['qual','o']+words
    if words and words[0]=='configuracoes': words=['abrir']+words
    return words


def features_words(words):
    words=expand(words)
    if words and words[0]=='nota' and not any(w in ('fiscal','musical','promissoria') for w in words):
        words[0]='anote'
    # Purpose text is not a duration. Keep the command up to its unit; policy
    # validation still sees the full original expanded input.
    timer=any(w in ('timer','temporizador') for w in words)
    if timer and words and words[0] in ('timer','temporizador','coloque','inicie','crie','cria','iniciar'):
        for i,w in enumerate(words):
            if w in ('minuto','minutos','segundo','segundos','hora','horas'):
                words=words[:i+1];break
    return prepare(words)

def normalize(text):
    folds = dict(zip('àáâãäåçèéêëìíîïñòóôõöùúûü', 'aaaaaaceeeeiiiinooooouuuu'))
    text = ''.join(ch if ord(ch) < 128 else folds.get(ch, ' ') for ch in text.lower()
                   if not 0x300 <= ord(ch) <= 0x36F)
    text = re.sub(r'\d+', '#', text)
    return ' '.join(re.findall(r'[a-z]+|#|[+*/=\-]', text))


def hash32(text):
    result = 2166136261
    for byte in text.encode('ascii'):
        result = ((result ^ byte) * 16777619) & 0xFFFFFFFF
    return result


def command_words(text, profile=5):
    if profile != 5:
        raise ValueError("Only the current feature profile 5 is supported")
    return features_words(normalize(text).split())

def features(text, profile=5):
    x = np.zeros(DIM, dtype=np.float32)
    words = command_words(text, profile)
    def add(term, value):
        x[hash32(term) & (DIM - 1)] += value
    for i, word in enumerate(words):
        add('w:' + word, 2.0)
        if i: add('b:' + words[i - 1] + ' ' + word, 1.0)
        padded = '^' + word + '$'
        for size in (3, 4, 5):
            for offset in range(max(0, len(padded) - size + 1)):
                add('c:' + padded[offset:offset + size], .25)
    np.minimum(x, 4, out=x)
    total = float(np.dot(x, x))
    if total: x /= np.float32(total ** .5)
    return x


