#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>

using namespace std;

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

    // Crear matriz NxN usando vector<vector<int>>
    vector<vector<int>> matriz(N, vector<int>(N));

    // Inicializar números aleatorios
    srand(time(NULL));

    // Llenar matriz
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            matriz[i][j] = rand() % 100;
        }
    }

    // Crear vector para almacenar la matriz en orden por filas
    vector<int> vectorFila(N * N);

    int k = 0;

    // Copiar matriz al vector (orden por filas)
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            vectorFila[k++] = matriz[i][j];
        }
    }

    // Mostrar vector
    cout << "Vector:" << endl;

    for (int i = 0; i < N * N; i++) {
        cout << vectorFila[i] << " ";
    }

    cout << endl;

    return 0;
}
