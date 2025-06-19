# ExploradorVHDX

Ferramenta desenvolvida na disciplina de Sistemas Operacionais para criar, montar e explorar discos virtuais VHDX no Windows.

## Estrutura do Projeto

* **VHDXNativeLib/**: Biblioteca C++ que encapsula a Windows Virtual Disk API.
* **ExploradorVHDX/**: Aplicação de console em C++ que utiliza a VHDXNativeLib.
* **x64/**: Artefatos de build para plataforma x64.
* **ExploradorVHDX.sln**: Solução do Visual Studio.

## Requisitos

* Windows 10 ou superior.
* Visual Studio 2019 ou posterior com workload C++.
* SDK do Windows com Virtual Disk API.
* Permissões administrativas para montar discos virtuais.

## Como Compilar

1. Abra o `ExploradorVHDX.sln` no Visual Studio.
2. Selecione a configuração **Release** e **x64**.
3. Compile a solução.

Ou via linha de comando:

```powershell
msbuild ExploradorVHDX.sln /p:Configuration=Release /p:Platform=x64
```

## Uso

1. Execute como administrador:

```powershell
ExploradorVHDX.exe
```

2. O programa permite você escolher a pasta onde estão os VHDX e escolher entre eles.
3. No menu interativo:

   * `[1]` Mudar diretório
   * `[2]` Listar arquivos e pastas
   * `[3]` Criar arquivo
   * `[4]` Criar pasta
   * `[0]` Sair
