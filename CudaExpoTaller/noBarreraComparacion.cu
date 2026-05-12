#include <cuda_runtime.h>
#include <stdio.h>
#include <algorithm>
#include <iostream>
#include <string.h>

#define TILE 16

// Variable global para controlar qué barrera eliminar (se configura desde main)
// 0 = ambas barreras (correcto)
// 1 = eliminar primera barrera
// 2 = eliminar segunda barrera
int g_sin_barrera = 0;

// ── Serial (para verificación)
void matmul_serial(const float* A, const float* B, float* C, int N) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            float acc = 0.0f;
            for (int k = 0; k < N; k++)
                acc += A[i*N + k] * B[k*N + j];
            C[i*N + j] = acc;
        }
}

// ── Kernel tiled DEMO (con barreras condicionales)
__global__ void matmul_tiled_demo(const float* A, const float* B, float* C, int N, int sin_barrera) {
    __shared__ float tA[TILE][TILE];
    __shared__ float tB[TILE][TILE];
    int tx = threadIdx.x, ty = threadIdx.y;
    int col  = blockIdx.x * TILE + tx;
    int fila = blockIdx.y * TILE + ty;
    float acc = 0.0f;

    for (int t = 0; t < (N + TILE - 1) / TILE; t++) {
        int idxAk = t * TILE + tx;
        int idxBk = t * TILE + ty;

        tA[ty][tx] = (fila < N && idxAk < N) ? A[fila * N + idxAk] : 0.0f;
        tB[ty][tx] = (idxBk < N && col  < N) ? B[idxBk * N + col]  : 0.0f;

        // Primera barrera (después de cargar)
        if (sin_barrera != 1) {
            __syncthreads();
        }

        for (int k = 0; k < TILE; k++)
            acc += tA[ty][k] * tB[k][tx];

        // Segunda barrera (antes de sobrescribir en la siguiente iteración)
        if (sin_barrera != 2) {
            __syncthreads();
        }
    }

    if (fila < N && col < N)
        C[fila * N + col] = acc;
}

// Kernel normal (sin condicionales, para referencia de rendimiento)
__global__ void matmul_tiled_normal(const float* A, const float* B, float* C, int N) {
    __shared__ float tA[TILE][TILE];
    __shared__ float tB[TILE][TILE];
    int tx = threadIdx.x, ty = threadIdx.y;
    int col  = blockIdx.x * TILE + tx;
    int fila = blockIdx.y * TILE + ty;
    float acc = 0.0f;

    for (int t = 0; t < (N + TILE - 1) / TILE; t++) {
        int idxAk = t * TILE + tx;
        int idxBk = t * TILE + ty;

        tA[ty][tx] = (fila < N && idxAk < N) ? A[fila * N + idxAk] : 0.0f;
        tB[ty][tx] = (idxBk < N && col  < N) ? B[idxBk * N + col]  : 0.0f;
        __syncthreads();

        for (int k = 0; k < TILE; k++)
            acc += tA[ty][k] * tB[k][tx];
        __syncthreads();
    }

    if (fila < N && col < N)
        C[fila * N + col] = acc;
}

void ejecutar_y_verificar(int N, int modo, const char* nombre_modo) {
    size_t bytes = N * N * sizeof(float);
    float *h_A = new float[N*N];
    float *h_B = new float[N*N];
    float *h_C = new float[N*N];
    float *h_C_serial = new float[N*N];

    // Inicialización determinista
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            h_A[i*N+j] = (float)(i + j) / N;
            h_B[i*N+j] = (float)(i - j + N) / N;
        }

    // Versión serial (referencia)
    matmul_serial(h_A, h_B, h_C_serial, N);

    float *d_A, *d_B, *d_C;
    cudaMalloc(&d_A, bytes);
    cudaMalloc(&d_B, bytes);
    cudaMalloc(&d_C, bytes);
    cudaMemcpy(d_A, h_A, bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, h_B, bytes, cudaMemcpyHostToDevice);

    dim3 bloque(TILE, TILE);
    dim3 grid((N+TILE-1)/TILE, (N+TILE-1)/TILE);

    // Ejecutar según el modo
    if (modo == 0) {
        // Versión normal (sin condiciones en kernel, mejor rendimiento)
        matmul_tiled_normal<<<grid, bloque>>>(d_A, d_B, d_C, N);
    } else {
        // Versión demo con barreras condicionales
        matmul_tiled_demo<<<grid, bloque>>>(d_A, d_B, d_C, N, modo);
    }
    cudaDeviceSynchronize();

    // Verificar errores
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf("Error en kernel: %s\n", cudaGetErrorString(err));
    }

    // Copiar resultado y verificar
    cudaMemcpy(h_C, d_C, bytes, cudaMemcpyDeviceToHost);
    
    float max_err = 0.0f;
    int num_fallos = 0;
    for (int i = 0; i < N*N; i++) {
        float err = fabsf(h_C[i] - h_C_serial[i]);
        if (err > max_err) max_err = err;
        if (err > 1e-3f) num_fallos++;
    }
    
    printf("Modo: %s\n", nombre_modo);
    printf("  Error máximo absoluto vs serial: %.2e\n", max_err);
    printf("  Elementos con error > 1e-3: %d / %d (%.2f%%)\n", 
           num_fallos, N*N, 100.0f * num_fallos / (N*N));
    
    if (max_err < 1e-3f)
        printf("  ✅ CORRECTO\n");
    else
        printf("  ❌ FALLO (carrera de datos detectada)\n");
    printf("\n");

    // Liberar
    cudaFree(d_A); cudaFree(d_B); cudaFree(d_C);
    delete[] h_A; delete[] h_B; delete[] h_C; delete[] h_C_serial;
}

