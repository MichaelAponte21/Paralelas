#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Uso: %s <N>\n", argv[0]);
        return 1;
    }

    int N = atoi(argv[1]);

    if (N <= 0) {
        printf("N debe ser un numero positivo\n");
        return 1;
    }

    // Reservar memoria para la matriz NxN
    int *matriz = (int *)malloc(N * N * sizeof(int));

    if (matriz == NULL) {
        printf("Error reservando memoria\n");
        return 1;
    }

    srand(time(NULL));

    // Llenar matriz
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            matriz[i * N + j] = rand() % 100;
        }
    }

    // Reservar vector
    int *vector = (int *)malloc(N * N * sizeof(int));

    if (vector == NULL) {
        printf("Error reservando memoria\n");
        free(matriz);
        return 1;
    }

    int k = 0;
    clock_t inicio, fin;

    // ⏱️ INICIO DEL TIMER
    inicio = clock();

    // Copiar matriz al vector (orden por filas)
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            vector[k++] = matriz[i * N + j];
        }
    }

    // ⏱️ FIN DEL TIMER
    fin = clock();

    double tiempo = (double)(fin - inicio) / CLOCKS_PER_SEC;

    printf("\n===== RESULTADOS =====\n");
    printf("Tiempo usando MALLOC: %.7f segundos\n", tiempo);

    free(matriz);
    free(vector);

    return 0;
}
