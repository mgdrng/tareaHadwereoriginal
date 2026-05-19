#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define CANT_NODOS 30
#define CANT_OBJETIVOS 5
#define MAX_RUTA 31
#define INFINITO 9999
#define MAX_HEBRAS 40


struct EstadoRuta 
{
    int nodoActual;
    int ruta[MAX_RUTA];
    int visitados[CANT_OBJETIVOS];
    int largoRuta;
};

int grafo[CANT_NODOS + 1][CANT_NODOS + 1];
int objetivos[CANT_OBJETIVOS];
int nodoInicial;
int mejorRuta[MAX_RUTA];
int mejorLargo;
sem_t semaforoHebras;
pthread_mutex_t mutexMejor;

int contadorHebrasActivas = 0;
pthread_mutex_t mutexContador;
pthread_cond_t condTodasTerminaron;
pthread_mutex_t mutexPrinteo;

struct EstadoRuta poolEstados[MAX_HEBRAS + 1];
int poolUsado[MAX_HEBRAS + 1];
pthread_mutex_t mutexPool;

int obtenerNumero(char linea[]);
void inicializarGrafo();
void leerGrafo();
void imprimirGrafo();
void inicializarBusqueda();
int buscarObjetivo(int nodo);
void imprimirRuta(int ruta[], int largo);
int todosObjetivosVisitadosEstado(struct EstadoRuta *estado);
void actualizarMejorRutaEstado(struct EstadoRuta *estado);
struct EstadoRuta* pedirEstado();
void devolverEstado(struct EstadoRuta *estado);
void recorrerGrafo(struct EstadoRuta *estado);
void inicializarEstadoRuta(struct EstadoRuta *estado, int nodoActual);
void copiarEstadoRuta(struct EstadoRuta *origen, struct EstadoRuta *destino);
int nodoEstaEnRutaEstado(struct EstadoRuta *estado, int nodo);
void* hebra_recorrer(void* arg);

int obtenerNumero(char linea[])
{
    int i = 0;

    while (linea[i] != '\0') {
        if (linea[i] >= '0' && linea[i] <= '9') {
            return atoi(&linea[i]);
        }
        i++;
    }

    return 0;
}
void inicializarGrafo()
{
    int i;
    int j;

    for (i = 0; i <= CANT_NODOS; i++) {
        for (j = 0; j <= CANT_NODOS; j++) {
            grafo[i][j] = -1;
        }
    }
}

void leerGrafo()
{
    FILE *archivo;
    char linea[300];
    char *token;

    int origen;
    int destino;
    int contadorObjetivos;
    int posicionVecino;

    archivo = fopen("grafo.csv", "r");

    if (archivo == NULL) {
        printf("Error: no se pudo abrir grafo.csv\n");
        return;
    }

    /* Primera linea: nodo inicial */
    if (fgets(linea, sizeof(linea), archivo) != NULL) {
        nodoInicial = obtenerNumero(linea);
    }

    /* Segunda linea: nodos obligatorios */
    if (fgets(linea, sizeof(linea), archivo) != NULL) {
        contadorObjetivos = 0;
        token = strtok(linea, ";\n\r");

        while (token != NULL && contadorObjetivos < CANT_OBJETIVOS) {
            objetivos[contadorObjetivos] = atoi(token);
            contadorObjetivos++;
            token = strtok(NULL, ";\n\r");
        }
    }

    /* Desde la tercera linea: enlaces del grafo */
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        token = strtok(linea, ";\n\r");

        if (token != NULL) {
            origen = atoi(token);
            posicionVecino = 0;

            token = strtok(NULL, ";\n\r");

            while (token != NULL) {
                destino = atoi(token);

                if (origen >= 1 && origen <= CANT_NODOS &&
                    destino >= 1 && destino <= CANT_NODOS &&
                    posicionVecino <= CANT_NODOS) {

                    grafo[origen][posicionVecino] = destino;
                    posicionVecino++;
                }

                token = strtok(NULL, ";\n\r");
            }
        }
    }

    fclose(archivo);
}

