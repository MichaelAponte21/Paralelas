/**
 * Taller — Propuesta 1: Filtros de convolución sobre imágenes
 * Curso: Programación Paralela
 *
 * INSTRUCCIONES:
 *   Complete las secciones marcadas con "// TODO:" siguiendo las indicaciones
 *   del enunciado del taller. No modifique las funciones de lectura/escritura
 *   PGM, generación de imagen de prueba ni verificación.
 *
 * COMPILACIÓN:
 *   g++ -std=c++17 -O2 -fopenmp taller_filtros.cpp -o taller_filtros.o -lm
 *
 * EJECUCIÓN:
 *   ./taller_filtros [archivo.pgm]
 *   Si no se proporciona archivo, se genera una imagen de prueba.
 *
 * FORMATO PGM:
 *   Portable GrayMap — formato de imagen en escala de grises, texto plano.
 *   Se puede abrir con GIMP, ImageMagick, IrfanView, etc.
 *   Convertir desde/hacia otros formatos:
 *     convert foto.jpg foto.pgm          (ImageMagick)
 *     ffmpeg -i foto.png foto.pgm        (FFmpeg)
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <omp.h>

// ============================================================
// Representación de una imagen en escala de grises
// Almacenamiento lineal: pixel(i,j) = datos[i * ancho + j]
// Valores en rango [0, 255]
// ============================================================
struct Imagen {
    int ancho;
    int alto;
    std::vector<double> datos;

    Imagen() : ancho(0), alto(0) {}
    Imagen(int w, int h) : ancho(w), alto(h), datos(w * h, 0.0) {}

    double& pixel(int fila, int col) { return datos[fila * ancho + col]; }
    double  pixel(int fila, int col) const { return datos[fila * ancho + col]; }
};

// ============================================================
// Representación de un kernel de convolución
// ============================================================
struct Kernel {
    int tamanio;   // siempre impar (3, 5, 7...)
    std::vector<double> datos;
    std::string nombre;

    double valor(int fila, int col) const { return datos[fila * tamanio + col]; }
};

// ============================================================
// Lectura de imagen PGM (formato P2 — texto)
// ============================================================
Imagen leer_pgm(const std::string& archivo) {
    std::ifstream in(archivo);
    if (!in.is_open()) {
        std::cerr << "Error: no se pudo abrir " << archivo << std::endl;
        std::exit(1);
    }

    std::string linea;
    // Leer encabezado P2
    std::getline(in, linea);
    if (linea != "P2") {
        std::cerr << "Error: formato no soportado (se espera P2). Encontrado: " << linea << std::endl;
        std::exit(1);
    }

    // Saltar comentarios
    while (std::getline(in, linea) && linea[0] == '#') {}

    // Leer dimensiones
    std::istringstream dims(linea);
    int ancho, alto;
    dims >> ancho >> alto;

    // Leer valor máximo
    int max_val;
    in >> max_val;

    Imagen img(ancho, alto);
    for (int i = 0; i < alto; ++i) {
        for (int j = 0; j < ancho; ++j) {
            int val;
            in >> val;
            img.pixel(i, j) = static_cast<double>(val);
        }
    }

    in.close();
    std::cout << "  Imagen cargada: " << archivo
              << " (" << ancho << "x" << alto << ")" << std::endl;
    return img;
}

// ============================================================
// Escritura de imagen PGM (formato P2 — texto)
// ============================================================
void escribir_pgm(const Imagen& img, const std::string& archivo) {
    std::ofstream out(archivo);
    out << "P2" << std::endl;
    out << "# Generado por taller de filtros paralelos" << std::endl;
    out << img.ancho << " " << img.alto << std::endl;
    out << "255" << std::endl;

    for (int i = 0; i < img.alto; ++i) {
        for (int j = 0; j < img.ancho; ++j) {
            // Clampar a [0, 255] y redondear
            int val = static_cast<int>(std::round(
                std::max(0.0, std::min(255.0, img.pixel(i, j)))
            ));
            out << val;
            if (j < img.ancho - 1) out << " ";
        }
        out << std::endl;
    }
    out.close();
}

// ============================================================
// Genera una imagen de prueba con patrones variados
// (gradientes, círculos, bordes) para evaluar los filtros
// ============================================================
Imagen generar_imagen_prueba(int ancho, int alto) {
    Imagen img(ancho, alto);

    for (int i = 0; i < alto; ++i) {
        for (int j = 0; j < ancho; ++j) {
            double val = 0.0;

            // Gradiente diagonal
            val += 128.0 * (static_cast<double>(i + j) / (alto + ancho));

            // Círculo central
            double cx = ancho / 2.0, cy = alto / 2.0;
            double r = std::sqrt((j - cx) * (j - cx) + (i - cy) * (i - cy));
            double radio = std::min(ancho, alto) / 4.0;
            if (r < radio) val += 80.0;

            // Cuadrícula de líneas (cada 50 píxeles)
            if (i % 50 < 2 || j % 50 < 2) val += 60.0;

            // Ruido suave
            val += (std::rand() % 20) - 10;

            img.pixel(i, j) = std::max(0.0, std::min(255.0, val));
        }
    }
    return img;
}

// ============================================================
// Definición de kernels clásicos
// ============================================================

// Blur promedio 3×3 (box blur)
Kernel kernel_blur_3x3() {
    return {3, {
        1.0/9, 1.0/9, 1.0/9,
        1.0/9, 1.0/9, 1.0/9,
        1.0/9, 1.0/9, 1.0/9
    }, "Blur 3x3"};
}

// Blur gaussiano 5×5 (sigma ≈ 1)
Kernel kernel_gaussiano_5x5() {
    return {5, {
        1.0/273,  4.0/273,  7.0/273,  4.0/273, 1.0/273,
        4.0/273, 16.0/273, 26.0/273, 16.0/273, 4.0/273,
        7.0/273, 26.0/273, 41.0/273, 26.0/273, 7.0/273,
        4.0/273, 16.0/273, 26.0/273, 16.0/273, 4.0/273,
        1.0/273,  4.0/273,  7.0/273,  4.0/273, 1.0/273
    }, "Gaussiano 5x5"};
}

// Detección de bordes — Sobel horizontal (bordes verticales)
Kernel kernel_sobel_x() {
    return {3, {
        -1, 0, 1,
        -2, 0, 2,
        -1, 0, 1
    }, "Sobel X"};
}

// Detección de bordes — Sobel vertical (bordes horizontales)
Kernel kernel_sobel_y() {
    return {3, {
        -1, -2, -1,
         0,  0,  0,
         1,  2,  1
    }, "Sobel Y"};
}

// Enfoque (sharpen)
Kernel kernel_sharpen() {
    return {3, {
         0, -1,  0,
        -1,  5, -1,
         0, -1,  0
    }, "Sharpen"};
}

// Realce de bordes (emboss)
Kernel kernel_emboss() {
    return {3, {
        -2, -1, 0,
        -1,  1, 1,
         0,  1, 2
    }, "Emboss"};
}

// ============================================================
// PARTE A: Convolución secuencial
//
// Para cada píxel (i, j) de la imagen de salida, se calcula
// la suma ponderada de los vecinos según el kernel:
//
//   salida[i][j] = Σ_di Σ_dj kernel[di][dj] * entrada[i+di-r][j+dj-r]
//
// donde r = tamanio_kernel / 2 (radio del kernel).
// Los píxeles fuera de la imagen se tratan como 0 (zero-padding).
// ============================================================
Imagen convolucion_secuencial(const Imagen& entrada, const Kernel& k) {
    Imagen salida(entrada.ancho, entrada.alto);
    int r = k.tamanio / 2;  // radio

    auto inicio = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < entrada.alto; ++i) {
        for (int j = 0; j < entrada.ancho; ++j) {
            double suma = 0.0;
            for (int di = 0; di < k.tamanio; ++di) {
                for (int dj = 0; dj < k.tamanio; ++dj) {
                    int fi = i + di - r;  // fila en la imagen
                    int fj = j + dj - r;  // columna en la imagen
                    // Zero-padding: fuera de la imagen se asume 0
                    if (fi >= 0 && fi < entrada.alto && fj >= 0 && fj < entrada.ancho) {
                        suma += k.valor(di, dj) * entrada.pixel(fi, fj);
                    }
                }
            }
            salida.pixel(i, j) = suma;
        }
    }

    auto fin = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(fin - inicio).count();
    std::cout << "  [Secuencial] " << k.nombre << ": " << ms << " ms" << std::endl;

    return salida;
}

// ============================================================
// PARTE B: Convolución paralela con OpenMP
// ============================================================
Imagen convolucion_paralela(const Imagen& entrada, const Kernel& k, int num_hilos) {
    Imagen salida(entrada.ancho, entrada.alto);
    int r = k.tamanio / 2;

    auto inicio = std::chrono::high_resolution_clock::now();

    // TODO: Paralelice el bucle externo (sobre filas i) usando
    //       #pragma omp parallel for num_threads(num_hilos) schedule(static)
    //
    //       Preguntas para el informe:
    //       - ¿Hay condiciones de carrera? ¿Por qué sí o por qué no?
    //       - ¿Es necesario declarar alguna variable como private?
    //       - ¿Por qué schedule(static) es apropiado aquí?
    //       - ¿Sería útil collapse(2)? ¿Qué ventaja o desventaja tendría?
    #pragma omp parallel for num_threads(num_hilos) schedule(static)
    for (int i = 0; i < entrada.alto; ++i) {
        for (int j = 0; j < entrada.ancho; ++j) {
            double suma = 0.0;
            for (int di = 0; di < k.tamanio; ++di) {
                for (int dj = 0; dj < k.tamanio; ++dj) {
                    int fi = i + di - r;
                    int fj = j + dj - r;
                    if (fi >= 0 && fi < entrada.alto && fj >= 0 && fj < entrada.ancho) {
                        suma += k.valor(di, dj) * entrada.pixel(fi, fj);
                    }
                }
            }
            salida.pixel(i, j) = suma;
        }
    }

    auto fin = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(fin - inicio).count();
    std::cout << "  [Paralelo " << num_hilos << "h] " << k.nombre << ": " << ms << " ms" << std::endl;

    return salida;
}

// ============================================================
// PARTE C: Magnitud del gradiente Sobel
//
// Combina Sobel X y Sobel Y para obtener la magnitud del
// gradiente en cada píxel:
//   G[i][j] = sqrt(Gx[i][j]² + Gy[i][j]²)
//
// Esto produce una imagen de detección de bordes completa.
// ============================================================
Imagen sobel_magnitud_secuencial(const Imagen& entrada) {
    Imagen gx_img = convolucion_secuencial(entrada, kernel_sobel_x());
    Imagen gy_img = convolucion_secuencial(entrada, kernel_sobel_y());

    Imagen salida(entrada.ancho, entrada.alto);

    for (int i = 0; i < entrada.alto; ++i) {
        for (int j = 0; j < entrada.ancho; ++j) {
            double gx = gx_img.pixel(i, j);
            double gy = gy_img.pixel(i, j);
            salida.pixel(i, j) = std::sqrt(gx * gx + gy * gy);
        }
    }
    return salida;
}

Imagen sobel_magnitud_paralela(const Imagen& entrada, int num_hilos) {
    Imagen gx_img = convolucion_paralela(entrada, kernel_sobel_x(), num_hilos);
    Imagen gy_img = convolucion_paralela(entrada, kernel_sobel_y(), num_hilos);

    Imagen salida(entrada.ancho, entrada.alto);

    // TODO: Paralelice este bucle para calcular la magnitud
    //       del gradiente en paralelo.
    #pragma omp parallel for num_threads(num_hilos) schedule(static)
    for (int i = 0; i < entrada.alto; ++i) {
        for (int j = 0; j < entrada.ancho; ++j) {
            double gx = gx_img.pixel(i, j);
            double gy = gy_img.pixel(i, j);
            salida.pixel(i, j) = std::sqrt(gx * gx + gy * gy);
        }
    }
    return salida;
}

// ============================================================
// PARTE D: Pipeline de filtros
//
// Aplica una secuencia de filtros en cadena:
//   entrada → filtro1 → filtro2 → ... → salida
// ============================================================
Imagen pipeline_secuencial(const Imagen& entrada, const std::vector<Kernel>& filtros) {
    auto inicio = std::chrono::high_resolution_clock::now();

    Imagen actual = entrada;
    for (const auto& k : filtros) {
        actual = convolucion_secuencial(actual, k);
    }

    auto fin = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(fin - inicio).count();
    std::cout << "  [Pipeline secuencial] " << filtros.size() << " filtros: " << ms << " ms" << std::endl;

    return actual;
}

Imagen pipeline_paralelo(const Imagen& entrada, const std::vector<Kernel>& filtros, int num_hilos) {
    auto inicio = std::chrono::high_resolution_clock::now();

    // TODO: Aplique los filtros en secuencia, pero cada convolución
    //       individual se ejecuta en paralelo con convolucion_paralela().
    //       Note que los filtros NO se pueden aplicar simultáneamente
    //       porque cada uno depende del resultado del anterior.
    //
    //       Pregunta para el informe: ¿es posible paralelizar los
    //       filtros entre sí (pipeline real)? ¿Bajo qué condiciones?
    Imagen actual = entrada;
    for (const auto& k : filtros) {
        actual = convolucion_paralela(actual, k, num_hilos);
    }

    auto fin = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(fin - inicio).count();
    std::cout << "  [Pipeline paralelo " << num_hilos << "h] "
              << filtros.size() << " filtros: " << ms << " ms" << std::endl;

    return actual;
}

// ============================================================
// Verificación: diferencia máxima entre dos imágenes
// ============================================================
bool verificar(const Imagen& ref, const Imagen& prueba, const std::string& nombre, double tol = 1e-6) {
    double max_diff = 0.0;
    for (int i = 0; i < ref.alto; ++i)
        for (int j = 0; j < ref.ancho; ++j)
            max_diff = std::max(max_diff, std::abs(ref.pixel(i, j) - prueba.pixel(i, j)));

    bool ok = max_diff < tol;
    std::cout << "  [" << (ok ? "OK" : "FALLO") << "] " << nombre
              << " — diff máx: " << max_diff << std::endl;
    return ok;
}

// ============================================================
// Función principal
// ============================================================
int main(int argc, char* argv[]) {
    std::cout << "====================================================" << std::endl;
    std::cout << " Taller: Filtros de convolución paralelos con OpenMP" << std::endl;
    std::cout << "====================================================" << std::endl;
    std::cout << "Procesadores disponibles: " << omp_get_max_threads() << std::endl;
    std::cout << std::endl;

    // --- Cargar o generar imagen ---
    Imagen img;
    if (argc > 1) {
        std::cout << "--- Cargando imagen ---" << std::endl;
        img = leer_pgm(argv[1]);
    } else {
        int tam = 1024;
        std::cout << "--- Generando imagen de prueba (" << tam << "x" << tam << ") ---" << std::endl;
        img = generar_imagen_prueba(tam, tam);
        escribir_pgm(img, "original.pgm");
        std::cout << "  Guardada como: original.pgm" << std::endl;
    }
    std::cout << std::endl;

    // --- Lista de kernels para probar ---
    std::vector<Kernel> kernels = {
        kernel_blur_3x3(),
        kernel_gaussiano_5x5(),
        kernel_sobel_x(),
        kernel_sharpen(),
        kernel_emboss()
    };

    // ── PARTE A: Convoluciones secuenciales ──
    std::cout << "=== Parte A: Convoluciones secuenciales ===" << std::endl;
    std::vector<Imagen> resultados_seq;
    std::vector<double> tiempos_seq;

    for (const auto& k : kernels) {
        auto t0 = std::chrono::high_resolution_clock::now();
        Imagen res = convolucion_secuencial(img, k);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        resultados_seq.push_back(res);
        tiempos_seq.push_back(ms);
    }

    // Guardar resultados secuenciales
    escribir_pgm(resultados_seq[0], "blur_3x3.pgm");
    escribir_pgm(resultados_seq[1], "gaussiano_5x5.pgm");
    escribir_pgm(resultados_seq[3], "sharpen.pgm");
    escribir_pgm(resultados_seq[4], "emboss.pgm");

    // Sobel magnitud
    Imagen sobel_seq = sobel_magnitud_secuencial(img);
    escribir_pgm(sobel_seq, "sobel_bordes.pgm");
    std::cout << std::endl;

    // ── PARTE B: Convoluciones paralelas con distintos hilos ──
    std::cout << "=== Parte B: Tabla de speedup ===" << std::endl;
    std::vector<int> hilos_prueba = {1, 2, 4, 8};

    std::cout << std::left
              << std::setw(18) << "Kernel"
              << std::setw(12) << "Seq(ms)";
    for (int nh : hilos_prueba)
        std::cout << std::setw(10) << (std::to_string(nh) + "h(ms)")
                  << std::setw(8)  << "SU";
    std::cout << std::endl;
    std::cout << std::string(18 + 12 + hilos_prueba.size() * 18, '-') << std::endl;

    for (size_t ki = 0; ki < kernels.size(); ++ki) {
        const auto& k = kernels[ki];
        std::cout << std::setw(18) << k.nombre
                  << std::setw(12) << std::fixed << std::setprecision(2) << tiempos_seq[ki];

        for (int nh : hilos_prueba) {
            auto t0 = std::chrono::high_resolution_clock::now();
            Imagen res = convolucion_paralela(img, k, nh);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double su = tiempos_seq[ki] / ms;
            std::cout << std::setw(10) << ms << std::setw(8) << su;

            // Verificar corrección
            verificar(resultados_seq[ki], res, k.nombre + " " + std::to_string(nh) + "h");
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    // ── PARTE C: Sobel magnitud paralela ──
    std::cout << "=== Parte C: Sobel magnitud ===" << std::endl;
    for (int nh : hilos_prueba) {
        Imagen sobel_par = sobel_magnitud_paralela(img, nh);
        verificar(sobel_seq, sobel_par, "Sobel magnitud " + std::to_string(nh) + "h");
    }
    std::cout << std::endl;

    // ── PARTE D: Pipeline de filtros ──
    std::cout << "=== Parte D: Pipeline (blur → sharpen → emboss) ===" << std::endl;
    std::vector<Kernel> pipeline_filtros = {
        kernel_gaussiano_5x5(),
        kernel_sharpen(),
        kernel_emboss()
    };

    Imagen pipe_seq = pipeline_secuencial(img, pipeline_filtros);
    escribir_pgm(pipe_seq, "pipeline_resultado.pgm");

    for (int nh : hilos_prueba) {
        Imagen pipe_par = pipeline_paralelo(img, pipeline_filtros, nh);
        verificar(pipe_seq, pipe_par, "Pipeline " + std::to_string(nh) + "h");
    }
    std::cout << std::endl;

    // ── Escalabilidad con distintos tamaños ──
    std::cout << "=== Escalabilidad (Gaussiano 5x5, 4 hilos) ===" << std::endl;
    std::cout << std::setw(12) << "Tamaño"
              << std::setw(12) << "Seq(ms)"
              << std::setw(12) << "Par(ms)"
              << std::setw(10) << "Speedup" << std::endl;
    std::cout << std::string(46, '-') << std::endl;

    Kernel kg = kernel_gaussiano_5x5();
    for (int sz : {256, 512, 1024, 2048}) {
        Imagen test = generar_imagen_prueba(sz, sz);

        auto t0 = std::chrono::high_resolution_clock::now();
        convolucion_secuencial(test, kg);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ts = std::chrono::duration<double, std::milli>(t1 - t0).count();

        t0 = std::chrono::high_resolution_clock::now();
        convolucion_paralela(test, kg, 4);
        t1 = std::chrono::high_resolution_clock::now();
        double tp = std::chrono::duration<double, std::milli>(t1 - t0).count();

        std::cout << std::setw(6) << sz << "x" << std::setw(4) << sz
                  << std::setw(12) << ts
                  << std::setw(12) << tp
                  << std::setw(10) << ts / tp << std::endl;
    }

    std::cout << "\n--- Fin del taller ---" << std::endl;
    return 0;
}
