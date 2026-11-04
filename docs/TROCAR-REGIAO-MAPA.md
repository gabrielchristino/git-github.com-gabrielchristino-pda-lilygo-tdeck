# Usar o mapa de outra região

O mapa é incorporado ao firmware durante a compilação. Hoje, trocar a região
é um procedimento de desenvolvimento: escolher um recorte, gerar os vetores no
computador, ajustar a projeção e recompilar. Não há importação de mapas pelo
menu, USB ou cartão SD. O download exige internet no computador; depois de
gravado, o mapa funciona offline no T-Deck.

As ruas vêm do OpenStreetMap. A busca de números de imóveis usa uma segunda
base, do IBGE, que precisa ser substituída ou desativada ao mudar de região.
Trocar somente os vetores deixa a numeração antiga associada ao mapa novo.

## 1. Escolher um recorte pequeno

Use coordenadas em graus decimais. A caixa é definida nesta ordem:

```text
(latitude sul, longitude oeste, latitude norte, longitude leste)
```

Sul deve ser menor que norte; oeste deve ser menor que leste. No Brasil,
lembre que -51.26 fica a oeste de -51.10. Escolha também uma origem próxima
do centro da caixa. Comece por um bairro ou parte da cidade; não tente um
estado ou país inteiro.

Exemplo de configuração para um recorte em Porto Alegre, a ajustar conforme
a área desejada:

```python
BBOX = (-30.10, -51.26, -29.97, -51.10)
ORIGIN = (-30.035, -51.18)
```

O formato atual guarda coordenadas em metros com sinal de 16 bits: cada eixo
deve ficar entre -32768 e 32767 em relação à origem. Esse é um limite do
formato, não uma garantia de que toda área desse tamanho caiba na flash.
Recortes densos podem exceder o limite mesmo sendo pequenos. Regiões que
cruzam o antimeridiano ou próximas aos polos exigem adaptar a projeção.

## 2. Ajustar a configuração em três lugares

1. Em [`tools/maps/build_pinheiros.py`](../tools/maps/build_pinheiros.py), altere
   `BBOX` e `ORIGIN` no início do arquivo. No objeto `manifest`, perto do final,
   altere o texto de `region` para identificar o novo recorte.
2. Em [`include/pda/map.h`](../include/pda/map.h), coloque a mesma origem em
   `originLat` e `originLon`. O Python e o firmware precisam usar os mesmos
   valores para o GPS e os vetores coincidirem.
3. Em [`src/core/map.cpp`](../src/core/map.cpp), ajuste `View::reset()` para uma
   latitude/longitude dentro do novo recorte, por exemplo:

   ```cpp
   void View::reset() {
       project(-30.035, -51.18, center);
       zoom = 4;
       follow = false;
       clamp();
   }
   ```

Em [`src/apps/map.cpp`](../src/apps/map.cpp), atualize também o aviso que cita
`Pinheiros / Zona Sul` e os exemplos de busca para ruas da região escolhida.
Os nomes dos arquivos `pinheiros-*` são históricos; podem ser mantidos para
não quebrar os caminhos usados pelas ferramentas.

## 3. Gerar as ruas, rios e ferrovias

Na raiz do repositório, com Python 3 disponível:

```powershell
python tools/maps/build_pinheiros.py --download --source build/maps/minha-regiao-osm.json
```

O script usa a caixa configurada para consultar o Overpass e gera:

- `src/core/map_data.inc`: vetores e índice de busca usados pelo firmware.
- `data/maps/pinheiros.json`: região, limites, origem, consulta e hash da fonte.
- `build/maps/minha-regiao-osm.json`: resposta original, excluída do Git.

Para converter novamente o mesmo extrato sem baixar:

```powershell
python tools/maps/build_pinheiros.py --source build/maps/minha-regiao-osm.json
```

Ao mudar a caixa, baixe um extrato novo. Um JSON antigo não contém as ruas da
nova área. Em caso de timeout ou limite do serviço, reduza o recorte ou tente
mais tarde; `--endpoint` permite informar outro endpoint Overpass compatível.
Uma resposta com erro não deve ser usada como mapa.

O gerador recusa menos de 100 trechos, mais de 350 mil pontos, mais de 65.536
trechos ou coordenadas fora do formato. Se houver erro, corrija a causa antes
de compilar; não considere as saídas de uma execução incompleta como válidas.

