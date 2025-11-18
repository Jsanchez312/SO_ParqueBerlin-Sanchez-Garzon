/*****************************************************
 * PONTIFICIA UNIVERSIDAD JAVERIANA
 *
 * Materia: Sistemas Operativos
 * Docente: J. Corredor, PhD
 * Autor: Juan David Garzon Ballen, Juan Sanchez Panqueva
 * Programa: agente.c
 * Fecha: 17 de noviembre de 2025
 * Tema: Agente del Sistema de Reservas
 * -----------------------------------------------
 * Descripción:
 * Este programa implementa el cliente del sistema de
 * reservas. Cada agente se ejecuta como un proceso
 * independiente que lee solicitudes de reserva desde
 * un archivo de texto y las envía al controlador
 * a través de un named pipe (FIFO) principal.
 * Para recibir respuestas, cada agente crea su propio
 * pipe nominal y utiliza una técnica de "doble open"
 * para evitar deadlocks durante la conexión inicial,
 * asegurando una comunicación robusta y sincronizada
 * con el servidor.
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* ============================================================================
 * CONSTANTES Y DEFINICIONES
 * ============================================================================ */
#define MAX_BUFFER 1024
#define MAX_NOMBRE 128  // Para nombres de familias y agentes
#define MAX_PIPE_NAME 256  // Buffer más grande para nombres de pipes
#define MAX_LINEA 256
#define TIEMPO_ESPERA 2  // Segundos de espera entre solicitudes

/* Tipos de mensaje entre agente y controlador */
typedef enum {
    MSG_REGISTRO,
    MSG_SOLICITUD_RESERVA,
    MSG_FIN_AGENTE
} TipoMensaje;

/* Tipos de respuesta del controlador */
typedef enum {
    RESP_HORA_ACTUAL,
    RESP_RESERVA_OK,
    RESP_RESERVA_REPROG,
    RESP_RESERVA_NEGADA,
    RESP_FIN_DIA
} TipoRespuesta;

/* Estructura para mensajes del agente al controlador */
typedef struct {
    TipoMensaje tipo;
    char nombreAgente[MAX_NOMBRE];
    char pipeRespuesta[MAX_NOMBRE];
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

/* ============================================================================
 * VARIABLES GLOBALES
 * ============================================================================ */
char nombreAgente[MAX_NOMBRE];
char archivoSolicitudes[MAX_NOMBRE];
char pipeControlador[MAX_NOMBRE];
char pipeRespuesta[MAX_PIPE_NAME];  // Buffer más grande para el nombre del pipe
int horaActualSimulacion = -1;

/* ============================================================================
 * PROTOTIPOS DE FUNCIONES
 * ============================================================================ */
void procesarArgumentos(int argc, char *argv[]);
int registrarseConControlador();
void procesarSolicitudes();
void enviarMensaje(MensajeAgente *msg);
int recibirRespuesta(RespuestaControlador *resp);
void imprimirRespuesta(RespuestaControlador *resp, char *nombreFamilia);
void limpiarRecursos();
void parsearLineaCSV(char *linea, char *familia, int *hora, int *personas);

/* ============================================================================
 * FUNCIÓN PRINCIPAL
 * ============================================================================ */
int main(int argc, char *argv[]) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  AGENTE DE RESERVAS - PARQUE BERLÍN                        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Procesar argumentos
    procesarArgumentos(argc, argv);
    
    // Crear nombre único para pipe de respuestas
    snprintf(pipeRespuesta, MAX_PIPE_NAME, "pipe_%s_%d", nombreAgente, getpid());
    
    // Crear pipe para recibir respuestas
    unlink(pipeRespuesta);
    unlink(pipeRespuesta);
    if (mkfifo(pipeRespuesta, 0666) == -1) {
        if (errno != EEXIST) {
            perror("Error al crear pipe de respuesta");
            exit(EXIT_FAILURE);
        }
    }
    
    printf("✓ Agente '%s' iniciado\n", nombreAgente);
    printf("✓ Archivo de solicitudes: %s\n", archivoSolicitudes);
    printf("✓ Pipe de respuesta: %s\n\n", pipeRespuesta);
    
    // Registrarse con el controlador
    if (!registrarseConControlador()) {
        fprintf(stderr, "Error al registrarse con el controlador\n");
        limpiarRecursos();
        exit(EXIT_FAILURE);
    }
    
    printf("✓ Registrado correctamente con el controlador\n");
    printf("✓ Hora actual del sistema: %d:00\n\n", horaActualSimulacion);
    
    // Procesar solicitudes del archivo
    procesarSolicitudes();
    
    // Notificar finalización al controlador
    MensajeAgente msgFin;
    msgFin.tipo = MSG_FIN_AGENTE;
    strncpy(msgFin.nombreAgente, nombreAgente, MAX_NOMBRE - 1);
    enviarMensaje(&msgFin);
    
    printf("\n✓ Agente %s termina.\n\n", nombreAgente);
    
    // Limpiar recursos
    limpiarRecursos();
    
    return 0;
}

