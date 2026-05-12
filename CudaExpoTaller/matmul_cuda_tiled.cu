// matmul_cuda_tiled.cu
// !nvcc -O2 -arch=native -DTILE=16 -o matmul_cuda_tiled matmul_cuda_tiled.cu

#include <cuda_runtime.h>
#include <stdio.h>
#include <algorithm>
#include <iostream>

// ── TILE se puede definir desde nvcc -DTILE=N ────────────────────────────────
#ifndef TILE
#define TILE 16
#endif

// ── Correr todos los N con -DTODOS=1 o solo N=1024 con -DTODOS=0
#ifndef TODOS
#define TODOS 1
#endif

#define NREP 5 // Usar número impar (Facilitar mediana)

// ── Kernel tiled ──────────────────────────────────────────────────────────────
__global__ void matmul_tiled(const float* A, const float* B, float* C, int N) {
    __shared__ float tA[TILE][TILE];
    __shared__ float tB[TILE][TILE];
    int tx = threadIdx.x, ty = threadIdx.y;
    int col  = blockIdx.x*TILE + tx;
    int fila = blockIdx.y*TILE + ty;
    float acc = 0.0f;
    for (int t = 0; t < (N+TILE-1)/TILE; t++) {
        int idxAk = t*TILE + tx, idxBk = t*TILE + ty;
        tA[ty][tx] = (fila < N && idxAk < N) ? A[fila*N + idxAk] : 0.0f;
        tB[ty][tx] = (idxBk < N && col  < N) ? B[idxBk*N + col]  : 0.0f;
        __syncthreads();
        for (int k = 0; k < TILE; k++) acc += tA[ty][k] * tB[k][tx];
        __syncthreads();
    }
    if (fila < N && col < N) C[fila*N + col] = acc;
}

void ejecutar(const int N) {
    //N = 1024;

    size_t bytes = N * N * sizeof(float);
    float *h_A = new float[N*N], *h_B = new float[N*N], *h_C = new float[N*N];

    // Inicialización determinista (protocolo del taller)
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            h_A[i*N+j] = (float)(i + j) / N;
            h_B[i*N+j] = (float)(i - j + N) / N;
        }

    float *d_A, *d_B, *d_C;
    cudaMalloc(&d_A, bytes); cudaMalloc(&d_B, bytes); cudaMalloc(&d_C, bytes);
    cudaMemcpy(d_A, h_A, bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, h_B, bytes, cudaMemcpyHostToDevice);

    dim3 bloque(TILE, TILE);
    dim3 grid((N+TILE-1)/TILE, (N+TILE-1)/TILE);

    // Warm-up (Calentamiento antes de)
    matmul_tiled<<<grid, bloque>>>(d_A, d_B, d_C, N);
    cudaDeviceSynchronize();

    // Medición (mediana de 5 repeticiones)
    cudaEvent_t t0, t1;
    cudaEventCreate(&t0); cudaEventCreate(&t1);

    float tiempos[NREP];
    for (int r = 0; r < NREP; r++) {
        cudaEventRecord(t0);
        matmul_tiled<<<grid, bloque>>>(d_A, d_B, d_C, N);
        cudaEventRecord(t1);
        cudaEventSynchronize(t1);
        cudaEventElapsedTime(&tiempos[r], t0, t1);

        // Obtener error si ocurre
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess)
            printf("Error kernel: %s\n", cudaGetErrorString(err));
    }
    std::sort(tiempos, tiempos+NREP);
    float ms = tiempos[NREP/2]; // mediana

    // Cálculo de GFLOPS
    double gflops = 2.0*N*N*N / (ms * 1e6);

    printf("Tiled N=%d TILE=%d: %.2f ms  %.2f GFLOPS\n", 
       N, TILE, ms, gflops);

    // Liberar
    cudaFree(d_A); cudaFree(d_B); cudaFree(d_C);
    delete[] h_A; delete[] h_B; delete[] h_C;
}

int main(){
  if (TODOS==0){
    ejecutar(1024);
  }else{
    ejecutar(512);
    ejecutar(1024);
    ejecutar(2048);
    ejecutar(4096);
  }
}
