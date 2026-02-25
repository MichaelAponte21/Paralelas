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

    // Crear matriz NxN usando vector (almacenada como vector 1D)
    vector<int> matriz(N * N);

    // Inicializar números aleatorios
    srand(time(NULL));

    // Llenar matriz
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            matriz[i * N + j] = rand() % 100;
        }
    }

    // Crear vector para almacenar la matriz en orden por filas
    vector<int> vectorFila(N * N);

    int k = 0;

    // Copiar matriz al vector
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            vectorFila[k++] = matriz[i * N + j];
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
