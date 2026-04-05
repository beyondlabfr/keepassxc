# Release GitHub Actions — secrets et signature

Le workflow [`.github/workflows/release.yml`](../.github/workflows/release.yml) se lance **manuellement** (*Actions → Release (manual, multi-platform, signed) → Run workflow*).

## Entrées du workflow

| Champ | Rôle |
|--------|------|
| `version` | Passé à CMake comme `-DOVERRIDE_VERSION` (ex. `2.8.0-fork.1`). |
| `tag_name` | Tag de la release GitHub. Vide → tag auto `release-<version>` (`/` et espaces remplacés par `_`). |
| `draft` / `prerelease` | Comportement GitHub Release. |
| `signing_strict` | Si activé, échec immédiat si un type de signature est demandé mais les secrets manquent. |
| `sign_windows` / `sign_macos` / `sign_linux_gpg` | Désactiver une plateforme pour ne pas signer (ou build sans certificats si secrets absents). |

## Secrets du dépôt (Settings → Secrets and variables → Actions)

### Windows (Authenticode)

| Secret | Description |
|--------|-------------|
| `WINDOWS_CODESIGN_PFX_BASE64` | Certificat `.pfx` encodé en base64 (une ligne). |
| `WINDOWS_CODESIGN_PFX_PASSWORD` | Mot de passe du PFX. |
| `WINDOWS_CODESIGN_SHA1_THUMBPRINT` | *(Optionnel)* Empreinte SHA1 du certificat pour `signtool /sha1`. Si vide, le workflow utilise `auto` (certificat importé dans le store utilisateur). |

Le build utilise `signtool` comme dans `cmake/WindowsCodesign.cmake.in` (étape CPack avant l’empaquetage ZIP).

### macOS (Developer ID + notarisation)

| Secret | Description |
|--------|-------------|
| `MACOS_CODESIGN_P12_BASE64` | Export PKCS#12 du certificat **Developer ID Application** (+ chaîne si besoin), en base64. |
| `MACOS_CODESIGN_P12_PASSWORD` | Mot de passe du P12. |
| `MACOS_CODESIGN_IDENTITY_NAME` | Nom exact du certificat, ex. `Developer ID Application: Votre Nom (TEAMID)`. |
| `APPLE_NOTARY_APPLE_ID` | Apple ID du compte développeur. |
| `APPLE_NOTARY_TEAM_ID` | Identifiant d’équipe (10 caractères). |
| `APPLE_NOTARY_APP_SPECIFIC_PASSWORD` | [Mot de passe spécifique à l’app](https://support.apple.com/en-us/102654) pour `notarytool`. |

Le workflow enregistre un profil keychain `kpxc-gh-notary` puis passe `-DWITH_XC_NOTARY_KEYCHAIN_PROFILE=kpxc-gh-notary` à CMake (voir `cmake/MacOSCodesign.cmake.in`).

### Linux (GPG sur l’archive `.tar.gz`)

| Secret | Description |
|--------|-------------|
| `GPG_RELEASE_PRIVATE_KEY` | Clé privée ASCII-armorée (bloc `BEGIN PGP PRIVATE KEY`). |
| `GPG_RELEASE_KEY_ID` | ID de clé pour `-u` (ex. empreinte courte ou email). |
| `GPG_RELEASE_PASSPHRASE` | *(Optionnel)* Phrase secrète si la clé est protégée. |

## Permissions

Le job `publish` utilise `GITHUB_TOKEN` avec `contents: write` pour créer la release et uploader les fichiers. Pour un dépôt sous **environment** protégé, vous pouvez restreindre le workflow à un environnement `release` (à ajouter dans le YAML si besoin).

## Limites / bonnes pratiques

- Les builds **vcpkg** (Windows, macOS) sont longs ; prévoir le cache vcpkg dans une évolution future (`actions/cache`).
- Premier lancement : tester avec `draft: true` et `signing_strict: false` pour valider les builds sans tous les secrets.
- Si le tag existe déjà, `softprops/action-gh-release` peut échouer : supprimer le tag ou changer `tag_name` / `version`.
