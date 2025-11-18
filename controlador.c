/*****************************************************
 * PONTIFICIA UNIVERSIDAD JAVERIANA
 *
 * Materia: Sistemas Operativos
 * Docente: J. Corredor, PhD
 * Autor: Juan David Garzon Ballen
 * Programa: controlador.c
 * Fecha: 17 de noviembre de 2025
 * Tema: Controlador del Sistema de Reservas
 * -----------------------------------------------
 * Descripción:
 * Este programa implementa el servidor del sistema de
 * reservas del Parque Berlín. Actúa como un proceso
 * multihilo que gestiona el estado del parque, procesa
 * las solicitudes de reserva de múltiples agentes y
 * controla el aforo. Utiliza hilos POSIX para manejar
 * tareas concurrentes (reloj y recepción de mensajes)
 * y mutex para garantizar la exclusión mutua al acceder
 * a datos compartidos. La comunicación con los agentes
 * se realiza a través de named pipes (FIFOs).
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>

/* ============================================================================
 * CONSTANTES Y DEFINICIONES
 * ============================================================================ */
#define MAX_BUFFER 1024
#define MAX_AGENTES 50
#define MAX_NOMBRE 128  // Para nombres de familias y agentes
#define MAX_PIPE_NAME 256  // Buffer más grande para nombres de pipes
#define HORAS_MIN 7
#define HORAS_MAX 19
#define MAX_HORAS (HORAS_MAX - HORAS_MIN + 1)  // 13 horas
#define DURACION_RESERVA 2  // Cada reserva es por 2 horas

/* Tipos de mensaje entre agente y controlador */
typedef enum {
    MSG_REGISTRO,           // Registro inicial del agente
    MSG_SOLICITUD_RESERVA,  // Solicitud de reserva
    MSG_FIN_AGENTE          // Agente termina
} TipoMensaje;

/* Tipos de respuesta del controlador */
typedef enum {
    RESP_HORA_ACTUAL,       // Respuesta con hora actual
    RESP_RESERVA_OK,        // Reserva aprobada
    RESP_RESERVA_REPROG,    // Reserva reprogramada
    RESP_RESERVA_NEGADA,    // Reserva negada
    RESP_FIN_DIA            // Fin del día
} TipoRespuesta;

/* Estructura para mensajes del agente al controlador */
typedef struct {
    TipoMensaje tipo;
    char nombreAgente[MAX_NOMBRE];
    char pipeRespuesta[MAX_NOMBRE];  // Pipe para respuestas específicas
    char nombreFamilia[MAX_NOMBRE];
    int horaSolicitada;
    int numPersonas;
} MensajeAgente;

/* Estructura para respuestas del controlador al agente */
typedef struct {
    TipoRespuesta tipo;
    int horaAsignada;
    int horaActual;
    char mensaje[MAX_BUFFER];
} RespuestaControlador;

/* Estructura para registrar una reserva */
typedef struct {
    char nombreFamilia[MAX_NOMBRE];
    char nombreAgente[MAX_NOMBRE];
    int horaInicio;
    int horaFin;
    int numPersonas;
    int activa;  // 1 si está activa, 0 si ya salió
} Reserva;

/* Estructura para información de un agente registrado */
typedef struct {
    char nombre[MAX_NOMBRE];
    char pipeRespuesta[MAX_NOMBRE];
    int activo;
} AgenteInfo;

/* ============================================================================
 * VARIABLES GLOBALES
 * ============================================================================ */
// Parámetros de configuración
int horaInicial;
int horaFinal;
int segundosPorHora;
int aforoMaximo;
char pipeRecibe[MAX_NOMBRE];

// Estado del sistema
int horaActual;
int ocupacionPorHora[MAX_HORAS];  // Personas por hora
Reserva reservas[1000];
int numReservas = 0;
AgenteInfo agentesRegistrados[MAX_AGENTES];
int numAgentes = 0;

// Estadísticas
int solicitudesNegadas = 0;
int solicitudesAceptadas = 0;
int solicitudesReprogramadas = 0;

// Mutex y variables de sincronización
pthread_mutex_t mutexReservas = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexAgentes = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexEstadisticas = PTHREAD_MUTEX_INITIALIZER;

// Control de señales
volatile sig_atomic_t alarmaRecibida = 0;
volatile sig_atomic_t finalizarServidor = 0;

/* ============================================================================
 * PROTOTIPOS DE FUNCIONES
 * ============================================================================ */
