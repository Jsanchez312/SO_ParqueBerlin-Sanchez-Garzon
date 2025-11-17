# 🎢 Sistema de Reservas - Parque Berlín

[![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C_(programming_language))
[![POSIX](https://img.shields.io/badge/POSIX-Threads-orange?style=for-the-badge)](https://pubs.opengroup.org/onlinepubs/9699919799/)

> **Proyecto Final - Sistemas Operativos 2025-30**  
> Pontificia Universidad Javeriana  
> Departamento de Ingeniería de Sistemas


Sistema concurrente de gestión de reservas para el Parque Berlín, implementado en C utilizando procesos POSIX, hilos (threads), pipes nominales (FIFOs) y mecanismos de sincronización mediante mutex.

## 📖 Descripción

El Parque Berlín es un parque privado pequeño que durante la época vacacional recibe muchas familias, causando colapso en sus servicios. Este sistema permite realizar reservas por horas para controlar el aforo máximo del parque, garantizando una experiencia de calidad para los visitantes.

### Problema

- **Sobrecarga de servicios** durante épocas vacacionales
- **Falta de control** sobre el número de visitantes
- **Necesidad de planificación** para familias

### Solución

Sistema cliente-servidor que:
- Gestiona reservas por horas (slots de 2 horas)
- Controla el aforo máximo del parque
- Permite múltiples puntos de venta (agentes) concurrentes
- Propone horas alternativas cuando no hay disponibilidad
- Genera reportes estadísticos automáticos

---

## ✨ Características

### Funcionalidades Principales

- ✅ **Gestión de Aforo**: Control estricto del número máximo de personas por hora
- ✅ **Reservas de 2 horas**: Cada reserva ocupa dos franjas horarias consecutivas
- ✅ **Múltiples Agentes**: Soporte para múltiples puntos de venta concurrentes
- ✅ **Reprogramación Inteligente**: Búsqueda automática de horas alternativas
- ✅ **Simulación de Tiempo**: Reloj configurable para simular el paso del tiempo
- ✅ **Reportes Estadísticos**: Generación automática de métricas y estadísticas

### Respuestas del Sistema

El controlador puede responder de tres formas:

1. **Reserva Aprobada**: La hora solicitada está disponible
2. **Reserva Reprogramada**: Se propone una hora alternativa disponible
3. **Reserva Negada**: No hay disponibilidad en todo el periodo

---

### Tecnologías Utilizadas

- **Lenguaje**: C (estándar C99)
- **Sistema Operativo**: Linux (POSIX compliant)
- **Concurrencia**: POSIX Threads (pthread)
- **IPC**: Named Pipes (FIFOs)
- **Sincronización**: Mutex POSIX
- **Compilador**: GCC 9.0+

---

## 🔧 Requisitos

### Software Necesario

- **Sistema Operativo**: Linux (Ubuntu 20.04+, Debian, Fedora, etc.) o WSL2
- **Compilador**: GCC 9.0 o superior
- **Make**: GNU Make 4.0+
- **Bibliotecas**: 
  - pthread (POSIX Threads)
  - Biblioteca estándar de C

### Instalación de Dependencias

#### Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential gcc make


#### Fedora/RHEL
sudo dnf install gcc make


#### WSL2 (Windows)
Desde PowerShell (como administrador)
wsl --install -d Ubuntu

Dentro de WSL
sudo apt-get update
sudo apt-get install build-essential gcc make

---

## 📥 Instalación

### 1. Clonar el Repositorio

git clone https://github.com/lJuam/SO_ParqueBerlin


### 2. Compilar el Proyecto
make

Esto generará dos ejecutables:
- `controlador`: Servidor del sistema de reservas
- `agente`: Cliente agente de reserva

### 3. Verificar la Compilación
make help

---

## 🚀 Uso

### Iniciar el Controlador (Servidor)

En una terminal, ejecuta:

./controlador -i 7 -f 19 -s 5 -t 20 -p pipe_control


**Parámetros**:
- `-i 7`: Hora inicial de operación (7:00 AM)
- `-f 19`: Hora final de operación (7:00 PM)
- `-s 5`: Segundos por hora de simulación (5 segundos = 1 hora simulada)
- `-t 20`: Aforo máximo del parque (20 personas)
- `-p pipe_control`: Nombre del pipe principal de comunicación

### Iniciar un Agente (Cliente)

En otra(s) terminal(es), ejecuta:

./agente -s AgenteA -a examples/solicitudes_ejemplo1.csv -p pipe_control

**Parámetros**:
- `-s AgenteA`: Nombre identificador del agente
- `-a solicitudes.csv`: Archivo CSV con solicitudes de reserva
- `-p pipe_control`: Nombre del pipe del controlador (debe coincidir)

### Formato del Archivo CSV

NombreFamilia,HoraSolicitada,NumeroPersonas
Garcia,8,5
Martinez,10,3
Lopez,12,8
Rodriguez,15,4

**Campos**:
1. **NombreFamilia**: Nombre de la familia que solicita
2. **HoraSolicitada**: Hora deseada (7-19)
3. **NumeroPersonas**: Cantidad de personas (debe ser ≤ aforo máximo)

---

## 💡 Ejemplos

### Ejemplo 1: Sistema Básico con 2 Agentes

**Terminal 1 - Controlador**:
./controlador -i 7 -f 19 -s 5 -t 20 -p pipe_control

**Terminal 2 - Agente A**:
./agente -s AgenciaCentro -a examples/solicitudes_ejemplo1.csv -p pipe_control


**Terminal 3 - Agente B**:
./agente -s AgenciaNorte -a examples/solicitudes_ejemplo2.csv -p pipe_control

### Ejemplo 2: Prueba de Aforo Máximo

Archivo `solicitudes_aforo.csv`:
Familia_Grande_1,8,15
Familia_Grande_2,8,10
Familia_Pequeña,8,3

En este caso:
- Primera familia: **Aprobada** (15 personas, aforo disponible: 20)
- Segunda familia: **Reprogramada** (10 personas exceden el aforo restante de 5)
- Tercera familia: **Reprogramada** (no hay cupo en hora 8)

### Ejemplo 3: Solicitudes Extemporáneas

Si la hora actual del sistema es 10:00 y se solicita hora 8:00, el sistema:
1. Detecta que es extemporánea
2. Busca automáticamente la próxima hora disponible
3. Propone hora alternativa (ej: 11:00-13:00)

---

## 🧪 Pruebas

### Suite Automatizada de Pruebas

El proyecto incluye una suite de 10 casos de prueba automatizados:

Dar permisos de ejecución
chmod +x tests/test_suite.sh

Ejecutar todas las pruebas
./tests/test_suite.sh

### Casos de Prueba Incluidos

| ID | Categoría | Descripción |
|----|-----------|-------------|
| T01 | Compilación | Verificación sin errores ni warnings |
| T02 | Funcional | Reserva simple exitosa |
| T03 | Concurrencia | Tres agentes simultáneos |
| T04 | Control de Aforo | Verificación de límite máximo |
| T05 | Excepciones | Solicitudes extemporáneas |
| T06 | Validación | Parámetros inválidos |
| T07 | Validación | Grupo excede aforo individual |
| T08 | Reportes | Generación de reporte final |
| T09 | Validación | Hora fuera del periodo |
| T10 | Rendimiento | Stress test con 15 solicitudes |

### Ejecutar Prueba Individual

Compilar primero
make

Crear archivo de prueba
echo "Garcia,8,5" > test_solicitudes.csv
echo "Martinez,10,3" >> test_solicitudes.csv

Terminal 1
./controlador -i 7 -f 19 -s 5 -t 20 -p pipe_test

Terminal 2
./agente -s TestAgente -a test_solicitudes.csv -p pipe_test

---

## 📚 Documentación

### Documentación Completa

El proyecto incluye documentación técnica completa 
  - Arquitectura del sistema
  - Diseño detallado de componentes
  - Marco teórico (IPC, Threads, Mutex)
  - Implementación y algoritmos
  - Resultados de pruebas
  - Análisis de desempeño


## 🛠️ Comandos Útiles

### Compilación y Limpieza

Compilar todo
make

Compilar solo el controlador
make controlador

Compilar solo el agente
make agente

Limpiar archivos compilados
make clean

Limpieza profunda (incluye logs y pipes)
make distclean

Ver ayuda del Makefile
make help

Ejecutar prueba básica
make test

## 🎯 Características Técnicas Avanzadas

### Mecanismos de IPC

- **Pipes Nominales (FIFOs)**: Comunicación bidireccional entre procesos
- **Doble Apertura**: Técnica para evitar deadlocks en apertura de pipes
- **Timeout en Lecturas**: `select()` para evitar bloqueos indefinidos

### Sincronización

- **3 Mutex POSIX**: 
  - `mutexReservas`: Protege array de reservas y ocupación
  - `mutexAgentes`: Protege lista de agentes registrados
  - `mutexEstadisticas`: Protege contadores estadísticos
- **Secciones Críticas**: Todas las operaciones sobre datos compartidos están protegidas

### Concurrencia

- **Hilos POSIX**: 2 hilos concurrentes en el controlador
  - Hilo del reloj (simulación)
  - Hilo de peticiones (procesamiento)
- **Múltiples Procesos**: Soporte para N agentes simultáneos

### Manejo de Señales

- **SIGINT**: Finalización ordenada del controlador (Ctrl+C)
- **SIGALRM**: Avance del reloj simulado
- **Handlers Personalizados**: Limpieza apropiada de recursos

---

## 👥 Autores

**Juan David Garzón Ballen**  
- 📧 Email: juan.garzon@javeriana.edu.co

**Juan Sánchez Panqueva**  
- 📧 Email: juan.sanchez@javeriana.edu.co
---
