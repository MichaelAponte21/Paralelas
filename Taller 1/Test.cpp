#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>

using namespace std;
using namespace chrono;

int main(int argc, char *argv[]) {

    if (argc != 2) {
        cout << "Uso: " << argv[0] << " <N>" << endl;
        return 1;
    }

    int N = atoi(argv[1]);

    if (N <= 0) {
        cout << "N debe ser un numero positivo" << endl;
        return 1;
    }

    srand(time(NULL));


    vector<int> matrizBase(N * N);

    for (int i = 0; i < N * N; i++) {
        matrizBase[i] = rand() % 100;
    }

    cout << "\n===== RESULTADOS DE TIEMPO =====\n";

    // BLOQUE 1 → NEW

    auto inicio1 = high_resolution_clock::now();

    int *matrizNew = new int[N * N];
    int *vectorNew = new int[N * N];

    // Copiar matriz base
    for (int i = 0; i < N * N; i++)
        matrizNew[i] = matrizBase[i];

    int k1 = 0;

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            vectorNew[k1++] = matrizNew[i * N + j];

    auto fin1 = high_resolution_clock::now();

    delete[] matrizNew;
    delete[] vectorNew;

    cout << "Tiempo usando NEW: "
         << duration<double>(fin1 - inicio1).count()
         << " segundos\n";

    // BLOQUE 2 → VECTOR 1D

    auto inicio2 = high_resolution_clock::now();

    vector<int> matrizVector(N * N);
    vector<int> vectorFila(N * N);

    matrizVector = matrizBase;

    int k2 = 0;

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            vectorFila[k2++] = matrizVector[i * N + j];

    auto fin2 = high_resolution_clock::now();

    cout << "Tiempo usando VECTOR: "
         << duration<double>(fin2 - inicio2).count()
         << " segundos\n";

    // BLOQUE 3 → VECTOR VECTOR

    auto inicio3 = high_resolution_clock::now();

    vector<vector<int>> matrizVV(N, vector<int>(N));
    vector<int> vectorVV(N * N);

    // Convertir matriz base a matriz 2D
    int index = 0;
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            matrizVV[i][j] = matrizBase[index++];

    int k3 = 0;

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            vectorVV[k3++] = matrizVV[i][j];

    auto fin3 = high_resolution_clock::now();

    cout << "Tiempo usando VECTOR<VECTOR>: "
         << duration<double>(fin3 - inicio3).count()
         << " segundos\n";

    cout << "================================\n";

    return 0;
}
