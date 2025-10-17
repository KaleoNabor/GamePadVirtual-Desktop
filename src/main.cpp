#include "mainwindow.h"
#include <QApplication>
#include <QSettings>
#include <QMessageBox>
#include <QProcess>
#include <Windows.h>
#include <shellapi.h>
#include <Shlobj.h>

// Verifica se o driver ViGEmBus est� instalado
bool isViGEmBusInstalled()
{
    QSettings settings("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\ViGEmBus", QSettings::NativeFormat);
    return settings.childGroups().contains("Parameters", Qt::CaseInsensitive);
}

// Executa o programa como administrador
void runAsAdmin(int argc, char* argv[])
{
    SHELLEXECUTEINFO sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = L"GamePadVirtual-Desktop.exe";
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;

    if (!ShellExecuteEx(&sei))
    {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            QMessageBox::warning(nullptr, "Permiss�o Negada", "A permiss�o de administrador � necess�ria para instalar o driver. O programa ser� fechado.");
        }
        else {
            QMessageBox::critical(nullptr, "Erro de Eleva��o", QString("Falha ao tentar obter privil�gios de administrador. Erro: %1").arg(error));
        }
    }
}

int main(int argc, char* argv[])
{
    // Verifica��o da instala��o do driver ViGEmBus
    if (!isViGEmBusInstalled()) {

        // Se j� est� executando como administrador, instala o driver
        if (IsUserAnAdmin()) {
            QMessageBox::information(nullptr, "Instala��o do Driver", "O instalador do driver ViGEmBus ser� iniciado agora. Por favor, siga as instru��es.");

            // Execu��o do instalador do ViGEmBus
            QProcess installer;
            installer.start("ViGEmBus_1.22.0_x64_x86_arm64.exe", QStringList() << "/S");

            if (installer.waitForFinished(-1)) {
                QMessageBox::information(nullptr, "Instala��o Conclu�da", "O driver foi instalado com sucesso! Por favor, execute o programa novamente.");
            }
            else {
                QMessageBox::critical(nullptr, "Falha na Instala��o", "Ocorreu um erro ao instalar o driver ViGEmBus.");
            }
            return 0;
        }
        else {
            // Solicita��o de permiss�o de administrador para instala��o
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(nullptr, "Driver Necess�rio",
                "O driver 'ViGEmBus' � necess�rio para que o programa funcione. "
                "Deseja instal�-lo agora? (Requer permiss�o de administrador)",
                QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                runAsAdmin(argc, argv);
            }
            else {
                QMessageBox::warning(nullptr, "Opera��o Cancelada", "O programa n�o pode funcionar sem o driver e ser� fechado.");
            }
            return 0;
        }
    }

    // Execu��o normal do programa se o driver estiver instalado
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}