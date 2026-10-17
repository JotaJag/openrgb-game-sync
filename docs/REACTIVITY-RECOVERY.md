# Reactividad y recuperación: 0.6.0-alpha.2

Rama: `feature/reactivity-recovery-0.6`. La instalación activa del usuario sigue
en alpha.1 mientras se verifica esta actualización. No se cambia el mapeo de Q.

## Menos trabajo repetido

El receptor renueva la sesión con todos los fotogramas válidos, aunque los
colores no cambien. Solo marca el renderizado como pendiente cuando cambia el
contenido o la forma de la cuadrícula. El byte de alfa que el protocolo no usa
no produce cambios falsos. Cambiar de sesión, liberar y volver a adquirir o
volver a detectar dispositivos invalida la caché correspondiente.

El renderizador calcula los colores finales por dispositivo antes de llamar a
sus setters. Si un paquete distinto da exactamente los mismos colores físicos
(por ejemplo, el mismo promedio para una tira), tampoco los reenvía. Al cambiar
sí aplica los LED modificados y llama a `UpdateLEDs`. Un cambio de modo fuerza
la actualización, aunque coincida con el último color enviado.

La prioridad compartida con Effects se sigue renovando aunque no haya cambios;
esto impide que Effects reaparezca al agotarse su lease. Al perder la sesión se
restauran los colores y modo anteriores. Se mantiene el intervalo de renderizado
de 33 ms: no se afirma una nueva latencia ni se sube todavía a 60 FPS.

La pestaña muestra fotogramas aceptados, duplicados y actualizaciones de
dispositivos. Estas últimas cuentan llamadas a `UpdateLEDs`, no paquetes USB.
El detalle de actividad indica la edad del último fotograma, no una medición de
latencia física extremo a extremo.

## Recuperación de helpers

`HelperProcess` encapsula arranque, supervisión y parada. Se reutiliza para
Chroma REST y para cada servicio de la DLL original iCUE:

- Tras una caída o un fallo de arranque hay hasta cinco reintentos adicionales,
  con esperas de 1, 2, 4, 8 y 16 segundos. Después se detiene el ciclo; desactivar
  y reactivar la integración permite reintentarlo explícitamente.
- Treinta segundos de funcionamiento saludable restablecen el presupuesto.
- iCUE original debe publicar un estado nuevo en un máximo de 45 segundos.
  Un archivo antiguo con `running=true` no prueba que el nuevo proceso funcione.
  Tras el arranque, diez segundos sin actualizar su estado disparan recuperación.
- REST se supervisa por la vida del proceso: no publica heartbeat de salud.
  No se afirma detectar un bloqueo HTTP de un proceso que siga vivo.
- Si falla un helper se libera la prioridad de su fuente. La recuperación del
  proceso no reproduce colores antiguos. El juego/SDK debe volver a establecer
  su sesión y emitir; no se garantiza la reconexión de todos los juegos.
- Desactivar una integración, desactivar Game Sync o descargar el plugin cancela
  los reintentos. La parada es normal primero y forzada solo tras el plazo de
  cierre. Descargar el plugin libera el renderizador antes de esperar al helper.

La configuración de juegos y los bloqueos de DLL no se alteran. Si hay varios
prefijos iCUE, una avería libera la fuente iCUE de forma conservadora; otro
emisor sano puede volver a adquirirla con su siguiente fotograma.

## Pruebas

- Receptor real: 1.000 duplicados no generan nuevas notificaciones de cambio,
  pero sí mantienen reciente la sesión; colores distintos sí notifican.
- Renderizador real con controladores simulados: duplica paquetes y compara
  salidas equivalentes, comprueba cambios efectivos, renovación de prioridad,
  restauración tras fallo y primer repintado al readquirir.
- Supervisor real con procesos Python temporales: caída, recuperación, reseteo
  del presupuesto tras estabilidad, agotamiento, cancelación durante la espera,
  fallo al ejecutar, estado antiguo y heartbeat detenido. No usa prefijos Wine.
- El test nativo completo comprueba con UDP que los mensajes sin cambios
  mantienen Effects pausado más allá del plazo de su lease sin reenviar colores.

La comprobación con sockets y Wine se ejecuta con
`bash scripts/verify-compatibility.sh --wine`. Su informe debe revisarse antes
de instalar la alpha.2 sobre la versión activa.