void imprimirGrafo()
{
    int i;
    int j;

    printf("Nodo inicial: %d\n", nodoInicial);

    printf("Nodos obligatorios: ");
    for (i = 0; i < CANT_OBJETIVOS; i++) {
        printf("%d ", objetivos[i]);
    }

    printf("\n\n");

    printf("Grafo cargado como matriz de vecinos:\n");

    for (i = 1; i <= CANT_NODOS; i++) {
        printf("Vecinos del nodo %d: ", i);

        j = 0;
        while (j <= CANT_NODOS && grafo[i][j] != -1) {
            printf("%d ", grafo[i][j]);
            j++;
        }

        printf("\n");
    }
}

void inicializarBusqueda()
{
    int i;

    mejorLargo = INFINITO;

    for (i = 0; i < MAX_RUTA; i++) {
        mejorRuta[i] = -1;
    }
}

int buscarObjetivo(int nodo)
{
    int i;

    /*
       Recorremos el arreglo de objetivos.
       
       Este arreglo contiene los 5 nodos obligatorios
       que fueron leídos desde la segunda línea del archivo grafo.csv.
       
       Por ejemplo:
       objetivos[0] = 2
       objetivos[1] = 8
       objetivos[2] = 17
       objetivos[3] = 20
       objetivos[4] = 28
    */
    for (i = 0; i < CANT_OBJETIVOS; i++) {

        /*
           Si el nodo recibido coincide con alguno de los objetivos,
           retornamos la posición donde fue encontrado.
           
           Por ejemplo:
           si nodo = 17 y objetivos[2] = 17,
           entonces retorna 2.
        */
        if (objetivos[i] == nodo) {
            return i;
        }
    }

    /*
       Si termina el ciclo y no se encontró el nodo,
       significa que el nodo no es uno de los 5 obligatorios.
       
       Retornamos -1 para indicar "no es objetivo".
    */
    return -1;
}


void imprimirRuta(int ruta[], int largo)
{
    int i;

    /*
       Esta funcion imprime una ruta almacenada en un arreglo.

       El arreglo ruta contiene los nodos recorridos.
       La variable largo indica cuantas posiciones validas tiene la ruta.

       Ejemplo:
       ruta[0] = 10
       ruta[1] = 4
       ruta[2] = 1
       largo = 3

       Se imprime:
       10 -> 4 -> 1
    */

    for (i = 0; i < largo; i++) {

        /*
           Imprimimos el nodo que esta en la posicion i de la ruta.
        */
        printf("%d", ruta[i]);

        /*
           Si no estamos en el ultimo nodo, imprimimos una flecha.
           Esto evita que la ruta termine con una flecha extra.
        */
        if (i < largo - 1) {
            printf(" -> ");
        }
    }

    printf("\n");
}


int todosObjetivosVisitadosEstado(struct EstadoRuta *estado)
{
    int i;
    for (i = 0; i < CANT_OBJETIVOS; i++) {
        if (estado->visitados[i] == 0) {
            return 0;
        }
    }
    return 1;
}

void actualizarMejorRutaEstado(struct EstadoRuta *estado)
{
    int i;
    int largoLocal;
    int rutaLocal[MAX_RUTA];
    int hayNuevaMejor;

    hayNuevaMejor = 0;

    pthread_mutex_lock(&mutexMejor);

    if (estado->largoRuta < mejorLargo) {
        mejorLargo = estado->largoRuta;

        for (i = 0; i < estado->largoRuta; i++) {
            mejorRuta[i] = estado->ruta[i];
        }
        for (i = estado->largoRuta; i < MAX_RUTA; i++) {
            mejorRuta[i] = -1;
        }

        largoLocal = mejorLargo;
        for (i = 0; i < largoLocal; i++) {
            rutaLocal[i] = mejorRuta[i];
        }
        hayNuevaMejor = 1;
    }

    pthread_mutex_unlock(&mutexMejor);

    if (hayNuevaMejor == 1) {
        pthread_mutex_lock(&mutexPrinteo);
        printf("\nNueva mejor ruta encontrada:\n");
        imprimirRuta(rutaLocal, largoLocal);
        printf("Cantidad de nodos visitados: %d\n", largoLocal);
        printf("Cantidad de saltos: %d\n", largoLocal - 1);
        pthread_mutex_unlock(&mutexPrinteo);
    }
}

