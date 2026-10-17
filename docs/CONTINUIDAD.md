# Estado para continuar el desarrollo

## Punto de partida

Prototipo 0.5.0 para OpenRGB 1.0 / Qt6 / Plugin API 5 en Linux.
El usuario validó visualmente Diablo IV con la DLL Corsair 3.0.234 original:
las luces funcionan y el rendimiento parece correcto después de corregir
latencia del transporte y LED válidos ausentes del teclado virtual.
El incidente y la evolución están en `diablo-incident-20261016.md`.

## Reglas de funcionamiento

- No reactivar la sustitución de DLL para Diablo/Battle.net. El servicio
  independiente usa la DLL original del juego. Conservar el bloqueo existente.
- El plugin Effects necesita `dependencies/effects-game-sync.patch` para
  coordinar la prioridad y corregir los saltos de Sequence.
- La validación con controladores simulados no prueba compatibilidad de un
  juego real. No prometer soporte de cualquier juego/SDK.
- No activar preparación automática de launchers al instalar para terceros.
- `tools/legacy/install.py`, `deploy_auto.py`, `deploy_original_service.py`,
  `rollback_battlenet.py` y `restart_openrgb.py` son herramientas históricas del
  prototipo, algunas con rutas de la instalación original. No son el instalador
  público. Usa el paquete generado por `scripts/package.py`.

## Preparación para distribuir

`scripts/install-user.py` se copia como `install.py` al paquete de release:
instala los dos plugins, respalda los existentes, retira binarios duplicados,
verifica dependencias con ldd y deja la preparación y servicios desactivados.
`scripts/configure-game.py` configura explícitamente un prefijo para el servicio
iCUE original. `--uninstall` restaura los archivos anteriores y se detiene si
los archivos instalados fueron modificados por el usuario.

Pruebas del instalador: `python3 tests/installer_test.py`.
Las pruebas de Wine necesitan compilar primero los adaptadores. Las de la DLL
original requieren que el desarrollador aporte legalmente esa DLL a su fixture
local; no se distribuye en Git ni en la release.

Pendiente validar el paquete en una segunda máquina/distribución compatible.
El paquete binario inicial usa las bibliotecas del equipo de compilación: no
es un binario universal para todas las distribuciones Linux ni para Flatpak.

La copia de desarrollo preparada para GitHub está separada de la instalación
activa del usuario; preparar/publicar el repositorio no actualiza sus plugins.

## Verificación de la preparación para GitHub

Pasan 17 pruebas: instalador (5), servicio (4), endurecimiento (5) y automatización
(3). Las dos últimas suites necesitan los adaptadores compilados. Las pruebas
REST no pudieron ejecutarse en esta sesión porque el entorno prohíbe crear
sockets; no se consideran aprobadas. La integración nativa y Wine no se han
revalidado durante este empaquetado. Se comprobaron la integridad del archivo
de release y los comandos de ayuda de sus asistentes, sin instalar sobre el
OpenRGB activo del usuario.
