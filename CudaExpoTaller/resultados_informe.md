# Resultados — Taller CUDA: Multiplicación de Matrices

Michael Alexander Aponte Rodriguez-2222954
Juan David Le´on Delgado 2211587
Daniel Fernando Leal Ayala 2191430
Juan Camilo Jaimes 2221882

**Fecha de ejecución:** 6 de abril de 2026  
**Plataforma:** Local (Windows 11, VS Code Notebook)

---

## 1. Entorno de Hardware y Software

```
+-----------------------------------------------------------------------------+
| NVIDIA-SMI 595.97   Driver Version: 595.97   CUDA Version: 13.2            |
+-----------------------------------------------------------------------------+
| GPU: NVIDIA GeForce RTX 4050 Laptop GPU (WDDM)                             |
| Temp: 53°C   Memoria: 6141 MiB total                                        |
+-----------------------------------------------------------------------------+

nvcc: NVIDIA (R) Cuda compiler driver
Cuda compilation tools, release 13.2, V13.2.51
Build cuda_13.2.r13.2/compiler.37434383_0
```

---

## 2. Multiplicación de Matrices Tiled

**Configuración:** `nvcc -O2 -arch=native -DTILE=<N>`  
**Tamaños evaluados:** N = 512, 1024, 2048, 4096

### TILE = 8

| N    | Tiempo (ms) | GFLOPS  |
|------|-------------|---------|
| 512  | 0.42        | 640.94  |
| 1024 | 3.28        | 655.36  |
| 2048 | 26.23       | 655.03  |
| 4096 | 244.38      | 562.40  |

### TILE = 16

| N    | Tiempo (ms) | GFLOPS  |
|------|-------------|---------|
| 512  | 0.32        | 840.21  |
| 1024 | 2.48        | 867.67  |
| 2048 | 19.56       | 878.11  |
| 4096 | 147.52      | 931.69  |

### TILE = 32

| N    | Tiempo (ms) | GFLOPS  |
|------|-------------|---------|
| 512  | 0.33        | 811.59  |
| 1024 | 2.53        | 848.36  |
| 2048 | 20.01       | 858.39  |
| 4096 | 144.90      | 948.52  |

### Comparativa GFLOPS — N = 4096

| TILE | GFLOPS  | Mejora vs TILE=8 |
|------|---------|-----------------|
| 8    | 562.40  | —               |
| 16   | 931.69  | +65.7 %         |
| 32   | 948.52  | +68.7 %         |

---

## 3. Multiplicación Naive

**Configuración:** `nvcc -O2 -arch=native -DBLOCK_SIZE=16`  
**Tamaño evaluado:** N = 1024

| Configuración         | Tiempo (ms) | GFLOPS |
|-----------------------|-------------|--------|
| Naive (BLOCK_SIZE=16) | 3.20        | 670.44 |

---

## 4. Nsight Compute — Perfilado (N = 1024)

**Secciones:** `MemoryWorkloadAnalysis`, `LaunchStats`, `Occupancy`  
**Ejecutado con privilegios de Administrador** — contadores hardware habilitados.

> El tiempo bajo Nsight es mayor al normal porque el profiler instrumenta el kernel en múltiples pasadas (9 passes).

---

### 4.1 Memory Workload Analysis

| Kernel            | Grid / Block        | Mem Throughput (GB/s) | Mem Busy (%) | Max BW (%) | L1/TEX Hit (%) | L2 Hit (%) |
|-------------------|---------------------|-----------------------|--------------|------------|----------------|------------|
| Tiled TILE=8      | (128,128,1)×(8,8,1) | 2.55                  | 75.00        | 83.95      | 24.42          | 99.00      |
| Tiled TILE=16     | (64,64,1)×(16,16,1) | 3.46                  | 61.81        | 96.45      | 7.36           | 98.36      |
| Tiled TILE=32     | (32,32,1)×(32,32,1) | 3.32                  | 51.08        | 85.13      | 0.00           | 96.86      |
| Naive BLOCK=16    | (64,64,1)×(16,16,1) | 2.66                  | 74.15        | 98.81      | **87.60**      | 98.40      |

> **Observaciones:**  
> - Naive tiene L1/TEX Hit Rate del 87.6 % gracias a la reutilización de datos en caché L1, **no** en shared memory.  
> - Tiled TILE=16 tiene L1 Hit Rate solo del 7.3 % porque los datos llegan directamente de shared memory (no pasan por L1).  
> - Tiled TILE=32 tiene L1 Hit Rate del 0 % — los tiles de 32×32 fp32 = 8 KB caben exactamente en shared memory sin spilling, eliminando todo tráfico por L1.

---

### 4.2 Launch Statistics

| Kernel         | Block Size | Grid Size | Reg/Thread | SMEM estática / bloque | Waves/SM |
|----------------|-----------|-----------|------------|------------------------|----------|
| Tiled TILE=8   | 64        | 16,384    | 40         | 512 bytes              | 34.13    |
| Tiled TILE=16  | 256       | 4,096     | 38         | 2,048 bytes (2.05 KB)  | 34.13    |
| Tiled TILE=32  | 1,024     | 1,024     | 38         | 8,192 bytes (8.19 KB)  | 51.20    |
| Naive BLOCK=16 | 256       | 4,096     | 40         | 0 bytes                | 34.13    |

