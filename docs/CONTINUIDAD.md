# Estado para continuar el desarrollo

## Trabajo actual: alpha.2

La rama `feature/reactivity-recovery-0.6` añade deduplicación de paquetes y
salidas por dispositivo, supervisión de helpers con reintentos limitados y
contadores de diagnóstico. Detalles en `REACTIVITY-RECOVERY.md`.
Pasan 25 pruebas Python sin sockets y ocho ejecutables host, incluidos procesos
reales que fallan o dejan de actualizar su estado y el renderizador con
controladores simulados. Se ha compilado la integración nativa ampliada.
Verificación externa completada el 17-10-2026 sobre `098e23e`: pasan las
27 pruebas Python, los ocho ejecutables host, la integración nativa con
Sequence/REST y las pruebas Wine de Chroma, iCUE 3/4 y Lightsync, incluidas
las variantes de 32 bits y CUSTOM2/ratón/flash/pulso. Se confirma que los
heartbeats repetidos mantienen la prioridad sin escrituras adicionales.
El informe termina con PASS. El fixture de DLL Corsair original sigue
ausente (SKIP), por lo que esa ruta no se considera revalidada.

La alpha.2 está instalada desde el 17-10-2026. El informe local
`build/alpha2-installation-report.json` confirma arranque de ambos plugins,
servicio original de Diablo activo y sin errores, y DLL original sin cambios.
Se conserva copia en `~/.local/share/openrgb-backups/alpha2-20261017-133028-796021`.
El usuario confirmó que funciona con Diablo tras actualizar. Esta confirmación
no constituye una medición instrumental de latencia ni una validación de otros
juegos; no se confirmó por separado la devolución de prioridad al cambiar de ventana.
La observación sobre la tecla Q queda aplazada y no se cambia su mapeo.
Los resultados siguientes corresponden a la alpha.1, no a la alpha.2.

## Punto de partida

La rama `feature/compatibility-0.6` añade una alpha `0.6.0-alpha.1` sobre la
release privada 0.5.0, que no se modifica. Consulta `COMPATIBILITY.md` para la
cobertura exacta: CUSTOM2/lotes Chroma, ratón iCUE 4 y liberación por dispositivo,
LED ausentes iCUE 3, animaciones temporizadas Lightsync y REST independiente
de la preparación de bibliotecas.

Verificación de la alpha: pasan 27 pruebas Python (incluidas REST con sockets) y seis ejecutables host
(decodificador Chroma, SDK 3, SDK 4, Lightsync, receptor Qt y metadatos del
plugin). Chroma y Lightsync también pasan UBSan. Se han compilado el plugin
Linux, los siete adaptadores DLL Windows y los fixtures de pruebas, incluido
`extended-smoke.exe`. No se pierde ninguna exportación de las siete DLL de 0.5.
El paquete alpha se ha instalado y desinstalado en una carpeta temporal.

Verificación externa completada el 17-10-2026 con el commit
`a60e02fa3ca0c9c2646a4feb2f269dd73dbe5555`. El informe termina con
`PASS: comprobaciones solicitadas completadas`. Pasa la integración nativa de
prioridad/Sequence y REST, incluido inicio con preparación pausada y parada
por desactivación de Chroma/Game Sync. Pasan Chroma Wine y supresión por foco,
iCUE 3/4 y Lightsync, las variantes iCUE 3/Lightsync de 32 bits, registro y
worker de un prefijo temporal. El fixture nuevo verifica DLLs/threads reales
para CUSTOM2, ratón SDK 4 y flash/pulso Lightsync.

El servicio con DLL Corsair original se marcó como `SKIP` al no incluir el
fixture propietario. No se considera revalidado por esta ejecución. El test
permite exigirlo con `OPENRGB_REQUIRE_ORIGINAL_FIXTURE=1`.

La alpha 0.6.0-alpha.1 ya está instalada y OpenRGB reiniciado. La actualización
controlada confirmó la carga de ambos plugins y el servicio de Diablo activo,
sin errores y esperando sesiones. La DLL original de Diablo conserva su SHA-256
validado; se mantienen la preparación automática desactivada y los bloqueos
para Battle.net/Diablo. Existe copia completa de configuración y plugins para
restaurar la instalación anterior.

El usuario confirmó posteriormente que la alpha.1 funciona con Diablo.
No se dispone de una medición instrumental de latencia ni de validación
exhaustiva de todos los juegos.

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
