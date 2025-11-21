#include "mainwindow.h"
#include <QApplication>
#include <QSettings>
#include <QMessageBox>
#include <QProcess>
#include <Windows.h>
#include <shellapi.h>
#include <Shlobj.h>
#include <stdlib.h>

// Verifica se o driver ViGEmBus está instalado
bool isViGEmBusInstalled()
{
    QSettings settings("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\ViGEmBus", QSettings::NativeFormat);
    return settings.childGroups().contains("Parameters", Qt::CaseInsensitive);
}

// Executa o programa como administrador
void runAsAdmin(int argc, char* argv[])
{
    // Use SHELLEXECUTEINFOW explicitamente para aceitar strings com L"..."
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = L"GamePadVirtual-Desktop.exe";
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;

    // Use ShellExecuteExW explicitamente
    if (!ShellExecuteExW(&sei))
    {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            QMessageBox::warning(nullptr, "Permissão Negada", "A permissão de administrador é necessária para instalar o driver. O programa será fechado.");
        }
        else {
            QMessageBox::critical(nullptr, "Erro de Elevação", QString("Falha ao tentar obter privilégios de administrador. Erro: %1").arg(error));
        }
    }
}

int main(int argc, char* argv[])
{
    qputenv("GST_PLUGIN_PATH", "C:\\Program Files\\gstreamer\\1.0\\msvc_x86_64\\lib\\gstreamer-1.0");
    // Verificação da instalação do driver ViGEmBus
    if (!isViGEmBusInstalled()) {

        // Se já está executando como administrador, instala o driver
        if (IsUserAnAdmin()) {
            QMessageBox::information(nullptr, "Instalação do Driver", "O instalador do driver ViGEmBus será iniciado agora. Por favor, siga as instruções.");

            // Execução do instalador do ViGEmBus
            QProcess installer;
            installer.start("ViGEmBus_1.22.0_x64_x86_arm64.exe", QStringList() << "/S");

            if (installer.waitForFinished(-1)) {
                QMessageBox::information(nullptr, "Instalação Concluída", "O driver foi instalado com sucesso! Por favor, execute o programa novamente.");
            }
            else {
                QMessageBox::critical(nullptr, "Falha na Instalação", "Ocorreu um erro ao instalar o driver ViGEmBus.");
            }
            return 0;
        }
        else {
            // Solicitação de permissão de administrador para instalação
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
            return 0;
        }
    }

    // Execução normal do programa se o driver estiver instalado
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}