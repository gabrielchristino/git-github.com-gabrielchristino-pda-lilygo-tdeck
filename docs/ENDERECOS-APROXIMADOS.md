# Busca offline de número aproximado

Abra **B** ou **Menu > Buscar** e digite `rua, numero`, por exemplo
`nacoes unidas, 12501`. Se houver uma faixa conhecida, a lista mostra até três
alternativas, com número e CEP. Enter ou toque abre o resultado. O mapa mostra
um **X laranja** e **Endereco aproximado**. Não depende de Wi-Fi, SD ou GPS.

Somente o nome da rua mantém a busca original de vias. C retorna ao centro
inicial e remove o alvo. Seguir um GPS válido por F também remove o alvo.
O marcador de GPS continua azul e exige posição real; o alvo laranja nunca
representa a localização do aparelho. O alvo é transitório; a vista é salva.

## Como é calculado

São usados números conhecidos do IBGE CNEFE 2022, edição GeoJSON municipal
de setembro de 2024, dentro da caixa existente do mapa e no município de
São Paulo. Só entram coordenadas originais/modificadas (níveis 1 e 2) e
números inteiros positivos de até 65.535. Não há cobertura garantida de todos
os números, imóveis sem número, complementos, letras ou municípios vizinhos.

As amostras são agrupadas por rua, CEP e paridade. Saltos espaciais grandes
e lacunas de mais de 600 números dividem as cadeias antes de reduzir pontos.
A redução conserva pontos de apoio onde a interpolação linear em função do
número se afastaria mais de 50 metros dos pontos usados para ajustá-la.
Isso permite acompanhar curvas e mudanças de numeração sem guardar todos os
endereços. **50 metros é o parâmetro de ajuste, não uma garantia de erro.**

Entre os apoios `(n0,x0,y0)` e `(n1,x1,y1)`, o número solicitado `n` usa
`t=(n-n0)/(n1-n0)` e `(x,y)=(x0,y0)+t*((x1,y1)-(x0,y0))`.
O firmware usa divisão inteira com produto intermediário de 64 bits.
Só usa a mesma paridade e o intervalo observado, sem extrapolação. Se não há
faixa, a interface informa que não foi possível estimar e permite buscar a rua.

Não basta ligar as duas extremidades de toda uma rua: ruas com curvas, trechos
desconectados, nomes iguais e numeração irregular exigem mais apoios.
Resultados em CEPs diferentes continuam separados para o usuário escolher;
o CEP exibido ajuda a distinguir alternativas, mas não é um campo de busca.
Na comparação do nome, de/do/da/dos/das são ignorados como palavras inteiras,
para aceitar grafias diferentes entre o mapa e o cadastro. O nome original de
cada fonte continua preservado.

## Tamanho medido

Os números abaixo registram a versão anterior à compactação de 03/11/2026.
As quantidades de ruas, faixas e apoios e o binário intermediário foram
preservados. Os tamanhos atuais na flash e a validação sem perda estão em
[Uso de memória](MEMORIA.md).

- 864.754 endereços numéricos utilizáveis após filtros e deduplicação.
- 20.846 nomes no índice (alguns sem faixa numérica utilizável).
- 70.182 cadeias e 223.204 apoios.
- Índice de ruas: 250.152 bytes; nomes: 446.255; cadeias: 561.456;
  tabela de CEPs: 77.192; apoios: 1.339.224.
- Binário intermediário, incluindo cabeçalho: **2.674.319 bytes**.
- Firmware completo compilado: **6.289.360 bytes**, dentro da partição de
  6.553.600 bytes; margem de **264.240 bytes**. Sem alteração de partições.

As tabelas ficam na flash e são consultadas diretamente. Não há imagens,
descompressão de uma base inteira ou cópia de todos os apoios para a RAM.

## Validação e precisão

Antes de ajustar os apoios, 172.927 números distintos foram separados
deterministicamente para validação (aproximadamente 20%). As cadeias e apoios
de teste foram ajustados apenas nos outros 691.827 endereços.

| Resultado dos endereços separados para teste | Quantidade |
| --- | ---: |
| Uma única resposta sem precisar informar CEP | 134.303 (77,66%) |
| Mais de uma faixa possível sem CEP | 14.976 |
| Fora das faixas ajustadas, sem extrapolação | 23.648 |

Nos 134.303 casos com uma única resposta: erro mediano de **14,3 m**, percentil
90 de **54,8 m**, percentil 95 de **100,6 m** e percentil 99 de **352,6 m**.
94,95% ficaram a até 100 m do ponto de referência. Esses números comparam
interpolação com **coordenadas do cadastro**, não com portas verificadas em
campo. Não são garantia de posição exata, atualidade ou existência do imóvel.
O modelo final usa todas as amostras utilizáveis após essa avaliação.

O cadastro pode conter erros, variantes de nomes e coordenadas de apartamentos
modificadas. A base anterior reunia apartamentos por rua/número/CEP; agrupamentos
com coordenadas dispersas ainda podem afetar a qualidade. A consulta deve
permanecer identificada como aproximada, inclusive nos próprios pontos de apoio.

Os testes C++ verificam índices, nomes, todas as faixas/apoios, paridade,
monotonicidade, interpolação dentro do segmento e recusa de extrapolação.
Um exemplo real foi comparado com a conta independente em Python. O preview
LVGL verifica digitação, resultado por toque, marcador, ausência de faixa e
preservação da vista quando não há estimativa. Vinte coordenadas na borda foram
limitadas em 1 metro para compatibilizar o arredondamento com os limites do mapa.

Instalado no T-Deck COM4, somente o aplicativo em 0x10000, com hash verificado.
`nacoes unidas, 12501` respondeu em 0,59–0,61 s pelo USB e centralizou em
`x=285, y=-3534, zoom=3`, consistente com a referência Python após arredondamento
inteiro. `hebe camargo, 65000` não alterou a vista. Cancelamento, busca original
de rua, retorno ao centro e salvamento passaram. A nota e o compromisso já
existentes permaneceram presentes. A tela final foi deixada no exemplo
aproximado, com GPS ainda sem posição válida.

## Reprodução

Usa a análise CNEFE já existente em `build/maps/`:

```powershell
& C:/Users/gabri/.platformio/penv/Scripts/python.exe tools/maps/analyze_interpolation.py --tolerances 50
& C:/Users/gabri/.platformio/penv/Scripts/python.exe tools/maps/analyze_interpolation.py --export build/maps/pinheiros-interpolation-50m.bin
./tests/run.ps1
```

Métricas e SHA-256: `data/maps/pinheiros-interpolation-analysis.json`.
Fonte primária:
<https://ftp.ibge.gov.br/Cadastro_Nacional_de_Enderecos_para_Fins_Estatisticos/Censo_Demografico_2022/Arquivos_CNEFE/GeoJSON/Municipio_20240910/>.
