# OpenRGB Game Sync

Plugin para dar prioridad a la iluminación enviada por juegos compatibles en
Linux con Wine/Proton y devolver el control a Effects cuando dejan de enviar.
Rama de desarrollo 0.6.0-alpha.1. La prerelease publicada 0.5.0 se conserva.

## Instalación sencilla

Requiere **Linux x86_64, OpenRGB 1.0 con Qt6/API 5 y Python 3.9+**.
Descarga el paquete de la sección **Releases** de este repositorio y extráelo.
Cierra OpenRGB y, desde la carpeta extraída, ejecuta:

```bash
bash install.sh
```

No uses sudo. Después abre OpenRGB y habilita Game Sync y Effects en Plugins.
El instalador respalda los plugins anteriores y no modifica archivos de juegos.
Para configurar el servicio de un juego, consulta la
[guía de instalación](docs/INSTALL.md). La instalación por sí sola no prepara
ni promete compatibilidad con todos los juegos.

Para desinstalar y recuperar los plugins anteriores, cierra OpenRGB y ejecuta:

```bash
bash uninstall.sh
```

El repositorio y sus releases son privados: otros usuarios necesitan acceso
explícito al repositorio para descargarlos. El paquete puede compartirse con
las personas a las que quieras facilitar la prueba.

## Mejoras en desarrollo 0.6

Chroma CUSTOM2 y lotes REST; ratón y liberación por dispositivo en iCUE 4;
paquetes iCUE 3 con LED ausentes; flash/pulso por software para Lightsync.
Consulta la [matriz de compatibilidad y límites](docs/COMPATIBILITY.md).
Estas mejoras están probadas de forma aislada y necesitan pruebas Wine/juegos.

## Estado

Diablo IV se probó con su DLL Corsair 3.0.234 **original**, mediante un servicio
independiente. Tras corregir la latencia y el tratamiento de LED ausentes, el
usuario confirmó luces y rendimiento correctos. La sustitución de DLL de
Diablo/Battle.net permanece bloqueada. No se capturan imágenes de pantalla ni
se inspecciona la memoria del juego.

Incluye adaptadores Chroma, iCUE y Lightsync; cada API tiene límites y necesita
validación por juego. El paquete Linux depende de las bibliotecas Qt6 del
sistema y requiere pruebas en una segunda máquina. No cubre Windows, macOS,
OpenRGB Qt5 ni instalaciones Flatpak.

## Desarrollo

- [Compilar y ejecutar pruebas](docs/DEVELOPMENT.md).
- [Continuidad y decisiones](docs/CONTINUIDAD.md).
- [Dependencias y parche de Effects](dependencies/README.md).
- [Detalle técnico del prototipo](docs/PROTOTYPE.md).
- [Incidente y validación con Diablo](docs/diablo-incident-20261016.md).

El plugin Effects incluido está parcheado para compartir la prioridad de
renderizado con Game Sync y corregir las transiciones Sequence. Usa las dos
versiones juntas. El instalador deja pausada la preparación automática de
bibliotecas y servicios hasta que el usuario configura el prefijo deseado.

## Licencia

El código local se distribuye bajo GPL-2.0-or-later; consulta [LICENSE](LICENSE).
Las dependencias y cabeceras de terceros conservan sus licencias originales.
No se incluyen DLL de juegos ni prefijos Wine en el repositorio.
