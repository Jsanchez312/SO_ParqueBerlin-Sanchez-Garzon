# ============================================================================
# Makefile - Sistema de Reservas del Parque Berlin
# Proyecto: Sistemas Operativos 2025-30
# Descripcion: Archivo de compilacion para el controlador y agentes
# ============================================================================

# Compilador y flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -pthread

# Nombres de los ejecutables
CONTROLADOR = controlador
AGENTE = agente

# Archivos objeto
CONTROLADOR_OBJ = controlador.o
AGENTE_OBJ = agente.o

# Regla por defecto: compilar todo
all: $(CONTROLADOR) $(AGENTE)
	@echo "=============================================="
	@echo "Compilacion exitosa"
	@echo "=============================================="
	@echo "Ejecutables generados:"
	@echo "  - ./$(CONTROLADOR) - Servidor de reservas"
	@echo "  - ./$(AGENTE)      - Cliente agente"
	@echo "=============================================="

# Compilar el controlador
$(CONTROLADOR): $(CONTROLADOR_OBJ)
	@echo "Enlazando $(CONTROLADOR)..."
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "$(CONTROLADOR) compilado correctamente"

# Compilar el agente
$(AGENTE): $(AGENTE_OBJ)
	@echo "Enlazando $(AGENTE)..."
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "$(AGENTE) compilado correctamente"

# Regla para compilar archivos .c a .o
%.o: %.c
	@echo "Compilando $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Limpiar archivos generados
clean:
	@echo "Limpiando archivos generados..."
	rm -f $(CONTROLADOR) $(AGENTE) *.o
	rm -f pipe_*
	@echo "Limpieza completada"

# Limpiar todo (incluyendo archivos de prueba)
distclean: clean
	@echo "Limpieza profunda..."
	rm -f *.log *.txt *.csv
	rm -rf test_results_*
	@echo "Limpieza profunda completada"

# Ayuda
help:
	@echo "=============================================="
	@echo "  Makefile - Sistema de Reservas"
	@echo "=============================================="
	@echo "Objetivos disponibles:"
	@echo "  make         - Compilar todo el proyecto"
	@echo "  make all     - Compilar todo el proyecto"
	@echo "  make clean   - Limpiar archivos compilados"
	@echo "  make distclean - Limpieza profunda"
	@echo "  make help    - Mostrar esta ayuda"
	@echo "  make test    - Ejecutar prueba basica"
	@echo "=============================================="

# Prueba basica del sistema
test: all
	@echo "=============================================="
	@echo "  Ejecutando prueba basica"
	@echo "=============================================="
	@echo "1. Creando archivo de prueba..."
	@echo "Garcia,8,5" > test_solicitudes.csv
	@echo "Martinez,10,3" >> test_solicitudes.csv
	@echo "Lopez,12,8" >> test_solicitudes.csv
	@echo "Archivo test_solicitudes.csv creado"
	@echo ""
	@echo "2. Para ejecutar la prueba, abra 3 terminales:"
	@echo ""
	@echo "   Terminal 1 (Controlador):"
	@echo "   ./$(CONTROLADOR) -i 7 -f 19 -s 5 -t 20 -p pipe_control"
	@echo ""
	@echo "   Terminal 2 (Agente 1):"
	@echo "   ./$(AGENTE) -s AgenteA -a test_solicitudes.csv -p pipe_control"
	@echo ""
	@echo "   Terminal 3 (Agente 2, opcional):"
	@echo "   ./$(AGENTE) -s AgenteB -a test_solicitudes.csv -p pipe_control"
	@echo "=============================================="
