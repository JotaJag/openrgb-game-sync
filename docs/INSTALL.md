# Instalación para usuarios

## Requisitos

Linux x86_64, **OpenRGB 1.0 con Qt6 y Plugin API 5**, Python 3.9 o posterior,
`pgrep` (procps) y `ldd`. El paquete usa las bibliotecas Qt6 del sistema: si
faltan o son incompatibles, el instalador se detiene antes de cambiar plugins.
Flatpak, AppImage con Qt5, Windows y macOS no están validados.
El servicio para juegos requiere además Wine, o Proton con `umu-run`.

## Instalar

Descarga el archivo `openrgb-game-sync-0.5.0-linux-x86_64.tar.gz` de Releases,
extráelo y cierra OpenRGB. Desde la carpeta extraída ejecuta:

```sh
bash install.sh
```

No uses sudo. Abre OpenRGB, habilita **OpenRGB Game Sync** y la versión incluida
de **Effects**, y activa Game Sync en su pestaña. Si usas otra carpeta de
configuración de OpenRGB, pasa `--config-dir /ruta/a/OpenRGB` al instalador.

Se instalan ambos plugins porque comparten la prioridad de renderizado. Los
plugins anteriores y su configuración se respaldan; los binarios duplicados de
Effects/Game Sync se retiran de la carpeta activa. Los perfiles de iluminación
no se modifican. El runtime se guarda en
`~/.local/share/openrgb-game-sync/installer/runtime/`.

Instalar no configura automáticamente todos los juegos. La preparación de
bibliotecas queda pausada: no se reemplazan DLL, se editan launchers ni se
inician juegos. Chroma/Lightsync y otras variantes requieren configuración
específica y no se prometen como compatibles con cualquier juego.

## Configurar el servicio iCUE original

La integración validada es Diablo IV con su DLL Corsair 3.0.234 original,
mediante protocolo 10. Para un prefijo Wine concreto, con OpenRGB cerrado:

```sh
python3 configure-game.py --prefix "$HOME/Games/battlenet"
```

Si ese prefijo usa Proton mediante UMU:

```sh
python3 configure-game.py --prefix "$HOME/Games/battlenet" \
  --proton "/ruta/a/Proton - Experimental"
```

Selecciona el mismo Wine/Proton que utiliza el launcher. Con Wine personalizado
usa `--wine /ruta/al/ejecutable/wine`. Con un prefijo Steam, selecciona su
subcarpeta `pfx`, que contiene `system.reg`. No ejecutes un prefijo de Proton con
otra versión de Wine para probar. El asistente guarda una copia de configuración,
configura un solo prefijo y no toca archivos del juego. La compatibilidad de
otros juegos y SDK requiere una prueba propia.

Abre OpenRGB, activa Game Sync e iCUE, después el launcher y el juego. Las luces
se ceden al juego cuando envía colores y tiene el foco; al salir se devuelve el
control a Effects. Si hay errores, revisa el estado en la pestaña Game Sync y
`~/.local/share/openrgb-game-sync/pipe-service/game.json.log`.

## Desinstalar o actualizar

Cierra OpenRGB y, desde la carpeta del paquete conservada, ejecuta:

```sh
bash uninstall.sh
```

Se restauran los plugins y configuración anteriores. Si un archivo instalado
cambió después, la desinstalación se detiene para no sobrescribir tus cambios.
Conserva primero ese archivo y revisa el manifiesto
`~/.local/share/openrgb-game-sync/installer/installation.json`.
Las copias y el runtime se conservan. Para actualizar, desinstala la versión
gestionada anterior y después instala el nuevo paquete; configura de nuevo el
servicio si lo necesitas. No borres el paquete antes de desinstalar.
