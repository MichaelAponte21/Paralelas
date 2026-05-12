// matmul_cuda_tiled.cu
// !nvcc -O2 -arch=native -o matmul_cuda_tiled matmul_cuda_tiled.cu

#include <cuda_runtime.h>
#include <stdio.h>
#include <algorithm>
#include <iostream>

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 16
#endif

#define NREP 5 // Usar número impar (Facilitar mediana)

// ── Kernel naive ──────────────────────────────────────────────────────────────
__global__ void matmul_naive(const float* A, const float* B, float* C, int N) {
    int col  = blockIdx.x * blockDim.x + threadIdx.x;
    int fila = blockIdx.y * blockDim.y + threadIdx.y;
    if (fila < N && col < N) {
        float acc = 0.0f;
        for (int k = 0; k < N; k++)
            acc += A[fila*N + k] * B[k*N + col];
        C[fila*N + col] = acc;
    }
}

void ejecutar(const int N) {
    //const int N = 1024;

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

    dim3 bloque(BLOCK_SIZE, BLOCK_SIZE);
    dim3 grid((N+BLOCK_SIZE-1)/BLOCK_SIZE, (N+BLOCK_SIZE-1)/BLOCK_SIZE);

    // Warm-up (Calentamiento antes de)
    matmul_naive<<<grid, bloque>>>(d_A, d_B, d_C, N);
    cudaDeviceSynchronize();

    // Medición (mediana de 5 repeticiones)
    cudaEvent_t t0, t1;
    cudaEventCreate(&t0); cudaEventCreate(&t1);

    float tiempos[NREP];
    for (int r = 0; r < NREP; r++) {
        cudaEventRecord(t0);
        matmul_naive<<<grid, bloque>>>(d_A, d_B, d_C, N);
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
    printf("CUDA naive N=%d BLOCK_SIZE=%d: %.2f ms  %.2f GFLOPS\n", N, BLOCK_SIZE, ms, gflops);

    cudaMemcpy(h_C, d_C, bytes, cudaMemcpyDeviceToHost);
    cudaFree(d_A); cudaFree(d_B); cudaFree(d_C);
    delete[] h_A; delete[] h_B; delete[] h_C;
}

int main(){
  //ejecutar(512);
  ejecutar(1024);
  //ejecutar(2048);
  //ejecutar(4096);
}