## 4. Escolher o que fazer com números de imóveis

### Opção A: somente ruas, sem busca de números

É o caminho mais simples para uma região nova, inclusive fora do Brasil.
Substitua o conteúdo de `src/core/map_addresses.cpp` pelo módulo abaixo.
Ele mantém a busca de ruas e trata consultas com vírgula como sem estimativa:

```cpp
#include "pda/map.h"

namespace pda { namespace maps {
std::string addressStreetName(uint32_t) { return {}; }
bool searchApproximateAddresses(const std::string &query, Coord,
                               std::vector<AddressResult> &results, size_t) {
    results.clear();
    return query.find(',') != std::string::npos;
}
bool validateAddressData() { return true; }
} }
```

O arquivo `map_addresses.inc` deixa de ser compilado por esse módulo. Ajuste
a ajuda e o exemplo de busca em `src/apps/map.cpp` para informar que essa
versão pesquisa somente ruas, removendo a indicação de numeração IBGE.
Guarde a versão original no Git para poder restaurá-la depois.

### Opção B: gerar a numeração de outro recorte no Brasil

Este fluxo é mais avançado. Os scripts atuais estão configurados para o
município de São Paulo; não escolhem outro município automaticamente.

1. Obtenha o ZIP GeoJSON municipal do IBGE CNEFE no formato esperado pelo
   script. Em `tools/maps/analyze_addresses.py`, atualize `SOURCE_URL`, o
   município indicado nos metadados e, se necessário, a leitura dos campos.
   Passe o arquivo local com `--source`. O script não baixa esse ZIP sozinho.
2. Com `BBOX` e `ORIGIN` já ajustados e uma compilação disponível para a
   medição de memória, execute:

   ```powershell
   python tools/maps/analyze_addresses.py --source build/maps/cnefe-meu-municipio.zip
   python tools/maps/analyze_interpolation.py --tolerances 50
   python tools/maps/analyze_interpolation.py --export build/maps/pinheiros-interpolation-50m.bin
   ```

3. O último comando substitui `src/core/map_addresses.inc`. Os caminhos de
   saída continuam com o prefixo histórico `pinheiros`. Atualize também os
   textos de município/fonte nos relatórios gerados e em
   `tools/maps/analyze_interpolation.py`; há descrições fixas de São Paulo.
4. Recompile e confira ruas, números, limites, pares/ímpares e consumo de flash.
   O recorte pode atravessar municípios, mas o script atual processa um ZIP
   municipal por execução; unir bases exige adaptar o gerador.

Não reutilize um binário de endereços gerado com outra origem ou caixa.
A estimativa continua aproximada e não comprova a entrada de um imóvel.
Fora do Brasil, outra fonte exige adaptar o importador; o fluxo IBGE não cobre
essa situação. A opção A dispensa essa adaptação.

## 5. Compilar, conferir e gravar

```powershell
pio run -e tdeck
```

Confira o tamanho final: a partição atual do aplicativo tem 6.553.600 bytes.
Se exceder, reduza a área ou reveja a base de numeração. Não aumente a partição
sem planejar a migração dos dados do aparelho.

Os testes e o preview atuais contêm ruas, coordenadas GPS simuladas e contagens
de referência de Pinheiros. Ajuste essas expectativas para o recorte novo antes
de executá-los; falhas nesses exemplos não significam, sozinhas, que o mapa novo
está incorreto. Preserve as verificações de índices, limites e projeção.

Confira no preview a vista inicial, nomes, busca, zoom, movimento e a posição
GPS simulada. Depois de validar a compilação, para um T-Deck que já usa o layout
de partições do PDA-OS:

```powershell
./tools/flash.ps1 -Port COM4 -SkipBuild
```

Troque `COM4` pela porta real. Esse script atualiza somente o aplicativo;
não é um instalador inicial de bootloader e partições. No mapa, pressione **C**
após a atualização para substituir a vista antiga salva pela nova vista inicial.
Confira o acompanhamento com GPS real quando houver sinal dentro da região.

Preserve a atribuição **© OpenStreetMap contributors**, a referência ODbL e as
licenças de quaisquer outras fontes usadas. Ao publicar sua variante, revise
também o README e as imagens para identificar a região que ela contém.
