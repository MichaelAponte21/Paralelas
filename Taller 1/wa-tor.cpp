#include <iostream>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <cmath>
#include <fstream>
#include <algorithm>


struct animal{
    int id_animal, breed, starve_time;

    inline animal(int id_animal, int starve_time)
    {
        this->id_animal = id_animal;        // saber si es pez (1), tiburon (2) o vacío (0)
        this->breed = 0;                    // contador para reproducirse (inicia en 0, si llega a un umbral, se reproduce y reinicia contador)
        this->starve_time = starve_time;    // contador para morir (inicia en un valor predefinido, si llega a 0 se muere el animal)

        // Los contadores deben aumentar o disminuir en cada época o generación (cada t)
    }
};

// Colocar animales en grilla ya creada
void llenar_grilla(int id_animal, int starve_time, int n_animales, animal* grid, int size_grid){

    int r;
    int animales_colocados = 0;

    while( animales_colocados < n_animales ){
        // Obtener un número random para colocar el animal
        r = rand() % size_grid;         // random entre 0 y size_grid-1

        if (grid[r].id_animal == 0){    // verificar que esté vacía la celda
            
            grid[r] = {id_animal, starve_time};
            animales_colocados++;
        }
    }
}

// Crear grilla y llenarla con la funcion llenar_grilla()
animal* inicializar_grilla(int N, int M, int n_peces, int n_tiburones, int starve_time){
    
    if(n_peces+n_tiburones > N*M){
        std::cout << "El número de especies no cabe en la grilla\n";
        return NULL;
    }

    // Reservar memoria para el array (2D EXTENDIDO)
    // Acceder como grid[i*M + j]
    animal* grid = (animal*)malloc(N * M * sizeof(animal));

    // Inicializar todo con 0
    for (int i = 0; i < (N*M); i++){
        grid[i] = {0, 0};
    }

    llenar_grilla(1, 0,           n_peces,     grid, (N*M));    // 0 porque pez no muere pez id=1
    llenar_grilla(2, starve_time, n_tiburones, grid, (N*M));    // llenar de tiburones id=2

    return grid;
}

// Mover tanto los peces como los tiburones
// se actualizan los contadores de cada animal
// el animal se muere o se reproduce según el caso
void mover_animales(animal* grid, int N, int M, int fish_breed, int shark_breed, int starve_time){
    
    // 8 direcciones: arriba, abajo, izquierda, derecha + diagonales
    int di[] = {-1,  1,  0,  0, -1, -1,  1,  1};
    int dj[] = { 0,  0, -1,  1, -1,  1, -1,  1};
    // Para wa tor se usan solo las 4 primeras

    animal* next_grid = (animal*)malloc(N * M * sizeof(animal));
    memcpy(next_grid, grid, N * M * sizeof(animal)); // copia estado actual a next_grid

    int r, moverA, posActual;

    for (int i=0; i<N; i++){
        for (int j=0; j<M; j++){

            posActual = i*M + j;
            if (grid[posActual].id_animal == 0) continue; // Asegurarse que haya un animal para mover

            int r = rand() % 4;  // mover hacia una de las 4 direcciones
            
            // Aplicar el toroide
            int ni = ((i + di[r]) % N + N) % N;
            int nj = ((j + dj[r]) % M + M) % M;
            moverA = ni * M + nj;
            
            // mover el animal
            if (next_grid[moverA].id_animal == 0){ // Verificar que el espacio esté disponible
                next_grid[moverA] = grid[posActual]; // mover
                
                // Contar el movimiento
                next_grid[moverA].breed ++;
                next_grid[moverA].starve_time --;

                // Si starve_time llegó a cero se muere el animal
                if (next_grid[moverA].starve_time == 0){
                    next_grid[moverA] = {0,0};
                    next_grid[posActual] = {0, 0};
                }

                // Reproducir si es el caso
                if (next_grid[moverA].id_animal==1 && next_grid[moverA].breed == fish_breed){
                    next_grid[posActual] = {1, 0};      // Nace Pez
                    next_grid[moverA].breed = 0;        // Resetear breed
                }
                else if (next_grid[moverA].id_animal==2 && next_grid[moverA].breed == shark_breed){
                    next_grid[posActual] = {2, starve_time};    // Nace tiburon
                    next_grid[moverA].breed = 0;                // Resetear breed
                }
                else{
                    next_grid[posActual] = {0, 0};      // Si no dejó hijo, vaciar la posicion
                }
            }
        }
    }
    memcpy(grid, next_grid, N * M * sizeof(animal)); // copia estado actual a next_grid
    free(next_grid);
}

