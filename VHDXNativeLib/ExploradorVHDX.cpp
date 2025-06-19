#define NOMINMAX
#define _WIN32_WINNT 0x0602  
#define WINVER        0x0602  

#include <windows.h>
#include <virtdisk.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <io.h>
#include <fcntl.h>
#include <limits>

#pragma comment(lib, "virtdisk.lib")
#pragma comment(lib, "advapi32.lib")

const GUID VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT =
{ 0xEC984AEC, 0xA0F9, 0x47e9, { 0x90, 0x1F, 0x71, 0x41, 0x5A, 0x66, 0x34, 0x5B } };

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return (isAdmin == TRUE);
}

bool AttachVHDX(const std::wstring& path, HANDLE& vhdHandleOut) {
    VIRTUAL_STORAGE_TYPE storageType = { VIRTUAL_STORAGE_TYPE_DEVICE_VHDX, VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT };
    HANDLE vhdHandle = nullptr;
    OPEN_VIRTUAL_DISK_PARAMETERS openParams = {};
    openParams.Version = OPEN_VIRTUAL_DISK_VERSION_1;
    openParams.Version1.RWDepth = OPEN_VIRTUAL_DISK_RW_DEPTH_DEFAULT;

    DWORD result = OpenVirtualDisk(&storageType, path.c_str(), VIRTUAL_DISK_ACCESS_ALL,
        OPEN_VIRTUAL_DISK_FLAG_NONE, &openParams, &vhdHandle);
    if (result != ERROR_SUCCESS) {
        std::wcerr << L"[ERRO] OpenVirtualDisk falhou. Coigo: " << result << std::endl;
        return false;
    }
    ATTACH_VIRTUAL_DISK_PARAMETERS attachParams = {};
    attachParams.Version = ATTACH_VIRTUAL_DISK_VERSION_1;
    result = AttachVirtualDisk(vhdHandle, nullptr, ATTACH_VIRTUAL_DISK_FLAG_NONE, 0, &attachParams, nullptr);
    if (result != ERROR_SUCCESS) {
        std::wcerr << L"[ERRO] AttachVirtualDisk falhou. Codigo: " << result << std::endl;
        CloseHandle(vhdHandle);
        return false;
    }
    vhdHandleOut = vhdHandle;
    std::wcout << L"VHDX montado com sucesso." << std::endl;
    return true;
}

void ListarArquivos(const std::wstring& currentDir, int nivel = 0) {
    namespace fs = std::filesystem;
    fs::path root(currentDir);
    for (const auto& entry : fs::directory_iterator(root)) {
        try {
            std::wcout << std::wstring(nivel * 4, L' ')
                << (fs::is_directory(entry) ? L"[D] " : L"[A] ")
                << entry.path().filename().wstring() << std::endl;
            if (fs::is_directory(entry)) {
                ListarArquivos(entry.path().wstring(), nivel + 1);
            }
        }
        catch (const fs::filesystem_error&) {}
    }
}

void CriarArquivo(const std::wstring& currentDir) {
    std::wcout << L"Nome do novo arquivo (ex: exemplo.txt): ";
    std::wstring nome;
    std::getline(std::wcin, nome);
    std::wstring caminho = currentDir + L"\\" + nome;
    std::wofstream file(caminho);
    if (!file) {
        std::wcerr << L"[ERRO] Nao foi possivel criar: " << caminho << std::endl;
        return;
    }
    std::wcout << L"Conteudo (enter para terminar):\n";
    std::wstring conteudo;
    std::getline(std::wcin, conteudo);
    file << conteudo;
    std::wcout << L"Arquivo criado: " << caminho << std::endl;
}

void CriarPasta(const std::wstring& currentDir) {
    std::wcout << L"Nome da nova pasta: ";
    std::wstring nome;
    std::getline(std::wcin, nome);
    std::wstring caminho = currentDir + L"\\" + nome;
    try {
        if (std::filesystem::create_directory(caminho))
            std::wcout << L"Pasta criada: " << caminho << std::endl;
        else
            std::wcerr << L"[ERRO] Nao foi possivel criar a pasta." << std::endl;
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::wcerr << L"[ERRO] " << e.what() << std::endl;
    }
}

