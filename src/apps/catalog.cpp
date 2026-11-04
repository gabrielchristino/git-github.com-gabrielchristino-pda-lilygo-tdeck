#include "pda/catalog.h"
namespace pda {
const AppInfo apps[] = {
    {AppId::Agenda, "Agenda", "Compromissos e lembretes"},
    {AppId::Calendar, "Calendário", "Vista mensal da agenda"},
    {AppId::Notes, "Notas", "Texto com salvamento automático"},
    {AppId::Draw, "Desenho", "Caneta, borracha e arquivos"},
    {AppId::Clock, "Relógio", "Hora, cronômetro e temporizador"},
    {AppId::Contacts, "Contatos", "Nome, telefone, email e notas"},
    {AppId::Calculator, "Calculadora", "Expressões e parênteses"},
    {AppId::Sheet, "Planilha", "Grade A1:Z100 com fórmulas"},
    {AppId::InputTest, "Diagnóstico", "Bateria, GPS e entradas"},
    {AppId::Settings, "Ajustes", "Memória, brilho, energia e SD"},
    {AppId::Games, "Jogos", "10 jogos e puzzles pelo teclado"},
    {AppId::Map, "Mapa", "Marginal Pinheiros / vetores offline"},
    {AppId::Bluetooth, "Bluetooth", "Teclado e mouse para PC ou celular"},
};
const size_t appCount = sizeof(apps) / sizeof(apps[0]);
}
