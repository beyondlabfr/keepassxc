# Build documentation

## Windows (MSVC + Ninja + vcpkg)

Pré-requis (machine de référence : win-compile, voir homelab-docs `machines/win-compile.md`) :
- Visual Studio (MSVC, outils C++ et CMake) ; Ninja est celui de Visual Studio
- vcpkg à jour (par défaut `C:\vcpkg`) : il compile Qt6, QtKeychain et les autres dépendances, en release seulement
  (triplet `x64-windows-release`) ; la première compilation prend environ 2 h 30
- WiX Toolset **v3** (`candle`/`light`, ex. binaires 3.14 dans `C:\Tools\wix314`) pour le MSI
- Ruby + `gem install asciidoctor` pour la documentation hors ligne : **le MSI en a besoin** (raccourcis du menu
  Démarrer vers le guide) ; sans asciidoctor, le script compile sans documentation et seul le ZIP sort

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

### Pré-requis (Ubuntu 24.04)

Depuis la fusion de l'upstream d'octobre 2026, KeePassXC est en **Qt6** (6.2.4 minimum). Le plus simple est
`INSTALL_DEPS=1 ./scripts/build-debian-package.sh`, qui installe exactement les paquets attendus, ou de compiler
dans un conteneur `ubuntu:24.04` pour ne rien installer sur la machine.

- **toolchain** : `build-essential`, `cmake`, `ninja-build`, `pkg-config`
- **Qt6** : `qt6-base-dev qt6-base-private-dev qt6-tools-dev qt6-tools-dev-tools qt6-base-dev-tools qt6-svg-dev qt6-5compat-dev qt6-l10n-tools`
- **WebDAV** : `qtkeychain-qt6-dev` (optionnel : sans lui, le mot de passe WebDAV ne peut pas être mémorisé)
- **autres** : `libbotan-2-dev libargon2-dev libminizip-dev zlib1g-dev libqrencode-dev libreadline-dev libkeyutils-dev libpcsclite-dev libusb-1.0-0-dev libxi-dev libxtst-dev libssl-dev`

### Build “dev” (sans .deb)

Depuis la racine du repo (toutes les fonctions sont activées par défaut, options `KPXC_FEATURE_*`) :

```bash
mkdir -p build
cd build
cmake -DKPXC_FEATURE_DOCS=OFF -DCMAKE_BUILD_TYPE=Release ..
ninja -j"$(nproc)"
```

Exécutable généré (en général) :
- `./build/src/keepassxc` (ou équivalent selon la plateforme)

### Build + tests (optionnel)

```bash
mkdir -p build
cd build
cmake -DKPXC_FEATURE_DOCS=OFF -DWITH_TESTS=ON -DCMAKE_BUILD_TYPE=Release ..
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

## WebDAV (ajout du fork)

Menu **Base de données > Ouvrir une base WebDAV**, ou une URL `https://…` / `webdavs://…` dans les bases récentes.
Le comportement suit celui de KeePass 2 :

- **Ouverture directe** depuis le serveur, sans copie locale.
- **Enregistrement** : si la base a changé sur le serveur depuis son ouverture (ETag, ou empreinte du début du
  fichier quand le serveur ne donne pas d'ETag fiable), KeePassXC propose la même question que pour un fichier
  local modifié : *fusionner puis enregistrer*, *écraser*, *abandonner mes changements* ou *annuler*.
- **Écriture sûre** : la base est envoyée dans `base.kdbx.<aléa>.tmp` puis renommée sur le serveur (`MOVE`) ;
  un envoi raté ne laisse donc jamais de base tronquée. Si le serveur refuse le renommage, envoi direct. Seul le
  mode « écriture directe » des réglages d'enregistrement envoie directement (avec `If-Match` quand le serveur
  donne un ETag fiable).
- **Identifiants** : *Mémoriser* = rien, l'identifiant, ou l'identifiant et le mot de passe. Le mot de passe va
  dans le **trousseau du système** (Secret Service / KWallet, Gestionnaire d'identification Windows, Trousseau
  macOS) via QtKeychain, jamais dans `keepassxc.ini`. Les mots de passe laissés en clair dans `keepassxc.ini`
  par les anciennes versions du fork sont déplacés vers le trousseau (ou effacés s'il n'y en a pas) au premier
  lancement. Pendant la session, ils restent en mémoire pour le verrouillage et le rechargement.
- Si l'intégration Secret Service de KeePassXC est activée, il est lui-même le trousseau : le mot de passe WebDAV
  n'est alors pas mémorisé d'une session à l'autre.
- Une adresse `http://` (ou `webdav://`) avec mot de passe déclenche un avertissement : le mot de passe circule
  en clair.

### Tests WebDAV

`tests/TestWebDav.cpp` tourne contre un vrai serveur Apache (Docker, configuration `tests/webdav/httpd.conf`) :
ETag fort, sans ETag, `MOVE` refusé, réponses gzip, redirection, mauvais mot de passe, modification concurrente.

```bash
scripts/test-webdav.sh build/tests/testwebdav
```

Sans serveur (`KPXC_TEST_WEBDAV_URL` absent), le test est sauté par `ctest`.

