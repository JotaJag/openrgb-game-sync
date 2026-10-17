# Compatibilidad: desarrollo 0.6.0-alpha.1

La compatibilidad requiere tres cosas distintas: que el juego emita iluminación,
que pueda conectar con la interfaz esperada y que sus llamadas tengan el
comportamiento correcto. La presencia de símbolos exportados no demuestra las
dos últimas. Este proyecto no promete funcionar con todos los juegos de un
fabricante ni habilita integraciones desconocidas automáticamente.

| Interfaz | Cobertura implementada | Límite pendiente |
| --- | --- | --- |
| Chroma DLL 32/64 bits | Estático, apagado, custom, custom-key; CUSTOM2 teclado 8×24, ratón 9×7 y alfombrilla 20 LED; STATIC2 ratón; caché de efectos | Efectos de hardware wave/reactive/breathing, CreateEffect por GUID real y notificaciones no implementados |
| Chroma REST | Sesiones, heartbeat, estático/custom/custom-key/custom2, lotes de creación/selección/borrado con resultados individuales | No implementa WebSocket ni descubre automáticamente cualquier juego; REST no puede identificar el foco del proceso emisor |
| iCUE SDK 3 DLL 32/64 bits | Teclado y ratón virtuales, escritura/buffer/flush, callbacks y lectura; ignora LED válidos ausentes sin perder los presentes; rechaza duplicados y RGB inválido | Propiedades de hardware y eventos de teclas no implementados; no emula todos los dispositivos Corsair |
| iCUE SDK 4 DLL 64 bits | Teclado y ratón virtuales, posiciones, alfa, buffer/flush por dispositivo, lectura y liberación independiente | No hay versión DLL de 32 bits, propiedades ni captura de eventos; no emula hardware Corsair real |
| Servicio DLL Corsair original | Protocolo 10 con la DLL original Corsair 3.0.234 de Diablo IV | No es un servidor universal para todas las versiones de iCUE |
| Lightsync DLL 32/64 bits | Global, por tecla y bitmap; guardar/restaurar; flash/pulso globales y por tecla, finito/infinito y parada | Quartz no implementado; temporización por software a aproximadamente 50 Hz, requiere validar equivalencia con cada juego |

Las cuadrículas se adaptan a la matriz de OpenRGB por coordenadas normalizadas.
En CUSTOM2, los overrides de teclas 6×22 se escalan sobre el fondo 8×24; esto no
promete alineación exacta de teclas en todos los layouts físicos. La iluminación
interna usa los roles existentes y colores medios cuando no hay un dispositivo
equivalente.

## Cambios respecto a 0.5.0

- Se amplía el límite de paquete de 132 a 192 colores, conservando emisores v1
  y v2 anteriores. Se añade la liberación v2 de un solo rol de dispositivo.
- Se evita reenviar teclados vacíos al vaciar únicamente el buffer del ratón
  iCUE 4; liberar el ratón no libera el teclado.
- Las llamadas a animaciones Lightsync programan trabajo y retornan sin esperar
  a que termine el efecto. El transporte mantiene su supresión fuera de foco.
  Al terminar o parar se recupera el estado previo; las escrituras directas por
  tecla dejan de ser sobrescritas por una animación global.
- Chroma REST se configura por separado sin activar la sustitución automática
  de DLL y se detiene al desactivar Chroma en la pestaña del plugin.
- El adaptador DLL iCUE 3 corrige el rechazo de paquetes mixtos con LED presentes
  y ausentes, igual que ya se había corregido en el servicio de DLL original.

## Qué se ha comprobado

Las pruebas locales ejecutan el código real de los adaptadores SDK 3/4 y
Lightsync con un shim de Win32/transporte. El reloj es controlado para probar
fin de efecto, restauración, parada, callbacks y rollover de tiempo. También se
prueba el receptor Qt real sin abrir un socket, y el handler REST y su encoder
con peticiones y datagramas simulados. Esto comprueba lógica y formato, no el
comportamiento completo del sistema Wine, el planificador o un juego real.

Se compilan los adaptadores para Windows con las cabeceras MinGW reales y el
plugin Linux Qt6. Las pruebas que abren sockets y Wine deben ejecutarse fuera
del entorno restringido de desarrollo:

```bash
bash scripts/verify-compatibility.sh --wine
```

El resultado se guarda en `build/compatibility-verification.txt`. La orden no
instala plugins y usa prefijos temporales para los smoke tests Wine. Puede
crear y conservar fixtures temporales; no utiliza los prefijos de tus juegos.
La integración nativa opcional necesita su propio fixture de DLL original.

El 17-10-2026 se completó ese verificador con `--wine` sobre el commit
`a60e02fa3ca0c9c2646a4feb2f269dd73dbe5555`: pasan las 27 pruebas Python,
los seis ejecutables host, la integración Qt/REST y los smoke tests Wine.
Se observaron los paquetes de CUSTOM2, ratón iCUE 4 y flash/pulso de Lightsync
procedentes de las DLL reales y sus threads. También pasan las pruebas de foco,
registro y el worker en prefijos temporales. La prueba de la DLL Corsair original
se marca `SKIP`: no se distribuye ese fixture. Este resultado valida los casos
ensayados bajo Wine, no todos los juegos ni las variantes de SDK existentes.

0.5.0 se validó visualmente con Diablo IV. **Estos cambios 0.6 no se han validado
todavía en juegos reales ni instalado en el OpenRGB activo del usuario.**

## Referencias de las interfaces

- [Tipos Chroma, enum y layouts](https://assets.razerzone.com/dev_portal/C%2B%2B/en/_rz_chroma_s_d_k_types_8h_source.html).
- [Teclado REST y CUSTOM2](https://assets.razerzone.com/dev_portal/REST/html/md__r_e_s_t_external_03_keyboard.html).
- [Lotes de identificadores REST](https://assets.razerzone.com/dev_portal/REST/html/md__r_e_s_t_external_10_8effect.html).
- [API Corsair](https://corsairofficial.github.io/cue-sdk/) y documentación SDK 3
  del checkout fijado en `dependencies/README.md`.
- [Bindings y documentación oficiales Logitech](https://github.com/Logitech/logiPy/blob/master/logipy/logi_led.py).