void MudarDiretorio(std::wstring& currentDir) {
    std::wcout << L"[SYSTEM]";
    std::wstring alvo;
    std::getline(std::wcin, alvo);
    namespace fs = std::filesystem;

    if (alvo == L"..") 
    {
        fs::path novo = currentDir;
        currentDir = novo.parent_path();

        std::wcout << L"Diretorio atual: " << currentDir << std::endl;
        return;
    }

    fs::path novo = alvo.front() == L'\\' || alvo.find(L":\\") != std::wstring::npos
        ? fs::path(alvo)
        : fs::path(currentDir) / alvo;
    if (fs::exists(novo) && fs::is_directory(novo)) {
        currentDir = novo.wstring();
        std::wcout << L"Diretorio atual: " << currentDir << std::endl;
    }
    else {
        std::wcerr << L"[ERRO] Diretorio nao encontrado." << std::endl;
    }
}

std::wstring DetectarNovaUnidade(DWORD antes, DWORD depois) {
    for (int i = 0; i < 26; ++i) {
        DWORD mask = (1 << i);
        if (!(antes & mask) && (depois & mask)) {
            wchar_t letra = L'A' + i;
            return std::wstring(1, letra) + L":";
        }
    }
    return L"";
}


int main() {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);

    if (!IsRunningAsAdmin()) {
        std::wcerr << L"Execute como Administrador." << std::endl;
        return 1;
    }

    // Estado inicial das unidades
    DWORD unidadesAntes = GetLogicalDrives();

    // Etapa 1 – Escolher pasta
    std::wstring pasta;
    std::wcout << L"Digite o caminho da pasta onde estao os arquivos VHDX: ";
    std::getline(std::wcin, pasta);

    namespace fs = std::filesystem;
    std::vector<std::wstring> arquivosVHDX;

    try {
        for (const auto& entry : fs::directory_iterator(pasta)) {
            if (entry.path().extension() == L".vhdx") {
                arquivosVHDX.push_back(entry.path().wstring());
            }
        }

        if (arquivosVHDX.empty()) {
            std::wcerr << L"Nenhum arquivo VHDX encontrado na pasta." << std::endl;
            return 1;
        }

        std::wcout << L"\nArquivos VHDX encontrados:\n";
        for (size_t i = 0; i < arquivosVHDX.size(); ++i) {
            std::wcout << L"[" << i << L"] " << arquivosVHDX[i] << std::endl;
        }

        int escolha = -1;
        std::wcout << L"\nDigite o numero do VHDX que deseja montar: ";
        std::wcin >> escolha;
        std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');

        if (escolha < 0 || escolha >= static_cast<int>(arquivosVHDX.size())) {
            std::wcerr << L"Escolha invalida." << std::endl;
            return 1;
        }

        const std::wstring& vhdx = arquivosVHDX[escolha];
        HANDLE handle;
        if (!AttachVHDX(vhdx, handle)) return 1;

        Sleep(1000);
        DWORD unidadesDepois = GetLogicalDrives();
        std::wstring novaUnidade = DetectarNovaUnidade(unidadesAntes, unidadesDepois);
        if (novaUnidade.empty()) {
            std::wcerr << L"[ERRO] Nao foi possivel detectar a unidade montada." << std::endl;
            return 1;
        }

        std::wstring currentDir = novaUnidade;
        std::wcout << L"Unidade montada em: " << currentDir << std::endl;

        int opcao;
        do {
        std::wcout << L"\n[1] Mudar Diretorio  [2] listar [3] criar arquivo [4] criar pasta [0] sair: ";
            std::wcin >> opcao;
            std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
            switch (opcao) {
            case 1: MudarDiretorio(currentDir); break;
            case 2: ListarArquivos(currentDir); break;
            case 3: CriarArquivo(currentDir); break;
            case 4: CriarPasta(currentDir); break;
            case 0: std::wcout << L"Saindo..." << std::endl; break;
            default: std::wcout << L"Opcao invalida." << std::endl; break;
            }
        } while (opcao != 0);
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::wcerr << L"[ERRO] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

