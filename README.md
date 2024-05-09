# Proyecto: StarPack - Simple TAR

## Descripción
El objetivo de StarPack es desarrollar un comando de empaquetado de archivos que imita y extiende la funcionalidad del comando `tar` en sistemas UNIX. Este proyecto busca implementar un conjunto de opciones estándar de `tar`, así como características adicionales específicas que incluyen el manejo de una estructura de archivo tipo FAT para la gestión eficiente del espacio y la desfragmentación de archivos sin uso de archivos temporales.

## Funcionalidades
StarPack soportará las siguientes operaciones:
- **Creación de Archivos** (`-c`): Permite a los usuarios empaquetar múltiples archivos en un archivo único.
- **Extracción** (`-x`): Facilita la extracción de archivos desde el archivo empaquetado.
- **Listado** (`-t`): Muestra los contenidos de un archivo empaquetado.
- **Actualización** (`-u`): Actualiza archivos dentro del archivo empaquetado si ya existen.
- **Eliminación** (`--delete`): Permite eliminar archivos específicos del empaquetado sin reducir el tamaño físico del archivo empaquetado.
- **Agregar Archivos** (`-r`): Añade más archivos al empaquetado existente.
- **Desfragmentación** (`-p`): Optimiza el espacio al eliminar bloques vacíos y ajustar el tamaño del archivo al contenido real.
- **Verbose** (`-v`): Muestra información detallada durante la ejecución de las operaciones.

## Tecnologías
- **Lenguaje de Programación**: C/C++
- **Sistemas Operativos**: Compatible con sistemas basados en UNIX.

## Miembros del Equipo
- **Camilo Sánchez Rodríguez** - Estudiante de Ingeniería Informática, matrícula 2021081146.
- **Mario Barboza Artavia** - Estudiante de Ingeniería Informática, matrícula 2021075241.

## Planificación
El desarrollo del proyecto se divide en varias fases:
1. **Investigación y Diseño**: Estudiar la implementación existente de `tar` y planificar la arquitectura de StarPack.
2. **Desarrollo**: Codificación de las funcionalidades básicas y avanzadas.
3. **Pruebas**: Verificar la funcionalidad y robustez en diferentes escenarios.
4. **Documentación**: Preparar manuales de usuario y documentación técnica.

## Objetivos
El principal objetivo es proporcionar una herramienta potente y flexible para el empaquetado de archivos, con mejoras específicas en la gestión del espacio y la eficiencia operativa. StarPack será una valiosa adición a las herramientas de manejo de archivos en entornos UNIX.


