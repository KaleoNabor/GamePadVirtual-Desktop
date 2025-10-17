#include "mainwindow.h"
#include <QApplication>
#include <QSettings>      // Para verificar o Registro do Windows
#include <QMessageBox>    // Para mostrar mensagens ao usu�rio
#include <QProcess>       // Para executar o instalador
#include <Windows.h>      // Para a API do Windows (eleva��o de privil�gios)
#include <shellapi.h>     // Para a API do Windows (eleva��o de privil�gios)
#include <Shlobj.h>

// Fun��o para verificar se o driver ViGEmBus est� instalado
bool isViGEmBusInstalled()
{
    // A maneira mais confi�vel de verificar � procurar pela chave de servi�o no Registro do Windows.
    QSettings settings("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\ViGEmBus", QSettings::NativeFormat);
    return settings.childGroups().contains("Parameters", Qt::CaseInsensitive);
}

// Fun��o para se re-executar como administrador
void runAsAdmin(int argc, char* argv[])
{
    SHELLEXECUTEINFO sei = { sizeof(sei) };
    sei.lpVerb = L"runas"; // O "verbo" para pedir eleva��o
    sei.lpFile = L"GamePadVirtual-Desktop.exe"; // O nome do seu execut�vel
    sei.hwnd = NULL;
    sei.nShow = SW_NORMAL;

    if (!ShellExecuteEx(&sei))
    {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            // O usu�rio clicou em "N�o" na janela de permiss�o
            QMessageBox::warning(nullptr, "Permiss�o Negada", "A permiss�o de administrador � necess�ria para instalar o driver. O programa ser� fechado.");
        }
        else {
            QMessageBox::critical(nullptr, "Erro de Eleva��o", QString("Falha ao tentar obter privil�gios de administrador. Erro: %1").arg(error));
        }
    }
}


int main(int argc, char* argv[])
{
    // PASSO 1: Verificar se o driver est� instalado
    if (!isViGEmBusInstalled()) {

        // Se o programa j� est� rodando como admin, significa que ele foi re-executado.
        // � hora de instalar o driver.
        if (IsUserAnAdmin()) {
            QMessageBox::information(nullptr, "Instala��o do Driver", "O instalador do driver ViGEmBus ser� iniciado agora. Por favor, siga as instru��es.");

            // Executa o instalador do ViGEmBus
            // O ideal � que o 'ViGEmBus_1.22.0_x64_x86_arm64.exe' esteja na mesma pasta que o .exe
            QProcess installer;
            installer.start("ViGEmBus_1.22.0_x64_x86_arm64.exe", QStringList() << "/S");

            if (installer.waitForFinished(-1)) {
                QMessageBox::information(nullptr, "Instala��o Conclu�da", "O driver foi instalado com sucesso! Por favor, execute o programa novamente.");
            }
            else {
                QMessageBox::critical(nullptr, "Falha na Instala��o", "Ocorreu um erro ao instalar o driver ViGEmBus.");
            }
            return 0; // Fecha o programa ap�s a tentativa de instala��o
        }
        else {
            // Se n�o estamos como admin, pedimos permiss�o ao usu�rio
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
            return 0; // Fecha o programa
        }
    }

    // PASSO 2: Se o driver j� est� instalado, executa o programa normalmente
    QApplication a(argc, argv);

    // O �cone � carregado a partir do arquivo .rc, ent�o n�o precisamos mais de c�digo para isso aqui.

    MainWindow w;
    w.show();
    return a.exec();
}