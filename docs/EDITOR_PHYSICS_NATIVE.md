# Compilador de fisicas nativas del editor

El Simulador de Fisicas es una herramienta de autoria. El mod se usa para
seleccionar, configurar y compilar los cuerpos, pero el resultado guardado en el
nivel esta formado exclusivamente por objetos y triggers de Geometry Dash
2.2081. No se instala un rigid body propio en `PlayLayer` y el nivel no depende
del mod para reproducir la salida compilada.

## Modos de salida

- **Triggers:** comportamiento durante la partida mediante Advanced Follow,
  Edit Advanced Follow, Collision Block, Collision, Spawn y Rotate.
- **Keyframes:** trayectoria determinista calculada en la vista previa y
  convertida en Keyframe Point + Keyframe Animation.

Cada cuerpo dinamico elige su modo por separado. Esto permite, por ejemplo,
hornear una pieza decorativa y dejar una caja interactiva en tiempo real dentro
de la misma compilacion.

## Presets reactivos

- **Empujable:** gravedad, roce, rebote contra cuerpos fijos e impulso al tocar
  al jugador.
- **Rebotador:** mas impulso, velocidad maxima mayor y menos perdida de energia.
- **Pesado:** responde menos al jugador, cae mas y rebota menos.
- **Flotante:** gravedad y velocidad reducidas.
- **Iman:** Advanced Follow hacia P1 o P2.
- **Pendulo:** crea un ancla nativa y limita el rango de movimiento.
- **Explosion:** separa los objetos del cuerpo en subgrupos, asigna un Control
  ID a cada fragmento y les aplica velocidad radial y gravedad independiente.

`Fuerza` multiplica los impulsos del preset. La masa reduce la respuesta a esos
impulsos; gravedad, rebote, friccion, arrastre, velocidad y giro inicial se
traducen a sus equivalentes nativos. `Sensor` modifica el grosor de los cuatro
Collision Blocks que viajan con el cuerpo. Los botones P1/P2 deciden que
jugadores pueden activar sus colisiones o cual es el objetivo de Iman.

## Grafo generado

La implementacion se divide en dos fases:

1. `PhysicsNative.cpp` produce un `TriggerGraph` puro, sin tocar el editor.
2. `PhysicsTriggerEmitter.cpp` reserva IDs, valida el grafo y materializa cada
   nodo mediante `LevelEditorLayer::createObject`.

Los subgrupos de acciones contienen triggers con `Spawn Triggered` y `Multi
Triggered`. Los Collision Trigger disparan esos subgrupos al entrar en contacto.
La gravedad usa un bucle Spawn con una espera minima de 0.02 s y Edit Advanced
Follow en modo aditivo.

El planificador usa namespaces separados para Group ID, Block ID y Control ID,
respeta el limite de diez grupos por objeto y rechaza grafos de mas de 8000
objetos. Si GD no reconoce una clase nativa, falta un ID o falla una asignacion,
se eliminan los objetos nuevos y se restauran los grupos originales. Compilar
otra vez la misma seleccion reemplaza su salida anterior; `Quitar ultimo`
revierte la compilacion mas reciente del editor actual.

## Alcance de la simulacion nativa

Esta salida es una aproximacion construida con el sistema de Advanced Follow de
GD, no un solver de cuerpos rigidos ejecutandose dentro del juego. Los cuerpos
fijos se aproximan con Collision Blocks rectangulares alineados a ejes y cada
cuerpo dinamico usa cuatro sensores laterales. El grafo cubre jugador contra
cuerpo y cuerpo contra capturas fijas; no resuelve contacto entre dos cuerpos
dinamicos ni entre una salida reactiva y otra horneada. Formas concavas,
apilamiento estable, torque exacto y contacto continuo no existen como
primitivas nativas y por tanto no pueden reproducirse con la misma precision
que la vista previa.

Los parametros numericos de Advanced Follow deben probarse dentro de GD porque
sus unidades no son las del solver de la vista previa. El compilador conserva
los valores como presets centralizados para que se puedan calibrar sin cambiar
el emisor ni el formato del grafo.

## Fuentes auditadas

- [Bindings 2.2081 (`GeometryDash.bro`)](https://github.com/geode-sdk/bindings/blob/main/bindings/2.2081/GeometryDash.bro)
- [Geometry Dash Editor Guide](https://www.robtopgames.com/files/GDEditor.pdf)

El catalogo local esta en `NativeTriggerCatalog.hpp`. Los IDs usados por el
emisor son 1268 (Spawn), 1346 (Rotate), 1815 (Collision), 1816 (Collision
Block), 3016 (Advanced Follow), 3032/3033 (Keyframes) y 3660 (Edit Advanced
Follow).

## Verificacion en GitHub y dentro del juego

El workflow de Windows ejecuta primero las regresiones puras del solver y del
grafo nativo, y despues compila el mod con la accion de Geode. La comprobacion
manual recomendada es:

1. Crear una caja dinamica y un suelo fijo; compilar `Empujable` con P1 y P2.
2. Jugar y verificar empuje desde los cuatro lados, gravedad y rebote.
3. Repetir con `Iman`, `Pendulo` y un cuerpo de tres objetos en `Explosion`.
4. Guardar, salir del editor y jugar otra vez con el mod desactivado para
   confirmar que la salida sigue funcionando como contenido nativo.
5. Recompilar la misma seleccion y usar `Quitar ultimo` para revisar el rollback.
