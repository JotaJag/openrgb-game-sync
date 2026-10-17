# Compilar y continuar el desarrollo

Clona el repositorio en una carpeta llamada `openrgb-game-sync`.
La compilación conserva la estructura del prototipo: los dos repositorios de
dependencias se preparan como carpetas hermanas. No hace falta ejecutar el
instalador para compilar o ejecutar pruebas sin hardware.

Necesitas Git, Python 3.9+, Qt6 Core/Gui/Widgets/Network con qmake6, g++, make,
pkg-config, clang, lld y tar con soporte zstd. Effects también utiliza las
dependencias declaradas por su propio archivo `.pro`. Instala los paquetes de
desarrollo correspondientes a tu distribución. El toolchain MinGW se descarga
por separado; no se sube a Git.

```bash
bash scripts/bootstrap.sh
python3 scripts/setup-toolchain.py

mkdir -p ../openrgb-sequence-fix/build
cd ../openrgb-sequence-fix/build
qmake6 ../OpenRGBEffectsPlugin.pro
make -j4
cd ../../openrgb-game-sync

mkdir -p build
cd build
qmake6 ../GameSync.pro
make -j4
cd ..
bash bridge/build.sh
bash pipe_service/build.sh
bash tests/build.sh
```

Pruebas que no necesitan acceso a juegos:

```bash
bash tests/host-build.sh
python3 tests/chroma_compat_test.py
python3 tests/rest_configuration_test.py
python3 tests/installer_test.py
python3 tests/pipe_service_test.py
python3 tests/hardening_test.py
python3 tests/automation_test.py
python3 tests/rest_test.py
QT_QPA_PLATFORM=offscreen build/integration-test
python3 tests/wine_smoke.py
python3 tests/sdk_wine_smoke.py
```

Las pruebas Wine necesitan Wine y los adaptadores compilados. Algunas rutas
de integración nativa comprueban opcionalmente un fixture de la DLL original.
Consulta el código de `pipe_lab/` para preparar ese fixture: el desarrollador
debe aportar su propia DLL, no se distribuye. Para las pruebas UMU define
`PROTONPATH` explícitamente. No uses un prefijo de juego real para las pruebas.

Generar paquete instalable:

```bash
python3 scripts/package.py
```

Produce `dist/openrgb-game-sync-<VersionStr>-linux-x86_64.tar.gz` y su SHA-256.
Contiene `install.sh`, `uninstall.sh`, el asistente para configurar un prefijo,
los dos plugins y el runtime necesario. Las bibliotecas se toman de la
compilación local: no se afirma compatibilidad universal entre distribuciones.

## Publicar en tu cuenta

Con GitHub CLI autenticado y un primer commit local, ejecuta:

```bash
bash scripts/publish-private.sh
```

Crea **un repositorio privado nuevo** en la cuenta de la sesión de `gh`, sube
`main` y publica una prerelease privada con el paquete. Se detiene si ya existe
el repositorio o un remoto `origin`. No incluye toolchain, compilados, DLL de
juegos, registros ni configuraciones personales dentro del historial Git.

`tools/legacy/` conserva herramientas históricas ligadas al equipo original.
No las uses como instaladores; no forman parte del paquete de release.
