#include "mainwindow.h"
#include <QApplication>
#include <QSettings>      // Para verificar o Registro do Windows
#include <QMessageBox>    // Para mostrar mensagens ao usuário
#include <QProcess>       // Para executar o instalador
#include <Windows.h>      // Para a API do Windows (elevação de privilégios)
#include <shellapi.h>     // Para a API do Windows (elevação de privilégios)
#include <Shlobj.h>

// Função para verificar se o driver ViGEmBus está instalado
bool isViGEmBusInstalled()
{
    // A maneira mais confiável de verificar é procurar pela chave de serviço no Registro do Windows.
    QSettings settings("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\ViGEmBus", QSettings::NativeFormat);
    return settings.childGroups().contains("Parameters", Qt::CaseInsensitive);
}

// Função para se re-executar como administrador
void runAsAdmin(int argc, char* argv[])
{
    SHELLEXECUTEINFO sei = { sizeof(sei) };
    sei.lpVerb = L"runas"; // O "verbo" para pedir elevação
    sei.lpFile = L"GamePadVirtual-Desktop.exe"; // O nome do seu executável
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;

    if (!ShellExecuteEx(&sei))
    {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            // O usuário clicou em "Não" na janela de permissão
            QMessageBox::warning(nullptr, "Permissão Negada", "A permissão de administrador é necessária para instalar o driver. O programa será fechado.");
        }
        else {
            QMessageBox::critical(nullptr, "Erro de Elevação", QString("Falha ao tentar obter privilégios de administrador. Erro: %1").arg(error));
        }
    }
}


int main(int argc, char* argv[])
{
    // PASSO 1: Verificar se o driver está instalado
    if (!isViGEmBusInstalled()) {

        // Se o programa já está rodando como admin, significa que ele foi re-executado.
        // É hora de instalar o driver.
        if (IsUserAnAdmin()) {
            QMessageBox::information(nullptr, "Instalação do Driver", "O instalador do driver ViGEmBus será iniciado agora. Por favor, siga as instruções.");

            // Executa o instalador do ViGEmBus
            // O ideal é que o 'ViGEmBus_1.22.0_x64_x86_arm64.exe' esteja na mesma pasta que o .exe
            QProcess installer;
            installer.start("ViGEmBus_1.22.0_x64_x86_arm64.exe", QStringList() << "/S");

            if (installer.waitForFinished(-1)) {
                QMessageBox::information(nullptr, "Instalação Concluída", "O driver foi instalado com sucesso! Por favor, execute o programa novamente.");
            }
            else {
                QMessageBox::critical(nullptr, "Falha na Instalação", "Ocorreu um erro ao instalar o driver ViGEmBus.");
            }
            return 0; // Fecha o programa após a tentativa de instalação
        }
        else {
            // Se não estamos como admin, pedimos permissão ao usuário
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(nullptr, "Driver Necessário",
                "O driver 'ViGEmBus' é necessário para que o programa funcione. "
                "Deseja instalá-lo agora? (Requer permissão de administrador)",
                QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                runAsAdmin(argc, argv);
            }
            else {
                QMessageBox::warning(nullptr, "Operação Cancelada", "O programa não pode funcionar sem o driver e será fechado.");
            }
            return 0; // Fecha o programa
        }
    }

    // PASSO 2: Se o driver já está instalado, executa o programa normalmente
    QApplication a(argc, argv);

    // O ícone é carregado a partir do arquivo .rc, então não precisamos mais de código para isso aqui.

    MainWindow w;
    w.show();
    return a.exec();
}