/* ============================================================================
 * PROCESAMIENTO DE ARGUMENTOS
 * ============================================================================ */
void procesarArgumentos(int argc, char *argv[]) {
    int opt;
    int flagS = 0, flagA = 0, flagP = 0;
    
    while ((opt = getopt(argc, argv, "s:a:p:")) != -1) {
        switch (opt) {
            case 's':
                strncpy(nombreAgente, optarg, MAX_NOMBRE - 1);
                nombreAgente[MAX_NOMBRE - 1] = '\0';
                flagS = 1;
                break;
            case 'a':
                strncpy(archivoSolicitudes, optarg, MAX_NOMBRE - 1);
                archivoSolicitudes[MAX_NOMBRE - 1] = '\0';
                flagA = 1;
                break;
            case 'p':
                strncpy(pipeControlador, optarg, MAX_NOMBRE - 1);
                pipeControlador[MAX_NOMBRE - 1] = '\0';
                flagP = 1;
                break;
            default:
                fprintf(stderr, "Uso: %s -s <nombre> -a <fileSolicitud> -p <pipeRecibe>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    // Validar que todos los parámetros fueron proporcionados
    if (!flagS || !flagA || !flagP) {
        fprintf(stderr, "Error: Faltan parámetros obligatorios\n");
        fprintf(stderr, "Uso: %s -s <nombre> -a <fileSolicitud> -p <pipeRecibe>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

/* ============================================================================
 * REGISTRO CON EL CONTROLADOR
 * ============================================================================ */
int registrarseConControlador() {
    MensajeAgente msg;
    RespuestaControlador resp;
    
    // Preparar mensaje de registro
    msg.tipo = MSG_REGISTRO;
    strncpy(msg.nombreAgente, nombreAgente, MAX_NOMBRE - 1);
    strncpy(msg.pipeRespuesta, pipeRespuesta, MAX_NOMBRE - 1);
    msg.horaSolicitada = 0;
    msg.numPersonas = 0;
    
    // Enviar mensaje de registro
    enviarMensaje(&msg);
    
    // Esperar respuesta con hora actual
    if (!recibirRespuesta(&resp)) {
        return 0;
    }
    
    if (resp.tipo == RESP_HORA_ACTUAL) {
        horaActualSimulacion = resp.horaActual;
        return 1;
    }
    
    return 0;
}

/* ============================================================================
 * PROCESAMIENTO DE SOLICITUDES
 * ============================================================================ */
void procesarSolicitudes() {
    FILE *archivo;
    char linea[MAX_LINEA];
    char nombreFamilia[MAX_NOMBRE];
    int horaSolicitada;
    int numPersonas;
    MensajeAgente msg;
    RespuestaControlador resp;
    int numLinea = 0;
    
    // Abrir archivo de solicitudes
    archivo = fopen(archivoSolicitudes, "r");
    if (archivo == NULL) {
        perror("Error al abrir archivo de solicitudes");
        return;
    }
    
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("         PROCESANDO SOLICITUDES DE RESERVA\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    
    // Leer y procesar cada línea del archivo
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        numLinea++;
        
        // Eliminar salto de línea
        linea[strcspn(linea, "\n")] = 0;
        
        // Ignorar líneas vacías
        if (strlen(linea) == 0) {
            continue;
        }
        
        // Parsear línea CSV
        parsearLineaCSV(linea, nombreFamilia, &horaSolicitada, &numPersonas);
        
        printf("┌─────────────────────────────────────────────────────────┐\n");
        printf("│ Solicitud #%d                                            │\n", numLinea);
        printf("├─────────────────────────────────────────────────────────┤\n");
        printf("│ Familia: %-47s│\n", nombreFamilia);
        printf("│ Hora solicitada: %d:00                                   │\n", horaSolicitada);
        printf("│ Personas: %-3d                                           │\n", numPersonas);
        printf("└─────────────────────────────────────────────────────────┘\n");
        
        // Validar que la hora no sea anterior a la hora actual
        if (horaSolicitada < horaActualSimulacion) {
            printf("⚠  ADVERTENCIA: Hora solicitada (%d:00) es anterior a la hora actual (%d:00)\n", 
                   horaSolicitada, horaActualSimulacion);
            printf("   El controlador intentará reprogramar la reserva.\n\n");
        }
        
        // Preparar mensaje de solicitud
        msg.tipo = MSG_SOLICITUD_RESERVA;
        strncpy(msg.nombreAgente, nombreAgente, MAX_NOMBRE - 1);
        strncpy(msg.pipeRespuesta, pipeRespuesta, MAX_NOMBRE - 1);
        strncpy(msg.nombreFamilia, nombreFamilia, MAX_NOMBRE - 1);
        msg.horaSolicitada = horaSolicitada;
        msg.numPersonas = numPersonas;
        
        // Enviar solicitud
        enviarMensaje(&msg);
        
        // Esperar respuesta
        if (recibirRespuesta(&resp)) {
            imprimirRespuesta(&resp, nombreFamilia);
        } else {
            printf("✗ Error al recibir respuesta del controlador\n\n");
        }
        
        // Esperar antes de enviar la siguiente solicitud
        sleep(TIEMPO_ESPERA);
    }
    
    fclose(archivo);
    
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("         FIN DE SOLICITUDES\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

/* ============================================================================
 * ENVÍO DE MENSAJES AL CONTROLADOR
 * ============================================================================ */
void enviarMensaje(MensajeAgente *msg) {
    int fdPipeControlador;
    int intentos = 0;
    int maxIntentos = 5;
    
    // Abrir pipe del controlador (con reintentos)
    while (intentos < maxIntentos) {
        fdPipeControlador = open(pipeControlador, O_WRONLY);
        if (fdPipeControlador != -1) {
            break;
        }
        
        if (errno == ENXIO) {
            // El controlador aún no ha abierto el pipe
            sleep(1);
            intentos++;
        } else {
            perror("Error al abrir pipe del controlador");
            exit(EXIT_FAILURE);
        }
    }
    
    if (fdPipeControlador == -1) {
        fprintf(stderr, "Error: No se pudo conectar con el controlador después de %d intentos\n", maxIntentos);
        exit(EXIT_FAILURE);
    }
    
    // Escribir mensaje
    if (write(fdPipeControlador, msg, sizeof(MensajeAgente)) == -1) {
        perror("Error al enviar mensaje al controlador");
        close(fdPipeControlador);
        exit(EXIT_FAILURE);
    }
    
    close(fdPipeControlador);
}

/* ============================================================================
 * RECEPCIÓN DE RESPUESTAS DEL CONTROLADOR
 * ============================================================================ */
int recibirRespuesta(RespuestaControlador *resp) {
    int fdPipeRespuesta;
    ssize_t bytesLeidos;
    
    // Abrir pipe de respuesta
    fdPipeRespuesta = open(pipeRespuesta, O_RDONLY);
    if (fdPipeRespuesta == -1) {
        perror("Error al abrir pipe de respuesta");
        return 0;
    }
    
    // Leer respuesta
    bytesLeidos = read(fdPipeRespuesta, resp, sizeof(RespuestaControlador));
    
    close(fdPipeRespuesta);
    
    if (bytesLeidos != sizeof(RespuestaControlador)) {
        fprintf(stderr, "Error: Respuesta incompleta del controlador\n");
        return 0;
    }
    
    return 1;
}

/* ============================================================================
 * IMPRESIÓN DE RESPUESTAS
 * ============================================================================ */
void imprimirRespuesta(RespuestaControlador *resp, char *nombreFamilia) {
    printf("\n╭─────────────────────────────────────────────────────────╮\n");
    printf("│ 📨 RESPUESTA DEL CONTROLADOR                            │\n");
    printf("├─────────────────────────────────────────────────────────┤\n");
    
    switch (resp->tipo) {
        case RESP_HORA_ACTUAL:
            printf("│ Tipo: Hora Actual                                       │\n");
            printf("│ Hora: %d:00                                              │\n", resp->horaActual);
            break;
            
        case RESP_RESERVA_OK:
            printf("│ Estado: ✓ RESERVA APROBADA                              │\n");
            printf("│ Familia: %-47s│\n", nombreFamilia);
            printf("│ Hora asignada: %d:00 - %d:00                             │\n", 
                   resp->horaAsignada, resp->horaAsignada + 2);
            printf("│ %s │\n", resp->mensaje);
            break;
            
        case RESP_RESERVA_REPROG:
            printf("│ Estado: ⚠ RESERVA REPROGRAMADA                          │\n");
            printf("│ Familia: %-47s│\n", nombreFamilia);
            printf("│ Nueva hora: %d:00 - %d:00                                │\n", 
                   resp->horaAsignada, resp->horaAsignada + 2);
            printf("│ Motivo: La hora solicitada no estaba disponible        │\n");
            break;
            
        case RESP_RESERVA_NEGADA:
            printf("│ Estado: ✗ RESERVA NEGADA                                │\n");
            printf("│ Familia: %-47s│\n", nombreFamilia);
            printf("│ Motivo:                                                 │\n");
            
            // Dividir mensaje en líneas si es muy largo
            char *token = strtok(resp->mensaje, ".");
            while (token != NULL) {
                printf("│   %s│\n", token);
                token = strtok(NULL, ".");
            }
            break;
            
        case RESP_FIN_DIA:
            printf("│ Estado: Fin del día de operaciones                     │\n");
            break;
            
        default:
            printf("│ Estado: Respuesta desconocida                           │\n");
            break;
    }
    
    printf("╰─────────────────────────────────────────────────────────╯\n\n");
}

/* ============================================================================
 * PARSEO DE LÍNEA CSV
 * ============================================================================ */
void parsearLineaCSV(char *linea, char *familia, int *hora, int *personas) {
    char *token;
    char lineaCopia[MAX_LINEA];
    
    // Hacer copia de la línea para no modificar la original
    strncpy(lineaCopia, linea, MAX_LINEA - 1);
    lineaCopia[MAX_LINEA - 1] = '\0';
    
    // Parsear familia
    token = strtok(lineaCopia, ",");
    if (token != NULL) {
        strncpy(familia, token, MAX_NOMBRE - 1);
        familia[MAX_NOMBRE - 1] = '\0';
        
        // Eliminar espacios al inicio y al final
        while (*familia == ' ') familia++;
    } else {
        familia[0] = '\0';
    }
    
    // Parsear hora
    token = strtok(NULL, ",");
    if (token != NULL) {
        *hora = atoi(token);
    } else {
        *hora = 0;
    }
    
    // Parsear número de personas
    token = strtok(NULL, ",");
    if (token != NULL) {
        *personas = atoi(token);
    } else {
        *personas = 0;
    }
}

/* ============================================================================
 * LIMPIEZA DE RECURSOS
 * ============================================================================ */
void limpiarRecursos() {
    // Eliminar pipe de respuesta
    unlink(pipeRespuesta);
}