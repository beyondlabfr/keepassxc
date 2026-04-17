# Build documentation

## Windows (MSVC + Ninja + vcpkg)

Pré-requis :
- Visual Studio (MSVC) + CMake + Ninja
- vcpkg (par défaut `C:\vcpkg`)
- (optionnel) WiX Toolset (pour générer un MSI)

### Build simple (exe dans le dossier de build)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Sortie :
- `.\build\src\KeePassXC.exe`
- `.\build\src\cli\keepassxc-cli.exe`
- `.\build\src\proxy\keepassxc-proxy.exe`

### Build en mode "Release" KeePassXC (désactive les warnings Snapshot dans l'app)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 -ReleaseMode
```

ou explicitement :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 -KeePassXCBuildType Release
```

### Clean rebuild

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 -Clean
```

### Packager un ZIP portable (avec DLL/Qt plugins)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 -Package
```

Sortie :
- ZIP : `.\build\KeePassXC-*.zip`
- staging CPack : `.\build\_CPack_Packages\win64\ZIP\KeePassXC-*\KeePassXC.exe`

### Packager ZIP + MSI (WiX requis)

Installe WiX Toolset (candle/light) et assure-toi que `candle.exe` et `light.exe` sont dans le `PATH`.
Ensuite :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 -Package
```

Sortie :
- ZIP : `.\build\KeePassXC-*.zip`
- MSI : `.\build\KeePassXC-*.msi`

### Options utiles

- **Changer le dossier de build** :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 -BuildDir build-rel
```

- **Changer le type CMake** :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 -BuildType RelWithDebInfo
```

- **Changer vcpkg** :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 -VcpkgRoot C:\vcpkg
```

- **Changer le niveau de parallélisme** :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1 -Jobs 12
```

## Ubuntu / Debian (.deb)

### Pré-requis (exemples Ubuntu/Debian)

Le script `./scripts/build-debian-package.sh` dépend du contenu exact du script et de ta distro, mais en général il te faut :
- **toolchain** : `build-essential`, `cmake`, `ninja-build` (ou `make`), `pkg-config`
- **Qt** : Qt5 (dev) et ses outils (ou Qt6 si tu adaptes le projet)
- **autres** : `git`, `asciidoctor` (pour la doc), etc.

Sur une machine neuve, commence par lire/ouvrir le script pour voir la liste exacte des paquets attendus, puis adapte selon ta version d’Ubuntu/Debian.

### Build “dev” (sans .deb)

Depuis la racine du repo :

```bash
mkdir -p build
cd build
cmake -DWITH_XC_ALL=ON -DCMAKE_BUILD_TYPE=Release ..
ninja -j"$(nproc)"
```

Exécutable généré (en général) :
- `./build/src/keepassxc` (ou équivalent selon la plateforme)

### Build + tests (optionnel)

```bash
mkdir -p build
cd build
cmake -DWITH_XC_ALL=ON -DWITH_TESTS=ON -DCMAKE_BUILD_TYPE=Release ..
ninja -j"$(nproc)"
ctest --output-on-failure
```

### Générer un .deb (script)

```bash
./scripts/build-debian-package.sh
```

Selon le script, le `.deb` se retrouvera dans un sous-répertoire de build/output (souvent `build/` ou un dossier `release/`).

### Alternative : release-tool.py (multi-plateformes)

Le dépôt inclut un `release-tool.py` qui automatise build + packaging (utile si tu veux reproduire la CI).
Exemple (aide) :

```bash
python3 release-tool.py build -h
```