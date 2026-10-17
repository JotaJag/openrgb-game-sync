# Diablo IV: iluminación ausente y cierre tras instalar Game Sync 0.4

Estado: el usuario confirma que el juego ya no falla tras la restauración.
Integración suspendida; mecanismo exacto del cierre sin confirmar.

El usuario informó de ausencia de iluminación y cierre con cuadro blanco y OK.
FenrisDebug registra inicialización ChromaCorsair, carga del adaptador local
CUESDK.x64_2015.dll y excepción c00000e5 en kernelbase. La traza no atribuye el
cierre al adaptador. OpenRGB estaba activo con receptor UDP localhost:6743.

Se pausó la preparación automática, se restauró la DLL original del juego
(comparación binaria con la copia previa) y se retiraron nueve adaptadores
instalados en el prefijo Battle.net. Registro Logitech restaurado por sección,
sin reemplazar todo el registro del prefijo. Copias y manifiesto:
~/.local/share/openrgb-backups/diablo-rollback-20261016-180533

GameSyncAutomation.json bloquea el prefijo Battle.net y el directorio de Diablo
IV. --enable conserva estas exclusiones. La prueba de regresión
`python3 tests/hardening_test.py` comprueba que incluso la preparación de arranque
con launch=True no reinstala bibliotecas en un prefijo suspendido.

El usuario ha repetido la prueba con los archivos originales y confirma que
el juego ya no falla. Esta comparación apunta a los cambios de integración,
pero se retiraron varias bibliotecas a la vez y no identifica la función o DLL
responsable. Mantener el bloqueo y la preparación automática pausada. Cualquier
corrección futura debe probarse de forma aislada antes de volver al juego. Las
pruebas de adaptadores Wine y dispositivos simulados no validan compatibilidad
real con Diablo IV.


## Servicio independiente 0.5

Se implementó el canal original sin sustituir DLL. La DLL guardada de Diablo
es Corsair 3.0.234 y anuncia protocolo 10. Su bootstrap usa los canales
CorsairUtilityEngine/{4A48E328-2134-4D19-B31F-764ADEFB844C}_in/_out,
registro :pid: y canales privados por sesión. Los mensajes son JSON UTF-8
terminados en NUL con longitud uint32 little-endian.

Las pruebas con la DLL original bajo UMU/Proton Experimental comprueban
handshake, dispositivo tipo teclado (2, no 1), 105 LEDs/posiciones, colores
síncronos, callbacks asíncronos, buffer/flush, lectura y release. El test nativo
comprueba que la DLL original llega al controlador simulado mediante el servicio
gestionado por el plugin, y que se detiene al desactivar Game Sync.

La versión 0.5 está instalada y OpenRGB reiniciado. El servicio de Battle.net
informó running=true, cero errores, esperando sesiones. La DLL de Diablo sigue
idéntica al original (SHA256
6a7fe2b27cb7b5d440dfb29ccb0276c28bc40cf2a96720185300bb100b528949).
La sustitución automática de bibliotecas sigue pausada y bloqueada para el juego.
Pendiente: validación real de efectos y estabilidad al reabrir Diablo.


## Lentitud y ausencia de iluminación con 0.5

El usuario informó que no había luces y el juego parecía ir lento. Se detuvo
solamente el backend `pipe_service/run.py` (PID 48376), y se vació pipe_services
para impedir su arranque automático. No se modificó la DLL de Diablo.
Copia de estado/configuración:
~/.local/share/openrgb-backups/pipe-service-stop-20261016-184445

El estado registra handshake, device count, device info, positions y 1723
CorsairSetLedsColors. La validación rechazaba paquetes con `Color packet values`;
el registro antiguo no distingue LED ausente de canal RGB fuera de rango.
La documentación SDK3 indica que los LED no presentes se ignoran sin error.
Se reprodujo y corrigió el rechazo de un paquete mixto con LED existente y
LEDs ausentes. Ahora se procesan los conocidos y se ignoran los demás. RGB
fuera de rango sigue siendo error, con diagnóstico específico.

La prueba de 100 llamadas síncronas con la DLL original bajo UMU/Proton tardó
4965 ms antes y 24 ms después de agrupar la cabecera/cuerpo en un envío y activar
TCP_NODELAY en ambos extremos. La prueba anterior falla por encima de 1000 ms.
Los nuevos contadores distinguen llamadas aceptadas, fotogramas emitidos,
foco, PID de cliente e identificadores ignorados. No hay captura completa de
paquetes de juego en producción. Pendiente: confirmar rendimiento tras detener
el servicio y comprobar los efectos reales con la corrección.


La prueba con la DLL original también pasa enviando a Escape junto a G1 (121)
y Mute (98), válidos para el SDK pero ausentes en el teclado virtual. Un primer
fixture con CLI_Invalid y 999 no era válido para la DLL, que lo rechazaba antes
de transmitir; se sustituyó por identificadores reales. La integración completa
con controlador simulado vuelve a pasar. El servicio real sigue apagado y su
arranque automático deshabilitado, pendiente de la comprobación de rendimiento
del usuario tras detenerlo y de una nueva prueba real de iluminación.


## Prueba real tras corregir transporte y LED ausentes

El usuario confirma que el rendimiento volvió a la normalidad al detener el
servicio. Se volvió a activar la versión corregida y se reinició OpenRGB.
Al reabrir Diablo, el estado real registra una sesión con PID/foco coincidentes,
45 llamadas de color aceptadas, 53 fotogramas enviados y cero errores. Los LED
ignorados incluyen teclas válidas ausentes del teclado virtual (64,66,67,...137),
lo que confirma la causa del rechazo anterior de fotogramas completos.
Pendiente: confirmación visual y de rendimiento del usuario con la corrección.

El usuario confirma después de la corrección que funcionan las luces y el
rendimiento parece correcto. Integración real de Diablo validada por el usuario
con la DLL original; mantener el bloqueo a la sustitución de bibliotecas.