---

### 4.3 Occupancy

| Kernel         | Block Limit SM | Block Limit Regs | Block Limit SMEM | Block Limit Warps | Teor. Occupancy (%) | Achieved Occupancy (%) |
|----------------|---------------|------------------|------------------|-------------------|---------------------|------------------------|
| Tiled TILE=8   | 24            | 24               | 42               | 24                | **100 %**           | **98.82 %**            |
| Tiled TILE=16  | 24            | 6                | 21               | 6                 | **100 %**           | **98.76 %**            |
| Tiled TILE=32  | 24            | 1                | 1                | 1                 | **66.67 %**         | **66.63 %**            |
| Naive BLOCK=16 | 24            | 6                | 16               | 6                 | **100 %**           | **98.71 %**            |

> **Nota clave para TILE=32:** La ocupación teórica cae a **66.67 %** porque el bloque de 1024 threads con 8 KB de SMEM y 38 registros/thread limita a 1 bloque por SM (Block Limit Regs = 1, Block Limit SMEM = 1). Nsight recomienda reducir registros o SMEM para ganar ~33 % de speedup adicional.

---

## 5. Análisis Teórico — Roofline e Intensidad Aritmética

**Dispositivo:** NVIDIA GeForce RTX 4050 Laptop GPU

### Especificaciones del dispositivo

| Parámetro                | Valor          |
|--------------------------|----------------|
| SM count                 | 20             |
| Shared mem / SM          | 100 KB         |
| Shared mem / bloque      | 48 KB          |
| Warp size                | 32             |
| Max threads / bloque     | 1024           |
| Ancho de banda pico      | 192.0 GB/s     |
| Rendimiento pico fp32    | 10.9 TFLOPS    |
| **Ridge point**          | **56.8 FLOP/byte** |

### Intensidad aritmética por implementación

| Implementación    | FLOP/byte | % del Ridge point |
|-------------------|-----------|-------------------|
| Naive             | 0.25      | 0.4 %             |
| Tiled TILE=8      | 2.00      | 3.5 %             |
| Tiled TILE=16     | 4.00      | 7.0 %             |
| Tiled TILE=32     | 8.00      | 14.1 %            |
| Ridge point       | 56.8      | 100 %             |

> Se necesitaría un **TILE ≥ 227** para alcanzar teóricamente el ridge point.

### Accesos a memoria global

- **Naive:** `2 × N³` bytes por elemento
- **Tiled:** `2 × N³ / TILE` bytes por elemento (factor TILE× menos accesos)

### Ocupación según memoria compartida por bloque

| TILE | SMEM / bloque | Bloques posibles / SM |
|------|--------------|-----------------------|
| 8    | 512 bytes    | hasta 200             |
| 16   | 2 048 bytes  | hasta 50              |
| 32   | 8 192 bytes  | hasta 12              |

---

## 6. Demostración de `__syncthreads()`

**Configuración:** N = 1024, TILE = 16, kernel tiled  
**Referencia:** resultado de multiplicación serial

| Modo                                    | Error máx. absoluto | Elementos con error > 1e-3 | Resultado   |
|-----------------------------------------|---------------------|-----------------------------|-------------|
| Ambas barreras (correcto)               | 0.00e+00            | 0 / 1 048 576 (0.00 %)      | ✅ CORRECTO |
| Sin primera barrera (después de cargar) | 2.40e+01            | 1 048 058 / 1 048 576 (99.95 %) | ❌ FALLO (carrera de datos) |
| Sin segunda barrera (antes de sobrescribir) | 3.83e-01        | 798 857 / 1 048 576 (76.18 %) | ❌ FALLO (carrera de datos) |

### Interpretación

- **Sin primera barrera:** los threads empiezan a calcular antes de que todos hayan terminado de cargar el tile en shared memory → datos inconsistentes (99.95 % de errores).
- **Sin segunda barrera:** algunos threads sobrescriben el tile antes de que otros terminen de usarlo → resultados parcialmente incorrectos (76.18 % de errores).

---

## 7. Conclusiones

1. **TILE=32 ofrece el mejor rendimiento absoluto** en matrices grandes (N=4096): ~949 GFLOPS frente a ~562 GFLOPS de TILE=8 (+68.7 %).
2. **TILE=16 vs TILE=32:** diferencia marginal (~1.8 %), pero TILE=32 usa 16× más shared memory por bloque, reduciendo la ocupación de 50 a 12 bloques/SM.
3. Ninguna configuración supera el 15 % del ridge point teórico, lo que indica que la implementación tiled sigue siendo **memory-bound** para los tamaños evaluados.
4. Se necesita un TILE de al menos 227 para acercarse al pico teórico, lo que excede la capacidad de shared memory disponible.
5. La eliminación de cualquiera de las dos barreras `__syncthreads()` produce **carreras de datos** con errores masivos, confirmando que las barreras son imprescindibles para la corrección del algoritmo tiled.
