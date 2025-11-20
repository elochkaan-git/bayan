# Bayan

Программа для поиска файлов-дубликатов

## Установка зависимостей
Для Arch Linux

```sh
sudo pacman -Syu
sudo pacman -S boost
```

Для Ubuntu

```sh
sudo apt-get update
sudo apt-get install libboost-all-dev
```

Для Windows (через vcpkg)

```sh
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install boost-program-options boost-filesystem --triplet x64-windows
```

## Сборка проекта
Linux

```sh
git clone https://github.com/elochkaan-git/bayan.git
cd bayan
cmake -B build -G "Unix Makefiles"
cmake --build build --config Release
```

Windows

```sh
git clone https://github.com/elochkaan-git/bayan.git
cd bayan
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```