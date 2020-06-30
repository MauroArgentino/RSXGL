# RSXGL - La biblioteca de gráficos RSX

Esta biblioteca implementa partes de la especificación de perfil de núcleo de OpenGL 3.1 para la GPU RSX de PlayStation 3. Es adecuado para su uso en programas que tienen acceso exclusivo al RSX, como el software GameOS (y es probable que no sea adecuado para implementar un escritorio multitarea, ya que la biblioteca no arbitra el acceso al RSX).

Consulte el archivo STATUS para obtener información actualizada sobre las capacidades actuales de esta biblioteca.

## Instalar

RSXGL utiliza las herramientas automáticas GNU para su sistema de compilación y se distribuye con un script de configuración. Requiere los siguientes proyectos:

* [ps3toolchain](http://github.com/ps3dev/ps3toolchain)
* [PSL1GHT](http://github.com/ps3dev/PSL1GHT)

RSXGL incorpora partes del proyecto Mesa, principalmente para proporcionar la compilación en tiempo de ejecución de los programas GLSL. Las versiones adecuadas de Mesa y libdrm se incluyen con RSXGL. python 2.6 con el módulo libxml2 es requerido por el proceso de compilación de Mesa (específicamente para construir las funciones integradas de GLSL). Para indicar al sistema de compilación que utilice un ejecutable de Python determinado, distinto del predeterminado, establezca la variable de entorno PYTHON:

```
export PYTHON=/path/to/python-2.6
```

Dependencias de ejemplo de Debian/Docker:
```
apt-get install python-libxml2 xutils-dev nvidia-cg-toolkit
```

La biblioteca RSXGL depende de una cadena de herramientas que puede generar archivos binarios para la PPU de PS3 y también para partes del SDK de PSL1GHT. Los programas de ejemplo también requieren algunas bibliotecas portadas, como libpng, que son proporcionadas por el proyecto ps3toolchain. ps3toolchain recomienda establecer dos variables de entorno para localizar estas dependencias:

```
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
```

El script configure de RSXGL utilizará las variables de entorno anteriores si están establecidas; si no se establecen, de forma predeterminada el script utiliza la configuración anterior.

RSXGL viene con un script que se debe usar para generar y ejecutar el script del proyecto. Desde el directorio de origen de nivel superior:

```
./autogen.sh
```

Simplemente puede generar el script de configuración, sin configurar realmente la compilación (útil si desea compilar en un directorio independiente del origen):

```
NOCONFIGURE=1 ./autogen.sh
```

```
./configure
make
make install
```

El sistema de compilación crea bibliotecas destinadas a ejecutarse en la PS3; también crea algunas utilidades (como un ensamblador de programas de sombreado derivado del cgcomp de PSL1GHT) que están destinadas a ejecutarse en el sistema de compilación. De forma predeterminada, estos productos se instalan en $PS3DEV/ppu y $PS3DEV, respectivamente. Puede dirigir el sistema de compilación para colocarlos en otro lugar:

```
./configure --with-ppu-prefix=/path/to/rsxgl --prefix=/path/to/ps3dev
```

Si las bibliotecas portadas, como libpng y zlib, se han instalado en otro lugar que no sea $-PS3DEV/portlibs/ppu, puede establecer una variable de entorno para encontrarlos:

```
./configure ppu_portlibs_PKG_CONFIG_PATH=/path/to/portlibs/lib/pkgconfig
```

Pase la opción "--help" para configurar para ver muchas otras opciones del sistema de compilación.

## Programas de muestra

Actualmente se construyen dos programas de ejemplo:

* src/samples/rsxgltest - Un programa de prueba muy simple cuyo contenido y comportamiento variarán. Este programa se utiliza principalmente para probar varias características de la biblioteca a medida que se desarrollan.

* src/samples/rsxglgears - Un puerto de una castaña antigua, el programa "glgears" que utiliza OpenGL para renderizar algunos engranajes giratorios. Este puerto se basa en una versión incluida en la biblioteca Mesa, que era en sí mismo un puerto a OpenGL ES 2 después de ser transmitido a lo largo de las edades.

Los programas de ejemplo se empaquetan en archivos .pkg NPDRM, pero esos paquetes permanecen en sus ubicaciones de compilación; no se mueven a ninguna parte en relación con la ruta de instalación de RSXGL por "hacer instalación".
El ejemplo puede imprimir información de depuración a través de TCP, a la manera del ejemplo de red/debugtest de PSL1GHT. Puede pasar la dirección IP de su sistema host a la configuración de RSXGL:

```
./configure RSXGL_CONFIG_samples_host_port=192.168.1.1 RSXGL_CONFIG_samples_port=9100
```

Antes de iniciar la aplicación en PS3, utilice este comando para recibir la salida de depuración:

```
nc -l 9100
```

Si no desea crear las muestras en absoluto:

```
./configure --disable-samples
```

Hay ejemplos más complejos disponibles en [a separate project](http://github.com/gzorin/rsxgl-samples).