/* Pool: pedir y devolver estados */
struct EstadoRuta* pedirEstado()
{
    int i;
    pthread_mutex_lock(&mutexPool);
    for (i = 0; i <= MAX_HEBRAS; i++) {
        if (poolUsado[i] == 0) {
            poolUsado[i] = 1;
            pthread_mutex_unlock(&mutexPool);
            return &poolEstados[i];
        }
    }
    pthread_mutex_unlock(&mutexPool);
    return NULL;
}

void devolverEstado(struct EstadoRuta *estado)
{
    int i;
    pthread_mutex_lock(&mutexPool);
    for (i = 0; i <= MAX_HEBRAS; i++) {
        if (&poolEstados[i] == estado) {
            poolUsado[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&mutexPool);
}


void recorrerGrafo(struct EstadoRuta *estado)
{
    int i;
    int vecino;
    int posicionObjetivo;
    int estabaVisitado;
    int nodoActual;
    int largoActual;
    pthread_t identificadorHebra;
    struct EstadoRuta *copiaEstado;
 
    nodoActual = estado->nodoActual;
 
    if (estado->largoRuta >= MAX_RUTA) {
        return;
    }
 
    /* Agregar nodo actual a la ruta local del estado */
    estado->ruta[estado->largoRuta] = nodoActual;
    estado->largoRuta++;
 
    /* Revisar si es objetivo */
    posicionObjetivo = buscarObjetivo(nodoActual);
    estabaVisitado = 0;
 
    if (posicionObjetivo != -1) {
        estabaVisitado = estado->visitados[posicionObjetivo];
        estado->visitados[posicionObjetivo] = 1;
    }
 
    if (todosObjetivosVisitadosEstado(estado) == 1) {
        actualizarMejorRutaEstado(estado);
    } else {
        i = 0;
        while (i <= CANT_NODOS && grafo[nodoActual][i] != -1) {
            vecino = grafo[nodoActual][i];
 
            if (nodoEstaEnRutaEstado(estado, vecino) == 0) {
 
                pthread_mutex_lock(&mutexMejor);
                largoActual = mejorLargo;
                pthread_mutex_unlock(&mutexMejor);
 
                if (estado->largoRuta < largoActual) {
 
                    if (sem_trywait(&semaforoHebras) == 0) {
                        /* Hay cupo: pedir del pool y crear hebra nueva */
                        copiaEstado = pedirEstado();
                        copiarEstadoRuta(estado, copiaEstado);
                        copiaEstado->nodoActual = vecino;
 
                        pthread_mutex_lock(&mutexContador);
                        contadorHebrasActivas++;
                        pthread_mutex_unlock(&mutexContador);
 
                        pthread_create(&identificadorHebra, NULL, hebra_recorrer, copiaEstado);
                        pthread_detach(identificadorHebra);
 
                    } else {
                        /* Sin cupo: usar malloc, no el pool */
                        copiaEstado = (struct EstadoRuta*) malloc(sizeof(struct EstadoRuta));
                        copiarEstadoRuta(estado, copiaEstado);
                        copiaEstado->nodoActual = vecino;
 
                        recorrerGrafo(copiaEstado);
                        free(copiaEstado);
                    }
                }
            }
            i++;
        }
    }
 
    /* Backtracking */
    if (posicionObjetivo != -1) {
        estado->visitados[posicionObjetivo] = estabaVisitado;
    }
 
    estado->largoRuta--;
    estado->ruta[estado->largoRuta] = -1;
}


void inicializarEstadoRuta(struct EstadoRuta *estado, int nodoActual)
{
    int i;

    /*
       Inicializa una variable struct EstadoRuta.

       Cada hebra tendrá su propio estado:
       - nodo actual
       - ruta recorrida
       - objetivos visitados
       - largo de la ruta
    */

    estado->nodoActual = nodoActual;
    estado->largoRuta = 0;

    for (i = 0; i < MAX_RUTA; i++) {
        estado->ruta[i] = -1;
    }

    for (i = 0; i < CANT_OBJETIVOS; i++) {
        estado->visitados[i] = 0;
    }
}

void copiarEstadoRuta(struct EstadoRuta *origen, struct EstadoRuta *destino)
{
    int i;

    /*
       Copia un struct EstadoRuta en otro.

       Esto será útil cuando una hebra cree otra ruta:
       no se debe compartir la misma ruta, sino crear una copia.
    */

    destino->nodoActual = origen->nodoActual;
    destino->largoRuta = origen->largoRuta;

    for (i = 0; i < MAX_RUTA; i++) {
        destino->ruta[i] = origen->ruta[i];
    }

    for (i = 0; i < CANT_OBJETIVOS; i++) {
        destino->visitados[i] = origen->visitados[i];
    }
}
int nodoEstaEnRutaEstado(struct EstadoRuta *estado, int nodo)
{
    int i;

    /*
       Esta funcion revisa si un nodo ya está dentro
       de la ruta almacenada en un struct EstadoRuta.

       Es similar a nodoEstaEnRuta(), pero en vez de revisar
       la variable global rutaActual, revisa estado->ruta.

       Esto será necesario para las hebras, porque cada hebra
       tendrá su propio struct EstadoRuta.
    */

    for (i = 0; i < estado->largoRuta; i++) {

        /*
           Si el nodo aparece dentro de la ruta del estado,
           retornamos 1.
        */
        if (estado->ruta[i] == nodo) {
            return 1;
        }
    }

    /*
       Si no se encuentra el nodo, retornamos 0.
    */
    return 0;
}

void* hebra_recorrer(void* arg) {
    struct EstadoRuta *estado = (struct EstadoRuta*) arg;

    recorrerGrafo(estado);
    devolverEstado(estado);
    sem_post(&semaforoHebras);  

    pthread_mutex_lock(&mutexContador);
    contadorHebrasActivas--;
    if (contadorHebrasActivas == 0) {
        pthread_cond_signal(&condTodasTerminaron);
    }
    pthread_mutex_unlock(&mutexContador);

    return NULL;
}

int main()
{
    struct EstadoRuta *estadoInicial;
    pthread_t hebraInicial;

    inicializarGrafo();
    leerGrafo();
    imprimirGrafo(); 
    inicializarBusqueda();

    sem_init(&semaforoHebras, 0, MAX_HEBRAS);
    pthread_mutex_init(&mutexMejor, NULL);
    pthread_mutex_init(&mutexContador, NULL);
    pthread_mutex_init(&mutexPrinteo, NULL);
    pthread_mutex_init(&mutexPool, NULL);        /* agregar aqui */
    pthread_cond_init(&condTodasTerminaron, NULL);

    memset(poolUsado, 0, sizeof(poolUsado));     /* agregar aqui */

    estadoInicial = pedirEstado();               /* antes: malloc */
    inicializarEstadoRuta(estadoInicial, nodoInicial);

    pthread_mutex_lock(&mutexContador);
    contadorHebrasActivas = 1;
    pthread_mutex_unlock(&mutexContador);

    sem_wait(&semaforoHebras);
    pthread_create(&hebraInicial, NULL, hebra_recorrer, estadoInicial);
    pthread_detach(hebraInicial);

    pthread_mutex_lock(&mutexContador);
    while (contadorHebrasActivas > 0) {
        pthread_cond_wait(&condTodasTerminaron, &mutexContador);
    }
    pthread_mutex_unlock(&mutexContador);

    printf("\nMejor ruta final encontrada:\n");

    if (mejorLargo == INFINITO) {
        printf("No se encontro una ruta que pase por todos los objetivos.\n");
    } else {
        imprimirRuta(mejorRuta, mejorLargo);
        printf("Cantidad de nodos visitados: %d\n", mejorLargo);
        printf("Cantidad de saltos: %d\n", mejorLargo - 1);
    }

    return 0;
}