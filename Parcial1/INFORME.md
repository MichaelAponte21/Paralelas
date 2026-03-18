# INFORME: Filtros de Convolución sobre Imágenes

**Propuesta 1: Taller Evaluativo de Programación Paralela**
**Integrantes:** [Tu Nombre / Grupo]
**Fecha:** 18 de Marzo de 2026
**Entorno:** 12 Procesadores Disponibles

---

## 1. Análisis PCAM

- **Particionamiento:** Hemos optado por una **Descomposición de Dominio**. La imagen se divide calculando tareas por *filas* de la matriz ($N$ tareas), lo cual es más práctico para OpenMP ya que mantiene los píxeles adyacentes en rangos continuos en la memoria. Particionar por cada uno de los $N^2$ píxeles habría generado un salto innecesario creando un sobrecosto en gestión de hilos que superaría al esfuerzo de multiplicar en paralelo.
  
- **Comunicación:** En el patrón de un kernel de convolución se nota que todas las vecindades (el halo o filas fronterizas superpuestas) son compartidas en **lecturas de solo lectura** concurrentes a lo largo de la `entrada`. Debido a que cada hilo de manera unívoca graba el resultado sobre píxeles disjuntos de una matriz separada de `salida`, **se evita la condición de carrera y no fue necesario diseñar mecanismos complejos como un "doble buffer"** o usar la cláusula `critical/locks`.
  
- **Aglomeración:** La granularidad es gruesa; unimos las tareas hasta dividirlas agrupando de forma uniforme filas enteras por procesador. Es decir, a un hilo le será asignado un lote agrupado de $N/P$ filas consecutivas, esto amortiza el overhead del procesamiento de OpenMP maximizando la relación Cómputo/Comunicación haciendo uso eficiente del caché temporal antes de que el procesador deba solicitar leer nuevas subsecciones de memoria.

- **Mapeo:** Hemos definido un comportamiento `schedule(static)` explícito que mapea predeciblemente iteraciones regulares. Se justifica por el hecho de que un kernel de imagen siempre recorre celdas fijas, por lo que su carga de cálculo se mantiene totalmente estable a lo largo de cualquier índice, descartando por completo el uso de dinámicos (`dynamic` o `guided`), logrando el mayor balanceo de la carga reduciendo paradas no programadas en la distribución de la rutina de OpenMP.

---

## 2. Detalles de la Implementación OpenMP

La directiva central que utilizamos fue el paradigma de paralelización de bucles:
```c++
#pragma omp parallel for num_threads(num_hilos) schedule(static)
for (int i = 0; i < entrada.alto; ++i) { ... }
```

- **Lugar de inserción:** Esta directiva fue insertada en lo más alto del for externo de filas, en funciones de convolución normal `convolucion_paralela` y para la iteración independiente en la `sobel_magnitud_paralela`.
- **Variables privadas:** El compilador C++17 lidia con las cláusulas `private` abstrayendo automáticamente la recolección dado que declaramos la variable contador `double suma = 0.0;` e iteradores `int di, dj` **dentro** del bucle en ejecución. 
- **No agrupamiento (Collapse):** Hemos elegido descartar el anidamiento con `collapse(2)` para evitar distorsionar el beneficio espacial del caché al aplanar columnas adyacentes si un balance natural en N-filas ya nos resulta suficiente.

---

## 3. Mediciones y Resultados 

### Tabla 1: Tiempos, Speedup y Eficiencia (Tamaño: 1024x1024)

*Eficiencia (Eff) = Speedup (SU) / Número de Hilos * 100%*

| Kernel | Secuencial (ms) | 2 hilos (ms) | SU (2h) | Eff (2h) | 4 hilos (ms) | SU (4h) | Eff (4h) | 8 hilos (ms) | SU (8h) | Eff (8h) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Blur 3x3** | 11.36 | 7.11 | **1.60x** | *80.0%* | 4.08 | **2.78x** | *69.5%* | 4.83 | **2.35x** | *29.4%* |
| **Gaussiano 5x5** | 27.81 | 14.27 | **1.95x** | *97.5%* | 7.74 | **3.59x** | *89.8%* | 9.64 | **2.89x** | *36.1%* |
| **Sobel X 3x3** | 13.42 | 6.40 | **2.10x** | *105%* | 5.48 | **2.45x** | *61.3%* | 3.71 | **3.62x** | *45.3%* |
| **Sharpen 3x3**| 11.16 | 8.95 | **1.25x** | *62.5%* | 5.27 | **2.12x** | *53.0%* | 3.75 | **2.98x** | *37.3%* |
| **Emboss 3x3**| 11.71 | 9.82 | **1.19x** | *59.5%* | 5.47 | **2.14x** | *53.5%* | 5.59 | **2.10x** | *26.3%* |

*(Nota: Sobel 2h muestra >100% derivado por aceleración afortunada de localidad optimizada gracias a la afinidad de la caché (memoria temporal calientita) que superó a una compilación secuencial base)*

***Respuesta de Análisis de Kernels:*** ¿Influye el tamaño del kernel en el speedup? Definitivamente. Las matrices "Gaussiano 5x5" requieren más multiplicaciones internas (25 operaciones) versus una de tamaño 3x3 (9 operaciones). A mayor carga granular, aumenta la relación de *cómputo vs comunicación* resultando en una **Eficiencia** envidiable del **89.8% en 4 hilos**.

### Tabla 2: Escalabilidad (Con Kernel Gaussiano 5x5 - Probando Weak Scaling)

| Tamaño de la Imagen | Tiempo Secuencial (ms) | Tiempo Paralelo a 4h (ms) | Speedup (SU) |
| :--- | :---: | :---: | :---: |
| **256 x 256** | 2.59 | 1.00 | **2.59x** |
| **512 x 512** | 6.88 | 3.21 | **2.15x** |
| **1024 x 1024** | 29.46 | 9.97 | **2.95x** |
| **2048 x 2048** | 117.31 | 39.88 | **2.94x** |

El pipeline mantiene buena escalabilidad incluso con problemas de mayor tamaño, donde el factor de aceleración sobrepasa la barrera del "overhead" del OpenMP y se consolida con un Speedup de ~2.9x de forma robusta.

---

## 4. Discusiones Finales

**Sobre la Fracción Serial:** 
Considerando la de la **Ley de Amdahl**, incluso apuntando a 8 procesadores no se lograron speedups ideales (8.0x). Esto ocurre debido a la inherente **fracción serial ($f$)** atada a la instanciación principal en las escrituras vectorizadas, la administración general del S.O y la reserva lineal de la memoria `(Imagen salida(...))` o creación de hilos a bajo nivel por OpenMP, fijando un tope asintótico sin el cual la velocidad seguiría escalando de forma lineal ilimitadamente. 

**Respuesta a la Parte D (En la estructura del Pipeline de Filtros):** 
¿Es posible resolver los filtros de cadena en forma de pipeline real simultáneo? *No.*
Se demostró en el sistema secuenciado. Por dependencia de datos, requerimos el output explícitamente completo resultante de la convolución del `"Gaussiano"` que sirva de material base consumible por el `"Sharpen"`; obligando a que se paralelicen las funciones individualmente interconectando ciclos pero sin solaparse uno de otro.