// depreadores se comen una presa aledaña (si es posible)
// el contador de vida starve_time se reinicia al máximo despues de comer
void depredacion(int id_depredador, int id_presa, animal* grid, int N, int M, int starve_time_depreador){
    // 8 direcciones: arriba, abajo, izquierda, derecha + diagonales
    int di[] = {-1,  1,  0,  0, -1, -1,  1,  1};
    int dj[] = { 0,  0, -1,  1, -1,  1, -1,  1};
    // Para wa tor se usan solo las 4 primeras

    animal* next_grid = (animal*)malloc(N * M * sizeof(animal));
    memcpy(next_grid, grid, N * M * sizeof(animal)); // copia estado actual a next_grid

    int r, posiblePresa, posActual;
    std::vector<int> posPresasCercanas;

    for (int i=0; i<N; i++){
        for (int j=0; j<M; j++){

            posActual = i*M + j;
            // Asegurarse de selecionar el depredador
            if (grid[posActual].id_animal==id_depredador){

                // Buscar presa cercana en las 4 direcciones
                for (r=0; r<4; r++){
                    // Aplicar el toroide
                    int ni = ((i + di[r]) % N + N) % N;
                    int nj = ((j + dj[r]) % M + M) % M;
                    posiblePresa = ni * M + nj;

                    if(grid[posiblePresa].id_animal == id_presa){
                        posPresasCercanas.push_back(posiblePresa);
                    }
                }

                // si no hay presas, no hacer nada
                if (posPresasCercanas.empty()) continue;

                // seleccionar una de las presas cercanas
                r = rand() % (posPresasCercanas.size());

                // comer presa
                next_grid[posPresasCercanas[r]] = grid[posActual];
                next_grid[posActual] = {0,0};

                // reiniciar contador de energía al máximo
                next_grid[posPresasCercanas[r]].starve_time = starve_time_depreador;

                posPresasCercanas.clear();
            }
        }
    }
    memcpy(grid, next_grid, N * M * sizeof(animal)); // copia estado actual a next_grid
    free(next_grid);
}

// Funcion que recorre toda la grilla contando los peces y ls tiburones
void contar_poblacion(animal* grid, int size_grid, std::vector<int>& poblacion_peces, std::vector<int>& poblacion_tiburones){

    int peces=0, tiburones=0;
    
    for (int i=0; i<size_grid; i++){
        if(grid[i].id_animal==1) peces++;       // Contar peces
        if(grid[i].id_animal==2) tiburones++;   // Contar tiburones
    }

    poblacion_peces.push_back(peces);
    poblacion_tiburones.push_back(tiburones);
}

// xd
void imprimirPoblacion(std::vector<int>& tiburones, std::vector<int>& peces){
    std::cout << "t  | tiburones | peces  \n";
    if (tiburones.size() != peces.size()) return;
    for (int i = 0; i < tiburones.size(); i++){
        std::cout << "" << i << " |   " << tiburones[i] << "   |   " << peces[i] << "\n";
    }
}

void exportarCSV(const std::vector<int>& poblacion_peces,
                 const std::vector<int>& poblacion_tiburones) 
{
    std::ofstream archivo("poblaciones.csv");

    if (!archivo.is_open()) {
        std::cerr << "Error al abrir el archivo\n";
        return;
    }

    archivo << "Dia,Peces,Tiburones\n";

    size_t tamaño = std::min(poblacion_peces.size(),
                             poblacion_tiburones.size());

    for (size_t i = 0; i < tamaño; i++) {
        archivo << i << ","
                << poblacion_peces[i] << ","
                << poblacion_tiburones[i] << "\n";
    }

    archivo.close();
}



void graficarConGnuplot() 
{
    // Crear archivo de instrucciones para gnuplot
    std::ofstream script("graficar.gp");

    script << "set datafile separator ','\n";
    script << "set title 'Modelo Depredador-Presa'\n";
    script << "set xlabel 'Dia'\n";
    script << "set ylabel 'Poblacion'\n";
    script << "set grid\n";
    script << "plot 'poblaciones.csv' using 1:2 with lines lw 2 title 'Peces',\\\n";
    script << "     'poblaciones.csv' using 1:3 with lines lw 2 title 'Tiburones'\n";
    script << "pause -1\n";  // Mantiene la ventana abierta

    script.close();

    // Ejecutar gnuplot
    system("gnuplot graficar.gp");
}

int main(){

    // Definir el tamaño de la grilla
    int N = 100;
    int M = 100;

    int n_peces      = 1500;  // ~15% de la grilla
    int n_tiburones  = 300;   // ~3% de la grilla (ratio 5:1 peces/tiburones)

    int starve_time  = 8;     // tiburon muere rápido si no come, más tensión

    int fish_breed   = 5;     // peces se reproducen rápido
    int shark_breed  = 10;    // tiburones más lentos en reproducirse

    int T = 500;              // Epocas de la simulación

    srand(42);  // semilla para reproductibilidad

    // Obtener grilla
    animal* grid = inicializar_grilla(N, M, n_peces, n_tiburones, starve_time);

    // Array para guardar poblaciones en cada generacion
    std::vector<int> poblacion_peces;
    std::vector<int> poblacion_tiburones;

    // Simular para las condiciones anteriores
    for (int t=0; t<T; t++){
        contar_poblacion(grid, (N*M), poblacion_peces, poblacion_tiburones);

        depredacion(2, 1, grid, N, M, starve_time);

        mover_animales(grid, N, M, fish_breed, shark_breed, starve_time);
    }


    //Exportar resultados a CSV
    exportarCSV(poblacion_peces, poblacion_tiburones);

    // Impirmir en consola
    imprimirPoblacion(poblacion_tiburones, poblacion_peces);
    //graficar con gnuplot
    graficarConGnuplot();

    // Vaciar memoria (malloc)
    free(grid);    

    return 0;

}