int cores_por_sm(int major, int minor) {
      if (major == 7 && minor == 5) return 64;   // Turing  (T4)
      if (major == 8 && minor == 0) return 64;   // Ampere A100
      if (major == 8 && minor == 6) return 128;  // Ampere RTX 30xx
      if (major == 8 && minor == 9) return 128;  // Ada    RTX 40xx
      if (major == 9 && minor == 0) return 128;  // Hopper
      return 64; // fallback conservador
    }


int main(int argc, char* argv[]) {
    const int N = 1024;  // Usar N pequeño para verificación rápida


    // ── Info del dispositivo ─────────────────────────────────────────────────
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    printf("=== Dispositivo: %s ===\n", prop.name);
    printf("    SM count          : %d\n", prop.multiProcessorCount);
    printf("    Shared mem / SM   : %zu KB\n", prop.sharedMemPerMultiprocessor/1024);
    printf("    Shared mem / bloque: %zu KB\n", prop.sharedMemPerBlock/1024);
    printf("    Warp size         : %d\n", prop.warpSize);
    printf("    Max threads/bloque: %d\n\n", prop.maxThreadsPerBlock);

    // ── Análisis teórico ─────────────────────────────────────────────────────
    printf("=== Análisis teórico ===\n");
    printf("    Accesos globales naive  : 2 * N^3  bytes por elemento\n");
    printf("    Accesos globales tiled  : 2 * N^3 / TILE  (factor TILEx menos)\n");

    // Ancho de banda de memoria (GB/s)
    // memoryClockRate en kHz (via cudaDeviceGetAttribute), memoryBusWidth en bits
    int memClkKHz = 0, clkKHz = 0;
    cudaDeviceGetAttribute(&memClkKHz, cudaDevAttrMemoryClockRate, 0);
    cudaDeviceGetAttribute(&clkKHz,    cudaDevAttrClockRate,       0);
    double bw_GBs = 2.0 * memClkKHz * 1e3   // kHz → Hz, DDR = ×2
              * (prop.memoryBusWidth / 8.0)  // bits → bytes
              / 1e9;                          // → GB/s
    // Rendimiento pico fp32 (TFLOPS)
    // clockRate en kHz, 2 ops/ciclo (FMA)
    double tflops = (double)prop.multiProcessorCount
                  * clkKHz * 1e3                         // kHz → Hz
                  * 2.0                                   // FMA = 1 mul + 1 add
                  * cores_por_sm(prop.major, prop.minor)  // CUDA cores/SM
                  / 1e12;
    // Ridge point (FLOP/byte)
    double ridge = (tflops * 1e12) / (bw_GBs * 1e9);

    printf("    Ancho de banda pico : %.1f GB/s\n", bw_GBs);
    printf("    Rendimiento pico fp32: %.1f TFLOPS\n", tflops);
    printf("    Ridge point          : %.1f FLOP/byte\n", ridge);

    double flops_por_byte_naive = 1.0/4.0;   // AI = (2*N³) / (2*N³*4) = 1/4 = 0.25 FLOP/byte/
    printf("    Naive llega a   : %.2f FLOP/byte  "
           "(%.1f%% del ridge point)\n",
           flops_por_byte_naive,
           100.0 * flops_por_byte_naive / ridge);

    for (int tile : {8, 16, 32}) {
      double flops_por_byte_tiled = (double)tile / 4.0; // (2*N*TILE²) / (2*N*TILE*4) = TILE/4
      printf("    Tiled con TILE=%.0f llega a   : %.0f FLOP/byte  "
            "(%.1f%% del ridge point)\n",
            (double)tile, flops_por_byte_tiled,
            100.0 * flops_por_byte_tiled / ridge);
    }
    printf("    Se necesita un TILE>=%.1f para alcanzar Ridge point.\n\n", ridge*4);

    // ── Memoria compartida usada por configuración de TILE ───────────────────
    printf("=== Memoria compartida por bloque ===\n");
    for (int tile : {8, 16, 32}) {
        size_t smem = 2 * tile * tile * sizeof(float);
        int bloques_por_sm = (int)(prop.sharedMemPerMultiprocessor / smem);
        printf("    TILE=%2d : %4zu bytes SMEM  →  hasta %d bloques/SM\n",
               tile, smem, bloques_por_sm);
    }
    printf("\n");

    
    printf("=== DEMOSTRACION DE __syncthreads()\n");
    printf("Comparación con serial\n");
    printf("N = %d, TILE = %d\n\n", N, TILE);
    
    // Modo 0: Ambas barreras (correcto)
    ejecutar_y_verificar(N, 0, "AMBAS BARRERAS (correcto)");
    
    // Modo 1: Sin primera barrera
    ejecutar_y_verificar(N, 1, "SIN PRIMERA BARRERA (despues de cargar)");
    
    // Modo 2: Sin segunda barrera
    ejecutar_y_verificar(N, 2, "SIN SEGUNDA BARRERA (antes de sobrescribir)");
    
    return 0;
}