void procesarArgumentos(int argc, char *argv[]);
void inicializarServidor();
void manejadorAlarma(int sig);
void manejadorSigInt(int sig);
void *hiloReloj(void *arg);
void *hiloRecibirPeticiones(void *arg);
void procesarMensaje(MensajeAgente *msg, int fdPipeRecibe);
void registrarAgente(MensajeAgente *msg);
void procesarSolicitudReserva(MensajeAgente *msg);
void enviarRespuesta(char *pipeAgente, RespuestaControlador *resp);
int verificarDisponibilidad(int hora, int numPersonas);
int buscarHoraAlternativa(int numPersonas, int *horaEncontrada);
void avanzarHora();
void imprimirEstadoHora();
void generarReporte();
void limpiarRecursos();
int validarHora(int hora);
int indiceHora(int hora);

/* ============================================================================
 * FUNCIÓN PRINCIPAL
 * ============================================================================ */
int main(int argc, char *argv[]) {
    pthread_t tidReloj, tidPeticiones;
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  SISTEMA DE RESERVAS - PARQUE BERLÍN                       ║\n");
    printf("║  Controlador de Reservas (Servidor)                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Procesar argumentos de línea de comandos
    procesarArgumentos(argc, argv);
    
    // Inicializar el servidor
    inicializarServidor();
    
    // Configurar manejadores de señales
    signal(SIGALRM, manejadorAlarma);
    signal(SIGINT, manejadorSigInt);
    
    // Crear hilo para el reloj de simulación
    if (pthread_create(&tidReloj, NULL, hiloReloj, NULL) != 0) {
        perror("Error al crear hilo del reloj");
        limpiarRecursos();
        exit(EXIT_FAILURE);
    }
    
    // Crear hilo para recibir peticiones de agentes
    if (pthread_create(&tidPeticiones, NULL, hiloRecibirPeticiones, NULL) != 0) {
        perror("Error al crear hilo de peticiones");
        limpiarRecursos();
        exit(EXIT_FAILURE);
    }
    
    printf("✓ Servidor iniciado correctamente\n");
    printf("✓ Hora inicial: %d:00\n", horaActual);
    printf("✓ Hora final: %d:00\n", horaFinal);
    printf("✓ Aforo máximo: %d personas\n", aforoMaximo);
    printf("✓ Segundos por hora: %d\n", segundosPorHora);
    printf("✓ Esperando conexiones de agentes...\n\n");
    
    // Esperar a que el hilo del reloj termine (termina cuando horaActual > horaFinal)
    pthread_join(tidReloj, NULL);
    
    // Dar tiempo para que los agentes reciban últimas respuestas
    printf("⏳ Esperando finalización de comunicaciones...\n");
    sleep(2);
    
    // Marcar finalización para el hilo de peticiones
    finalizarServidor = 1;
    
    // Dar tiempo para que el hilo de peticiones detecte la señal de finalización
    sleep(1);
    
    // Cancelar el hilo de peticiones si aún está esperando
    pthread_cancel(tidPeticiones);
    pthread_join(tidPeticiones, NULL);
    
    // Generar reporte final
    generarReporte();
    
    // Limpiar recursos
    limpiarRecursos();
    
    printf("\n✓ Servidor finalizado correctamente\n\n");
    
    return 0;
}

/* ============================================================================
 * PROCESAMIENTO DE ARGUMENTOS
 * ============================================================================ */
void procesarArgumentos(int argc, char *argv[]) {
    int opt;
    int flagI = 0, flagF = 0, flagS = 0, flagT = 0, flagP = 0;
    
    while ((opt = getopt(argc, argv, "i:f:s:t:p:")) != -1) {
        switch (opt) {
            case 'i':
                horaInicial = atoi(optarg);
                flagI = 1;
                break;
            case 'f':
                horaFinal = atoi(optarg);
                flagF = 1;
                break;
            case 's':
                segundosPorHora = atoi(optarg);
                flagS = 1;
                break;
            case 't':
                aforoMaximo = atoi(optarg);
                flagT = 1;
                break;
            case 'p':
                strncpy(pipeRecibe, optarg, MAX_NOMBRE - 1);
                pipeRecibe[MAX_NOMBRE - 1] = '\0';
                flagP = 1;
                break;
            default:
                fprintf(stderr, "Uso: %s -i <horaIni> -f <horaFin> -s <segHoras> -t <total> -p <pipeRecibe>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    // Validar que todos los parámetros fueron proporcionados
    if (!flagI || !flagF || !flagS || !flagT || !flagP) {
        fprintf(stderr, "Error: Faltan parámetros obligatorios\n");
        fprintf(stderr, "Uso: %s -i <horaIni> -f <horaFin> -s <segHoras> -t <total> -p <pipeRecibe>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Validar rangos
    if (!validarHora(horaInicial) || !validarHora(horaFinal)) {
        fprintf(stderr, "Error: Las horas deben estar entre %d y %d\n", HORAS_MIN, HORAS_MAX);
        exit(EXIT_FAILURE);
    }
    
    if (horaInicial >= horaFinal) {
        fprintf(stderr, "Error: La hora inicial debe ser menor que la hora final\n");
        exit(EXIT_FAILURE);
    }
    
    if (segundosPorHora <= 0) {
        fprintf(stderr, "Error: Los segundos por hora deben ser mayores a 0\n");
        exit(EXIT_FAILURE);
    }
    
    if (aforoMaximo <= 0) {
        fprintf(stderr, "Error: El aforo máximo debe ser mayor a 0\n");
        exit(EXIT_FAILURE);
    }
}

/* ============================================================================
 * INICIALIZACIÓN DEL SERVIDOR
 * ============================================================================ */
void inicializarServidor() {
    // Inicializar hora actual
    horaActual = horaInicial;
    
    // Inicializar ocupación por hora
    memset(ocupacionPorHora, 0, sizeof(ocupacionPorHora));
    
    // Crear el pipe nominal para recibir mensajes
    unlink(pipeRecibe);  // Eliminar si existe
    if (mkfifo(pipeRecibe, 0666) == -1) {
        if (errno != EEXIST) {
            perror("Error al crear pipe de recepción");
            exit(EXIT_FAILURE);
        }
    }
}

/* ============================================================================
 * MANEJADORES DE SEÑALES
 * ============================================================================ */
void manejadorAlarma(int sig) {
    (void)sig;  // Suprimir warning de parámetro no usado
    alarmaRecibida = 1;
}

void manejadorSigInt(int sig) {
    (void)sig;  // Suprimir warning de parámetro no usado
    finalizarServidor = 1;
}

/* ============================================================================
 * HILO DEL RELOJ DE SIMULACIÓN
 * ============================================================================ */
void *hiloReloj(void *arg) {
    (void)arg;  // Suprimir warning de parámetro no usado
    
    while (horaActual <= horaFinal && !finalizarServidor) {
        // Esperar los segundos configurados (simula el paso de una hora)
        sleep(segundosPorHora);
        
        // Avanzar a la siguiente hora
        avanzarHora();
        
        // Imprimir estado actual
        imprimirEstadoHora();
    }
    
    // Marcar que el servidor debe finalizar
    finalizarServidor = 1;
    
    // Notificar que la simulación ha terminado
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║          🏁 SIMULACIÓN FINALIZADA                          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return NULL;
}

/* ============================================================================
 * HILO PARA RECIBIR PETICIONES DE AGENTES
 * ============================================================================ */
void *hiloRecibirPeticiones(void *arg) {
    (void)arg;  // Suprimir warning de parámetro no usado
    
    int fdPipeRecibe;
    MensajeAgente msg;
    ssize_t bytesLeidos;
    
    // Abrir pipe en modo lectura (bloqueante hasta que un agente se conecte)
    fdPipeRecibe = open(pipeRecibe, O_RDONLY | O_NONBLOCK);
    if (fdPipeRecibe == -1) {
        perror("Error al abrir pipe de recepción");
        finalizarServidor = 1;
        return NULL;
    }
    
    // Cambiar a modo bloqueante después de abrir
    int flags = fcntl(fdPipeRecibe, F_GETFL, 0);
    fcntl(fdPipeRecibe, F_SETFL, flags & ~O_NONBLOCK);
    
    while (!finalizarServidor) {
        // Configurar timeout para read usando select
        fd_set readfds;
        struct timeval tv;
        
        FD_ZERO(&readfds);
        FD_SET(fdPipeRecibe, &readfds);
        
        // Timeout de 1 segundo
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int ret = select(fdPipeRecibe + 1, &readfds, NULL, NULL, &tv);
        
        if (ret == -1) {
            if (errno != EINTR && !finalizarServidor) {
                perror("Error en select");
            }
            continue;
        } else if (ret == 0) {
            // Timeout - verificar si debe finalizar
            continue;
        }
        
        // Leer mensaje del agente
        bytesLeidos = read(fdPipeRecibe, &msg, sizeof(MensajeAgente));
        
        if (bytesLeidos > 0) {
            if (bytesLeidos == sizeof(MensajeAgente)) {
                procesarMensaje(&msg, fdPipeRecibe);
            }
        } else if (bytesLeidos == 0) {
            // EOF - reabrir el pipe para aceptar nuevas conexiones
            close(fdPipeRecibe);
            
            if (!finalizarServidor) {
                fdPipeRecibe = open(pipeRecibe, O_RDONLY | O_NONBLOCK);
                if (fdPipeRecibe == -1) {
                    if (!finalizarServidor) {
                        perror("Error al reabrir pipe de recepción");
                    }
                    break;
                }
                // Cambiar a modo bloqueante
                flags = fcntl(fdPipeRecibe, F_GETFL, 0);
                fcntl(fdPipeRecibe, F_SETFL, flags & ~O_NONBLOCK);
            }
        } else {
            if (errno != EINTR && !finalizarServidor) {
                perror("Error al leer del pipe");
            }
        }
    }
    
    close(fdPipeRecibe);
    return NULL;
}

/* ============================================================================
 * PROCESAMIENTO DE MENSAJES
 * ============================================================================ */
void procesarMensaje(MensajeAgente *msg, int fdPipeRecibe) {
    (void)fdPipeRecibe;  // Suprimir warning de parámetro no usado
    
    switch (msg->tipo) {
        case MSG_REGISTRO:
            registrarAgente(msg);
            break;
        case MSG_SOLICITUD_RESERVA:
            procesarSolicitudReserva(msg);
            break;
        case MSG_FIN_AGENTE:
            printf("→ Agente %s ha finalizado\n", msg->nombreAgente);
            break;
        default:
            fprintf(stderr, "Mensaje desconocido recibido\n");
            break;
    }
}

/* ============================================================================
 * REGISTRO DE AGENTES
 * ============================================================================ */
void registrarAgente(MensajeAgente *msg) {
    RespuestaControlador resp;
    
    pthread_mutex_lock(&mutexAgentes);
    
    // Registrar agente
    if (numAgentes < MAX_AGENTES) {
        strncpy(agentesRegistrados[numAgentes].nombre, msg->nombreAgente, MAX_NOMBRE - 1);
        strncpy(agentesRegistrados[numAgentes].pipeRespuesta, msg->pipeRespuesta, MAX_NOMBRE - 1);
        agentesRegistrados[numAgentes].activo = 1;
        numAgentes++;
    }
    
    pthread_mutex_unlock(&mutexAgentes);
    
    printf("→ Agente '%s' registrado\n", msg->nombreAgente);
    
    // Enviar hora actual al agente
    resp.tipo = RESP_HORA_ACTUAL;
    resp.horaActual = horaActual;
    snprintf(resp.mensaje, MAX_BUFFER, "Bienvenido. Hora actual: %d:00", horaActual);
    
    enviarRespuesta(msg->pipeRespuesta, &resp);
}

/* ============================================================================
 * PROCESAMIENTO DE SOLICITUDES DE RESERVA
 * ============================================================================ */
void procesarSolicitudReserva(MensajeAgente *msg) {
    RespuestaControlador resp;
    int horaAlternativa;
    
    printf("\n╔═══════════════════════════════════════════════════════╗\n");
    printf("║ SOLICITUD DE RESERVA                                  ║\n");
    printf("╠═══════════════════════════════════════════════════════╣\n");
    printf("║ Agente: %-45s ║\n", msg->nombreAgente);
    printf("║ Familia: %-44s ║\n", msg->nombreFamilia);
    printf("║ Hora solicitada: %d:00                                 ║\n", msg->horaSolicitada);
    printf("║ Personas: %-3d                                         ║\n", msg->numPersonas);
    printf("╚═══════════════════════════════════════════════════════╝\n");
    
    // Validar que la hora esté en rango
    if (!validarHora(msg->horaSolicitada)) {
        pthread_mutex_lock(&mutexEstadisticas);
        solicitudesNegadas++;
        pthread_mutex_unlock(&mutexEstadisticas);
        
        resp.tipo = RESP_RESERVA_NEGADA;
        resp.horaActual = horaActual;
        snprintf(resp.mensaje, MAX_BUFFER, 
                "Reserva NEGADA - Hora fuera del rango de operación (%d-%d)", 
                HORAS_MIN, HORAS_MAX);
        
        printf("✗ Respuesta: %s\n\n", resp.mensaje);
        enviarRespuesta(msg->pipeRespuesta, &resp);
        return;
    }
    
    // Validar que el número de personas no exceda el aforo
    if (msg->numPersonas > aforoMaximo) {
        pthread_mutex_lock(&mutexEstadisticas);
        solicitudesNegadas++;
        pthread_mutex_unlock(&mutexEstadisticas);
        
        resp.tipo = RESP_RESERVA_NEGADA;
        resp.horaActual = horaActual;
        snprintf(resp.mensaje, MAX_BUFFER, 
                "Reserva NEGADA - Número de personas (%d) excede el aforo máximo (%d). Debe volver otro día.", 
                msg->numPersonas, aforoMaximo);
        
        printf("✗ Respuesta: %s\n\n", resp.mensaje);
        enviarRespuesta(msg->pipeRespuesta, &resp);
        return;
    }
    
    // Validar hora solicitada vs hora actual
    if (msg->horaSolicitada < horaActual) {
        printf("⚠ Solicitud extemporánea (hora solicitada < hora actual)\n");
        
        // Buscar hora alternativa
        if (buscarHoraAlternativa(msg->numPersonas, &horaAlternativa)) {
            pthread_mutex_lock(&mutexEstadisticas);
            solicitudesReprogramadas++;
            pthread_mutex_unlock(&mutexEstadisticas);
            
            // Realizar reserva en hora alternativa
            pthread_mutex_lock(&mutexReservas);
            strncpy(reservas[numReservas].nombreFamilia, msg->nombreFamilia, MAX_NOMBRE - 1);
            strncpy(reservas[numReservas].nombreAgente, msg->nombreAgente, MAX_NOMBRE - 1);
            reservas[numReservas].horaInicio = horaAlternativa;
            reservas[numReservas].horaFin = horaAlternativa + DURACION_RESERVA - 1;
            reservas[numReservas].numPersonas = msg->numPersonas;
            reservas[numReservas].activa = 0;  // Se activará cuando llegue su hora
            
            // Actualizar ocupación
            for (int h = horaAlternativa; h < horaAlternativa + DURACION_RESERVA && validarHora(h); h++) {
                ocupacionPorHora[indiceHora(h)] += msg->numPersonas;
            }
            
            numReservas++;
            pthread_mutex_unlock(&mutexReservas);
            
            resp.tipo = RESP_RESERVA_REPROG;
            resp.horaAsignada = horaAlternativa;
            resp.horaActual = horaActual;
            snprintf(resp.mensaje, MAX_BUFFER, 
                    "Reserva REPROGRAMADA - Hora solicitada ya pasó. Nueva hora: %d:00 - %d:00", 
                    horaAlternativa, horaAlternativa + DURACION_RESERVA);
            
            printf("✓ Respuesta: %s\n\n", resp.mensaje);
        } else {
            pthread_mutex_lock(&mutexEstadisticas);
            solicitudesNegadas++;
            pthread_mutex_unlock(&mutexEstadisticas);
            
            resp.tipo = RESP_RESERVA_NEGADA;
            resp.horaActual = horaActual;
            snprintf(resp.mensaje, MAX_BUFFER, 
                    "Reserva NEGADA - Hora extemporánea y sin disponibilidad posterior. Debe volver otro día.");
            
            printf("✗ Respuesta: %s\n\n", resp.mensaje);
        }
        
        enviarRespuesta(msg->pipeRespuesta, &resp);
        return;
    }
    
    // Verificar si la hora solicitada está fuera del periodo de simulación
    if (msg->horaSolicitada > horaFinal) {
        pthread_mutex_lock(&mutexEstadisticas);
        solicitudesNegadas++;
        pthread_mutex_unlock(&mutexEstadisticas);
        
        resp.tipo = RESP_RESERVA_NEGADA;
        resp.horaActual = horaActual;
        snprintf(resp.mensaje, MAX_BUFFER, 
                "Reserva NEGADA - Hora solicitada fuera del periodo de simulación. Debe volver otro día.");
        
        printf("✗ Respuesta: %s\n\n", resp.mensaje);
        enviarRespuesta(msg->pipeRespuesta, &resp);
        return;
    }
    
    // Verificar disponibilidad en la hora solicitada
    if (verificarDisponibilidad(msg->horaSolicitada, msg->numPersonas)) {
        pthread_mutex_lock(&mutexEstadisticas);
        solicitudesAceptadas++;
        pthread_mutex_unlock(&mutexEstadisticas);
        
        // Realizar reserva
        pthread_mutex_lock(&mutexReservas);
        strncpy(reservas[numReservas].nombreFamilia, msg->nombreFamilia, MAX_NOMBRE - 1);
        strncpy(reservas[numReservas].nombreAgente, msg->nombreAgente, MAX_NOMBRE - 1);
        reservas[numReservas].horaInicio = msg->horaSolicitada;
        reservas[numReservas].horaFin = msg->horaSolicitada + DURACION_RESERVA - 1;
        reservas[numReservas].numPersonas = msg->numPersonas;
        reservas[numReservas].activa = 0;  // Se activará cuando llegue su hora
        
        // Actualizar ocupación
        for (int h = msg->horaSolicitada; h < msg->horaSolicitada + DURACION_RESERVA && validarHora(h); h++) {
            ocupacionPorHora[indiceHora(h)] += msg->numPersonas;
        }
        
        numReservas++;
        pthread_mutex_unlock(&mutexReservas);
        
        resp.tipo = RESP_RESERVA_OK;
        resp.horaAsignada = msg->horaSolicitada;
        resp.horaActual = horaActual;
        snprintf(resp.mensaje, MAX_BUFFER, 
                "Reserva APROBADA - Hora: %d:00 - %d:00 para %d personas", 
                msg->horaSolicitada, msg->horaSolicitada + DURACION_RESERVA, msg->numPersonas);
        
        printf("✓ Respuesta: %s\n\n", resp.mensaje);
    } else {
        printf("⚠ No hay disponibilidad en hora solicitada\n");
        
        // Buscar hora alternativa
        if (buscarHoraAlternativa(msg->numPersonas, &horaAlternativa)) {
            pthread_mutex_lock(&mutexEstadisticas);
            solicitudesReprogramadas++;
            pthread_mutex_unlock(&mutexEstadisticas);
            
            // Realizar reserva en hora alternativa
            pthread_mutex_lock(&mutexReservas);
            strncpy(reservas[numReservas].nombreFamilia, msg->nombreFamilia, MAX_NOMBRE - 1);
            strncpy(reservas[numReservas].nombreAgente, msg->nombreAgente, MAX_NOMBRE - 1);
            reservas[numReservas].horaInicio = horaAlternativa;
            reservas[numReservas].horaFin = horaAlternativa + DURACION_RESERVA - 1;
            reservas[numReservas].numPersonas = msg->numPersonas;
            reservas[numReservas].activa = 0;
            
            // Actualizar ocupación
            for (int h = horaAlternativa; h < horaAlternativa + DURACION_RESERVA && validarHora(h); h++) {
                ocupacionPorHora[indiceHora(h)] += msg->numPersonas;
            }
            
            numReservas++;
            pthread_mutex_unlock(&mutexReservas);
            
            resp.tipo = RESP_RESERVA_REPROG;
            resp.horaAsignada = horaAlternativa;
            resp.horaActual = horaActual;
            snprintf(resp.mensaje, MAX_BUFFER, 
                    "Reserva REPROGRAMADA - Sin disponibilidad en hora solicitada. Nueva hora: %d:00 - %d:00", 
                    horaAlternativa, horaAlternativa + DURACION_RESERVA);
            
            printf("✓ Respuesta: %s\n\n", resp.mensaje);
        } else {
            pthread_mutex_lock(&mutexEstadisticas);
            solicitudesNegadas++;
            pthread_mutex_unlock(&mutexEstadisticas);
            
            resp.tipo = RESP_RESERVA_NEGADA;
            resp.horaActual = horaActual;
            snprintf(resp.mensaje, MAX_BUFFER, 
                    "Reserva NEGADA - Sin disponibilidad en todo el periodo. Debe volver otro día.");
            
            printf("✗ Respuesta: %s\n\n", resp.mensaje);
        }
    }
    
    enviarRespuesta(msg->pipeRespuesta, &resp);
}

/* ============================================================================
 * ENVÍO DE RESPUESTAS A AGENTES
 * ============================================================================ */
void enviarRespuesta(char *pipeAgente, RespuestaControlador *resp) {
    int fdPipeAgente;
    
    // Abrir pipe del agente para escribir
    fdPipeAgente = open(pipeAgente, O_WRONLY);
    if (fdPipeAgente == -1) {
        perror("Error al abrir pipe del agente para responder");
        return;
    }
    
    // Escribir respuesta
    if (write(fdPipeAgente, resp, sizeof(RespuestaControlador)) == -1) {
        perror("Error al escribir respuesta al agente");
    }
    
    close(fdPipeAgente);
}

/* ============================================================================
 * VERIFICACIÓN DE DISPONIBILIDAD
 * ============================================================================ */
int verificarDisponibilidad(int hora, int numPersonas) {
    pthread_mutex_lock(&mutexReservas);
    
    // Verificar que la hora y la hora siguiente tengan cupo
    for (int h = hora; h < hora + DURACION_RESERVA && validarHora(h); h++) {
        if (ocupacionPorHora[indiceHora(h)] + numPersonas > aforoMaximo) {
            pthread_mutex_unlock(&mutexReservas);
            return 0;  // No hay disponibilidad
        }
    }
    
    pthread_mutex_unlock(&mutexReservas);
    return 1;  // Hay disponibilidad
}

/* ============================================================================
 * BÚSQUEDA DE HORA ALTERNATIVA
 * ============================================================================ */
int buscarHoraAlternativa(int numPersonas, int *horaEncontrada) {
    pthread_mutex_lock(&mutexReservas);
    
    // Buscar desde la hora actual hasta el final
    for (int h = horaActual; h <= horaFinal - DURACION_RESERVA + 1; h++) {
        int disponible = 1;
        
        for (int offset = 0; offset < DURACION_RESERVA; offset++) {
            if (!validarHora(h + offset) || 
                ocupacionPorHora[indiceHora(h + offset)] + numPersonas > aforoMaximo) {
                disponible = 0;
                break;
            }
        }
        
        if (disponible) {
            *horaEncontrada = h;
            pthread_mutex_unlock(&mutexReservas);
            return 1;
        }
    }
    
    pthread_mutex_unlock(&mutexReservas);
    return 0;  // No se encontró hora alternativa
}

/* ============================================================================
 * AVANCE DE HORA
 * ============================================================================ */
void avanzarHora() {
    pthread_mutex_lock(&mutexReservas);
    
    horaActual++;
    
    // Activar reservas que comienzan en esta hora
    for (int i = 0; i < numReservas; i++) {
        if (reservas[i].horaInicio == horaActual && !reservas[i].activa) {
            reservas[i].activa = 1;
        }
    }
    
    // Desactivar reservas que terminan en esta hora
    for (int i = 0; i < numReservas; i++) {
        if (reservas[i].horaFin < horaActual && reservas[i].activa) {
            reservas[i].activa = 0;
        }
    }
    
    pthread_mutex_unlock(&mutexReservas);
}

/* ============================================================================
 * IMPRESIÓN DEL ESTADO DE LA HORA
 * ============================================================================ */
void imprimirEstadoHora() {
    pthread_mutex_lock(&mutexReservas);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                   ⏰ HORA: %02d:00                           ║\n", horaActual);
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    // Familias que salen
    printf("\n📤 Familias que SALEN del parque:\n");
    int totalSalen = 0;
    int haySalidas = 0;
    for (int i = 0; i < numReservas; i++) {
        if (reservas[i].horaFin == horaActual - 1 && reservas[i].horaFin >= horaInicial) {
            printf("   • Familia %s (%d personas) - Agente: %s\n", 
                   reservas[i].nombreFamilia, 
                   reservas[i].numPersonas,
                   reservas[i].nombreAgente);
            totalSalen += reservas[i].numPersonas;
            haySalidas = 1;
        }
    }
    if (!haySalidas) {
        printf("   (Ninguna)\n");
    } else {
        printf("   Total: %d personas\n", totalSalen);
    }
    
    // Familias que entran
    printf("\n📥 Familias que ENTRAN al parque:\n");
    int totalEntran = 0;
    int hayEntradas = 0;
    for (int i = 0; i < numReservas; i++) {
        if (reservas[i].horaInicio == horaActual) {
            printf("   • Familia %s (%d personas) - Agente: %s [%d:00-%d:00]\n", 
                   reservas[i].nombreFamilia, 
                   reservas[i].numPersonas,
                   reservas[i].nombreAgente,
                   reservas[i].horaInicio,
                   reservas[i].horaFin + 1);
            totalEntran += reservas[i].numPersonas;
            hayEntradas = 1;
        }
    }
    if (!hayEntradas) {
        printf("   (Ninguna)\n");
    } else {
        printf("   Total: %d personas\n", totalEntran);
    }
    
    // Ocupación actual
    int ocupacionActual = 0;
    if (validarHora(horaActual)) {
        ocupacionActual = ocupacionPorHora[indiceHora(horaActual)];
    }
    
    printf("\n📊 Ocupación actual: %d / %d personas", ocupacionActual, aforoMaximo);
    
    // Barra de progreso visual
    int porcentaje = (ocupacionActual * 100) / aforoMaximo;
    printf(" [");
    int barras = porcentaje / 5;
    for (int i = 0; i < 20; i++) {
        if (i < barras) printf("█");
        else printf("░");
    }
    printf("] %d%%\n", porcentaje);
    
    pthread_mutex_unlock(&mutexReservas);
    printf("\n");
}

/* ============================================================================
 * GENERACIÓN DE REPORTE FINAL
 * ============================================================================ */
void generarReporte() {
    pthread_mutex_lock(&mutexReservas);
    pthread_mutex_lock(&mutexEstadisticas);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                  📊 REPORTE FINAL DEL DÍA                  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Encontrar horas pico
    int maxOcupacion = 0;
    for (int i = 0; i < MAX_HORAS; i++) {
        if (ocupacionPorHora[i] > maxOcupacion) {
            maxOcupacion = ocupacionPorHora[i];
        }
    }
    
    printf("🔝 HORAS PICO (mayor ocupación: %d personas):\n", maxOcupacion);
    for (int i = 0; i < MAX_HORAS; i++) {
        if (ocupacionPorHora[i] == maxOcupacion && maxOcupacion > 0) {
            printf("   • %d:00 - %d personas\n", HORAS_MIN + i, ocupacionPorHora[i]);
        }
    }
    
    // Encontrar horas valle
    int minOcupacion = aforoMaximo + 1;
    for (int i = 0; i < MAX_HORAS; i++) {
        if (ocupacionPorHora[i] < minOcupacion) {
            minOcupacion = ocupacionPorHora[i];
        }
    }
    
    printf("\n🔽 HORAS VALLE (menor ocupación: %d personas):\n", minOcupacion);
    for (int i = 0; i < MAX_HORAS; i++) {
        if (ocupacionPorHora[i] == minOcupacion) {
            printf("   • %d:00 - %d personas\n", HORAS_MIN + i, ocupacionPorHora[i]);
        }
    }
    
    // Estadísticas de solicitudes
    printf("\n📈 ESTADÍSTICAS DE SOLICITUDES:\n");
    printf("   • Solicitudes aceptadas en su hora:  %d\n", solicitudesAceptadas);
    printf("   • Solicitudes reprogramadas:          %d\n", solicitudesReprogramadas);
    printf("   • Solicitudes negadas:                %d\n", solicitudesNegadas);
    printf("   • Total de solicitudes:               %d\n", 
           solicitudesAceptadas + solicitudesReprogramadas + solicitudesNegadas);
    
    // Tabla de ocupación por hora
    printf("\n📅 OCUPACIÓN POR HORA:\n");
    printf("   ┌──────┬───────────┬────────────┐\n");
    printf("   │ Hora │ Personas  │ Porcentaje │\n");
    printf("   ├──────┼───────────┼────────────┤\n");
    for (int i = 0; i < MAX_HORAS; i++) {
        int hora = HORAS_MIN + i;
        if (hora <= horaFinal) {
            int porcentaje = (ocupacionPorHora[i] * 100) / aforoMaximo;
            printf("   │ %02d:00│    %3d    │    %3d%%   │\n", 
                   hora, ocupacionPorHora[i], porcentaje);
        }
    }
    printf("   └──────┴───────────┴────────────┘\n");
    
    pthread_mutex_unlock(&mutexEstadisticas);
    pthread_mutex_unlock(&mutexReservas);
}

/* ============================================================================
 * LIMPIEZA DE RECURSOS
 * ============================================================================ */
void limpiarRecursos() {
    // Eliminar pipe nominal
    unlink(pipeRecibe);
    
    // Destruir mutexes
    pthread_mutex_destroy(&mutexReservas);
    pthread_mutex_destroy(&mutexAgentes);
    pthread_mutex_destroy(&mutexEstadisticas);
}

/* ============================================================================
 * FUNCIONES DE UTILIDAD
 * ============================================================================ */
int validarHora(int hora) {
    return (hora >= HORAS_MIN && hora <= HORAS_MAX);
}

int indiceHora(int hora) {
    return hora - HORAS_MIN;
}