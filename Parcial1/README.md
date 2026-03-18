# Resultados Taller Evaluativo: Filtros de Convolución (Propuesta 1)

Este documento contiene los resultados de rendimiento y perfilamiento obtenidos al aplicar paralelización con OpenMP sobre diferentes filtros de convolución de imágenes. Las pruebas se realizaron en un entorno con 12 procesadores lógicos disponibles.

## Tabla de Speedup para cada Kernel y número de hilos

*Nota: Tiempos medidos sobre una imagen autogenerada de `1024x1024` píxeles. El Speedup (SU) se calcula como `Tiempo_Secuencial / Tiempo_Paralelo`.*

| Kernel | Secuencial (ms) | 1 hilo (ms) | SU (1h) | 2 hilos (ms) | SU (2h) | 4 hilos (ms) | SU (4h) | 8 hilos (ms) | SU (8h) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Blur 3x3** | 11.36 | 11.19 | 1.01x | 7.11 | 1.60x | 4.08 | 2.78x | 4.83 | 2.35x |
| **Gaussiano 5x5** | 27.81 | 26.39 | 1.05x | 14.27 | 1.95x | 7.74 | 3.59x | 9.64 | 2.89x |
| **Sobel X** | 13.42 | 10.53 | 1.27x | 6.40 | 2.10x | 5.48 | 2.45x | 3.71 | 3.62x |
| **Sharpen** | 11.16 | 10.93 | 1.02x | 8.95 | 1.25x | 5.27 | 2.12x | 3.75 | 2.98x |
| **Emboss** | 11.71 | 17.98 | 0.65x | 9.82 | 1.19x | 5.47 | 2.14x | 5.59 | 2.10x |

**Observación sobre el Tamaño del Kernel:** 
El kernel "Gaussiano 5x5" muestra consistentemente un mayor speedup relativo que los kernels de 3x3. Esto indica que a mayor cantidad de operaciones aritméticas (mayor intensidad computacional por píxel), la paralelización rinde mejores frutos, compensando de manera mucho más eficiente el *overhead* o costo de administrar los hilos en OpenMP.

---

## Tabla de Escalabilidad 
*(Escaneado sobre Gaussiano 5x5, utilizando 4 hilos)*

Esta tabla muestra cómo se comporta el rendimiento y la aceleración paralelizada a medida que incrementamos el tamaño del grid (la densidad de carga).

| Tamaño de la Imagen | Tiempo Secuencia (ms) | Tiempo Paralelo 4h (ms) | Speedup (SU) |
| :--- | :---: | :---: | :---: |
| **256 x 256** | 2.59 | 1.00 | **2.59x** |
| **512 x 512** | 6.88 | 3.21 | **2.15x** |
| **1024 x 1024** | 29.46 | 9.97 | **2.95x** |
| **2048 x 2048** | 117.31 | 39.88 | **2.94x** |

**Análisis de Escalabilidad:**
Se observa que a medida que crece el tamaño del problema, el factor de aceleración tiende a estabilizarse o mejorar (acercándose notablemente a un factor casi ideal de ~3.0x para 4 hilos a partir de las matrices grandes). Esto ocurre porque en resoluciones pequeñas (ej. 256x256), el tiempo empleado en la fase de sincronización y creación/distribución de hilos (overhead) se acerca demasiado al tiempo de trabajo útil, limitando el speedup. En cambio, con cargas de trabajo enormes (2048x2048), la proporción de tiempo dedicado puramente al procesamiento se hace dominante.