#!/bin/bash

#######################################################
# PONTIFICIA UNIVERSIDAD JAVERIANA
# Sistema de Reservas - Suite de Pruebas Automatizadas
# Autor: Juan David Garzon Ballen
# Fecha: 17 de noviembre de 2025
#
# Descripción:
# Script que ejecuta múltiples casos de prueba para
# verificar el correcto funcionamiento del sistema
# de reservas del Parque Berlín.
#######################################################

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Contadores de pruebas
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Directorio para archivos de prueba y logs
TEST_DIR="test_results_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$TEST_DIR"
LOG_FILE="$TEST_DIR/test_suite.log"

# Función para imprimir encabezado
print_header() {
    echo -e "${BOLD}${CYAN}"
    echo "╔═══════════════════════════════════════════════════════════════╗"
    echo "║         SUITE DE PRUEBAS - SISTEMA DE RESERVAS               ║"
    echo "║              Parque Berlín - Proyecto SO                     ║"
    echo "╚═══════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

# Función para logging
log() {
    echo -e "$1" | tee -a "$LOG_FILE"
}

# Función para imprimir resultado de prueba
print_test_result() {
    local test_name="$1"
    local result="$2"
    local message="$3"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if [ "$result" = "PASS" ]; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
        log "${GREEN}✓ PASS${NC} - Test #$TOTAL_TESTS: $test_name"
        [ -n "$message" ] && log "         $message"
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
        log "${RED}✗ FAIL${NC} - Test #$TOTAL_TESTS: $test_name"
        [ -n "$message" ] && log "         ${RED}$message${NC}"
    fi
    echo "" | tee -a "$LOG_FILE"
}

# Función para limpiar procesos y archivos
cleanup() {
    log "${YELLOW}Limpiando recursos...${NC}"
    
    # Matar procesos controlador y agente
    pkill -9 -f "./controlador" 2>/dev/null
    pkill -9 -f "./agente" 2>/dev/null
    
    # Eliminar pipes
    rm -f pipe_* 2>/dev/null
    
    # Pequeña pausa
    sleep 1
}

# Función para esperar que un proceso termine o timeout
wait_for_process() {
    local pid=$1
    local timeout=$2
    local count=0
    
    while kill -0 $pid 2>/dev/null && [ $count -lt $timeout ]; do
        sleep 1
        count=$((count + 1))
    done
    
    if kill -0 $pid 2>/dev/null; then
        kill -9 $pid 2>/dev/null
        return 1
    fi
    return 0
}

# Función para verificar compilación
test_compilation() {
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    log "${BOLD}${BLUE}  TEST 1: VERIFICACIÓN DE COMPILACIÓN${NC}"
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    
    # Limpiar compilaciones anteriores
    make clean > /dev/null 2>&1
    
    # Compilar
    if make > "$TEST_DIR/compilation.log" 2>&1; then
        if [ -f "./controlador" ] && [ -f "./agente" ]; then
            print_test_result "Compilación exitosa" "PASS" "Ejecutables generados correctamente"
            return 0
        else
            print_test_result "Compilación" "FAIL" "Ejecutables no encontrados"
            return 1
        fi
    else
        print_test_result "Compilación" "FAIL" "Error en compilación (ver $TEST_DIR/compilation.log)"
        return 1
    fi
}

# TEST 2: Reserva simple exitosa
test_simple_reservation() {
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    log "${BOLD}${BLUE}  TEST 2: RESERVA SIMPLE EXITOSA${NC}"
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    
    cleanup
    
    # Crear archivo de solicitudes
    cat > "$TEST_DIR/test2_solicitudes.csv" << EOF
Familia_Garcia,8,5
Familia_Lopez,10,3
EOF
    
    # Iniciar controlador
    ./controlador -i 7 -f 12 -s 2 -t 20 -p pipe_test2 > "$TEST_DIR/test2_controlador.log" 2>&1 &
    local ctrl_pid=$!
    sleep 2
    
    # Iniciar agente
    ./agente -s AgenteTest2 -a "$TEST_DIR/test2_solicitudes.csv" -p pipe_test2 > "$TEST_DIR/test2_agente.log" 2>&1 &
    local agent_pid=$!
    
    # Esperar a que termine el agente
    wait_for_process $agent_pid 15
    
    # Esperar a que termine el controlador
    sleep 3
    kill -INT $ctrl_pid 2>/dev/null
    wait_for_process $ctrl_pid 5
    
    # Verificar resultados
    if grep -q "RESERVA APROBADA" "$TEST_DIR/test2_agente.log"; then
        print_test_result "Reserva simple exitosa" "PASS" "Reservas aprobadas correctamente"
    else
        print_test_result "Reserva simple exitosa" "FAIL" "No se encontraron reservas aprobadas"
    fi
    
    cleanup
}

# TEST 3: Múltiples agentes concurrentes
test_multiple_agents() {
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    log "${BOLD}${BLUE}  TEST 3: MÚLTIPLES AGENTES CONCURRENTES${NC}"
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    
    cleanup
    
    # Crear archivos de solicitudes para 3 agentes
    cat > "$TEST_DIR/test3_agente1.csv" << EOF
Familia_A1,8,4
Familia_A2,10,3
EOF
    
    cat > "$TEST_DIR/test3_agente2.csv" << EOF
Familia_B1,9,5
Familia_B2,11,2
EOF
    
    cat > "$TEST_DIR/test3_agente3.csv" << EOF
Familia_C1,8,3
Familia_C2,12,4
EOF
    
    # Iniciar controlador
    ./controlador -i 7 -f 14 -s 2 -t 15 -p pipe_test3 > "$TEST_DIR/test3_controlador.log" 2>&1 &
    local ctrl_pid=$!
    sleep 2
    
    # Iniciar 3 agentes simultáneamente
    ./agente -s Agente1 -a "$TEST_DIR/test3_agente1.csv" -p pipe_test3 > "$TEST_DIR/test3_agente1_out.log" 2>&1 &
    local agent1_pid=$!
    
    ./agente -s Agente2 -a "$TEST_DIR/test3_agente2.csv" -p pipe_test3 > "$TEST_DIR/test3_agente2_out.log" 2>&1 &
    local agent2_pid=$!
    
    ./agente -s Agente3 -a "$TEST_DIR/test3_agente3.csv" -p pipe_test3 > "$TEST_DIR/test3_agente3_out.log" 2>&1 &
    local agent3_pid=$!
    
    # Esperar a que terminen los agentes
    wait_for_process $agent1_pid 20
    wait_for_process $agent2_pid 20
    wait_for_process $agent3_pid 20
    
    sleep 3
    kill -INT $ctrl_pid 2>/dev/null
    wait_for_process $ctrl_pid 5
    
    # Verificar que se registraron los 3 agentes
    local registered_agents=$(grep -c "Agente.*registrado" "$TEST_DIR/test3_controlador.log")
    
    if [ "$registered_agents" -ge 3 ]; then
        print_test_result "Múltiples agentes concurrentes" "PASS" "$registered_agents agentes registrados correctamente"
    else
        print_test_result "Múltiples agentes concurrentes" "FAIL" "Solo $registered_agents agentes registrados"
    fi
    
    cleanup
}

# TEST 4: Aforo máximo alcanzado
test_max_capacity() {
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    log "${BOLD}${BLUE}  TEST 4: AFORO MÁXIMO ALCANZADO${NC}"
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    
    cleanup
    
    # Crear solicitudes que excedan el aforo
    cat > "$TEST_DIR/test4_solicitudes.csv" << EOF
Familia_1,8,8
Familia_2,8,8
Familia_3,8,10
EOF
    
    # Iniciar controlador con aforo máximo de 10
    ./controlador -i 7 -f 12 -s 2 -t 10 -p pipe_test4 > "$TEST_DIR/test4_controlador.log" 2>&1 &
    local ctrl_pid=$!
    sleep 2
    
    # Iniciar agente
    ./agente -s AgenteTest4 -a "$TEST_DIR/test4_solicitudes.csv" -p pipe_test4 > "$TEST_DIR/test4_agente.log" 2>&1 &
    local agent_pid=$!
    
    wait_for_process $agent_pid 20
    sleep 3
    kill -INT $ctrl_pid 2>/dev/null
    wait_for_process $ctrl_pid 5
    
    # Verificar que algunas reservas fueron reprogramadas o negadas
    if grep -q "REPROGRAMADA\|NEGADA" "$TEST_DIR/test4_agente.log"; then
        print_test_result "Control de aforo máximo" "PASS" "Sistema reprogramó/negó reservas correctamente"
    else
        print_test_result "Control de aforo máximo" "FAIL" "No se detectó control de aforo"
    fi
    
    cleanup
}

# TEST 5: Solicitudes extemporáneas
test_late_requests() {
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    log "${BOLD}${BLUE}  TEST 5: SOLICITUDES EXTEMPORÁNEAS${NC}"
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    
    cleanup
    
    # Crear solicitudes con horas que ya pasaron
    cat > "$TEST_DIR/test5_solicitudes.csv" << EOF
Familia_Tarde,7,5
Familia_Normal,12,3
EOF
    
    # Iniciar controlador con hora inicial 10
    ./controlador -i 10 -f 15 -s 2 -t 20 -p pipe_test5 > "$TEST_DIR/test5_controlador.log" 2>&1 &
    local ctrl_pid=$!
    sleep 2
    
    # Iniciar agente
    ./agente -s AgenteTest5 -a "$TEST_DIR/test5_solicitudes.csv" -p pipe_test5 > "$TEST_DIR/test5_agente.log" 2>&1 &
    local agent_pid=$!
    
    wait_for_process $agent_pid 20
    sleep 3
    kill -INT $ctrl_pid 2>/dev/null
    wait_for_process $ctrl_pid 5
    
    # Verificar manejo de hora extemporánea
    if grep -q "extemporánea\|REPROGRAMADA" "$TEST_DIR/test5_controlador.log"; then
        print_test_result "Solicitudes extemporáneas" "PASS" "Solicitudes extemporáneas manejadas correctamente"
    else
        print_test_result "Solicitudes extemporáneas" "FAIL" "No se detectó manejo de solicitudes extemporáneas"
    fi
    
    cleanup
}

# TEST 6: Validación de parámetros inválidos
test_invalid_parameters() {
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    log "${BOLD}${BLUE}  TEST 6: VALIDACIÓN DE PARÁMETROS INVÁLIDOS${NC}"
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    
    cleanup
    
    # Probar controlador con horas inválidas
    ./controlador -i 25 -f 30 -s 2 -t 20 -p pipe_test6 > "$TEST_DIR/test6_invalid.log" 2>&1 &
    local ctrl_pid=$!
    sleep 2
    
    # Verificar que terminó con error
    if ! kill -0 $ctrl_pid 2>/dev/null; then
        if grep -q "Error\|fuera del rango" "$TEST_DIR/test6_invalid.log"; then
            print_test_result "Validación de parámetros inválidos" "PASS" "Parámetros inválidos rechazados correctamente"
        else
            print_test_result "Validación de parámetros inválidos" "FAIL" "No se encontró mensaje de error apropiado"
        fi
    else
        kill -9 $ctrl_pid 2>/dev/null
        print_test_result "Validación de parámetros inválidos" "FAIL" "Proceso no terminó con error"
    fi
    
    cleanup
}

# TEST 7: Personas exceden aforo máximo
test_exceed_capacity() {
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    log "${BOLD}${BLUE}  TEST 7: GRUPO EXCEDE AFORO MÁXIMO${NC}"
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    
    cleanup
    
    # Crear solicitud que excede aforo individual
    cat > "$TEST_DIR/test7_solicitudes.csv" << EOF
Familia_Grande,8,25
EOF
    
    # Iniciar controlador con aforo máximo de 20
    ./controlador -i 7 -f 12 -s 2 -t 20 -p pipe_test7 > "$TEST_DIR/test7_controlador.log" 2>&1 &
    local ctrl_pid=$!
    sleep 2
    
    # Iniciar agente
    ./agente -s AgenteTest7 -a "$TEST_DIR/test7_solicitudes.csv" -p pipe_test7 > "$TEST_DIR/test7_agente.log" 2>&1 &
    local agent_pid=$!
    
    wait_for_process $agent_pid 15
    sleep 3
    kill -INT $ctrl_pid 2>/dev/null
    wait_for_process $ctrl_pid 5
    
    # Verificar que fue negada
    if grep -q "excede el aforo\|NEGADA" "$TEST_DIR/test7_agente.log"; then
        print_test_result "Grupo excede aforo máximo" "PASS" "Reserva negada correctamente"
    else
        print_test_result "Grupo excede aforo máximo" "FAIL" "Reserva no fue rechazada"
    fi
    
    cleanup
}

# TEST 8: Reporte final completo
test_final_report() {
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    log "${BOLD}${BLUE}  TEST 8: GENERACIÓN DE REPORTE FINAL${NC}"
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    
    cleanup
    
    # Crear solicitudes variadas
    cat > "$TEST_DIR/test8_solicitudes.csv" << EOF
Familia_1,8,5
Familia_2,10,4
Familia_3,12,6
EOF
    
    # Iniciar controlador
    ./controlador -i 7 -f 13 -s 2 -t 15 -p pipe_test8 > "$TEST_DIR/test8_controlador.log" 2>&1 &
    local ctrl_pid=$!
    sleep 2
    
    # Iniciar agente
    ./agente -s AgenteTest8 -a "$TEST_DIR/test8_solicitudes.csv" -p pipe_test8 > "$TEST_DIR/test8_agente.log" 2>&1 &
    local agent_pid=$!
    
    wait_for_process $agent_pid 20
    sleep 3
    kill -INT $ctrl_pid 2>/dev/null
    wait_for_process $ctrl_pid 5
    
    # Verificar elementos del reporte
    local report_elements=0
    grep -q "HORAS PICO" "$TEST_DIR/test8_controlador.log" && report_elements=$((report_elements + 1))
    grep -q "HORAS VALLE" "$TEST_DIR/test8_controlador.log" && report_elements=$((report_elements + 1))
    grep -q "Solicitudes aceptadas" "$TEST_DIR/test8_controlador.log" && report_elements=$((report_elements + 1))
    grep -q "Solicitudes negadas" "$TEST_DIR/test8_controlador.log" && report_elements=$((report_elements + 1))
    grep -q "OCUPACIÓN POR HORA" "$TEST_DIR/test8_controlador.log" && report_elements=$((report_elements + 1))
    
    if [ $report_elements -ge 4 ]; then
        print_test_result "Generación de reporte final" "PASS" "$report_elements/5 elementos del reporte encontrados"
    else
        print_test_result "Generación de reporte final" "FAIL" "Solo $report_elements/5 elementos encontrados"
    fi
    
    cleanup
}

# TEST 9: Hora fuera del periodo de simulación
test_out_of_range() {
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    log "${BOLD}${BLUE}  TEST 9: HORA FUERA DEL PERIODO DE SIMULACIÓN${NC}"
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    
    cleanup
    
    # Crear solicitud fuera del rango
    cat > "$TEST_DIR/test9_solicitudes.csv" << EOF
Familia_Futura,20,5
EOF
    
    # Iniciar controlador con rango 7-15
    ./controlador -i 7 -f 15 -s 2 -t 20 -p pipe_test9 > "$TEST_DIR/test9_controlador.log" 2>&1 &
    local ctrl_pid=$!
    sleep 2
    
    # Iniciar agente
    ./agente -s AgenteTest9 -a "$TEST_DIR/test9_solicitudes.csv" -p pipe_test9 > "$TEST_DIR/test9_agente.log" 2>&1 &
    local agent_pid=$!
    
    wait_for_process $agent_pid 15
    sleep 3
    kill -INT $ctrl_pid 2>/dev/null
    wait_for_process $ctrl_pid 5
    
    # Verificar que fue negada por estar fuera de rango
    if grep -q "fuera del.*periodo\|NEGADA" "$TEST_DIR/test9_agente.log"; then
        print_test_result "Hora fuera del periodo" "PASS" "Solicitud fuera de rango rechazada"
    else
        print_test_result "Hora fuera del periodo" "FAIL" "Solicitud no fue rechazada apropiadamente"
    fi
    
    cleanup
}

# TEST 10: Stress test - Muchas solicitudes
test_stress() {
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    log "${BOLD}${BLUE}  TEST 10: STRESS TEST - MÚLTIPLES SOLICITUDES${NC}"
    log "${BOLD}${BLUE}══════════════════════════════════════════════════════════════${NC}"
    
    cleanup
    
    # Crear archivo con 15 solicitudes (reducido para mayor estabilidad)
    > "$TEST_DIR/test10_solicitudes.csv"
    for i in {1..15}; do
        hora=$((7 + (i % 9)))
        personas=$((2 + (i % 6)))
        echo "Familia_$i,$hora,$personas" >> "$TEST_DIR/test10_solicitudes.csv"
    done
    
    # Iniciar controlador con periodo más largo y más tiempo por hora
    ./controlador -i 7 -f 19 -s 2 -t 30 -p pipe_test10 > "$TEST_DIR/test10_controlador.log" 2>&1 &
    local ctrl_pid=$!
    sleep 2
    
    # Iniciar agente
    ./agente -s AgenteStress -a "$TEST_DIR/test10_solicitudes.csv" -p pipe_test10 > "$TEST_DIR/test10_agente.log" 2>&1 &
    local agent_pid=$!
    
    # Esperar más tiempo: 15 solicitudes × 2 segundos + margen = 50 segundos
    wait_for_process $agent_pid 90
    sleep 3
    kill -INT $ctrl_pid 2>/dev/null
    wait_for_process $ctrl_pid 5
    
    # Contar solicitudes procesadas
    local processed=$(grep -c "SOLICITUD DE RESERVA" "$TEST_DIR/test10_controlador.log")
    
    if [ "$processed" -ge 12 ]; then
        print_test_result "Stress test" "PASS" "$processed/15 solicitudes procesadas correctamente"
    else
        print_test_result "Stress test" "FAIL" "Solo $processed/15 solicitudes procesadas"
    fi
    
    cleanup
}

# Función para imprimir resumen final
print_summary() {
    log ""
    log "${BOLD}${CYAN}╔═══════════════════════════════════════════════════════════════╗${NC}"
    log "${BOLD}${CYAN}║                    RESUMEN DE PRUEBAS                         ║${NC}"
    log "${BOLD}${CYAN}╚═══════════════════════════════════════════════════════════════╝${NC}"
    log ""
    log "Total de pruebas ejecutadas: ${BOLD}$TOTAL_TESTS${NC}"
    log "${GREEN}Pruebas exitosas: $PASSED_TESTS${NC}"
    log "${RED}Pruebas fallidas: $FAILED_TESTS${NC}"
    log ""
    
    if [ $FAILED_TESTS -eq 0 ]; then
        log "${BOLD}${GREEN}🎉 ¡TODAS LAS PRUEBAS PASARON EXITOSAMENTE! 🎉${NC}"
    else
        log "${BOLD}${YELLOW}⚠️  Algunas pruebas fallaron. Revisar logs en: $TEST_DIR${NC}"
    fi
    
    log ""
    log "Porcentaje de éxito: $(awk "BEGIN {printf \"%.1f\", ($PASSED_TESTS/$TOTAL_TESTS)*100}")%"
    log ""
    log "Logs detallados guardados en: ${BOLD}$TEST_DIR${NC}"
    log "Log principal: ${BOLD}$LOG_FILE${NC}"
}

# Función principal
main() {
    print_header
    
    log "Iniciando suite de pruebas..."
    log "Directorio de resultados: $TEST_DIR"
    log ""
    
    # Verificar que los archivos fuente existen
    if [ ! -f "controlador.c" ] || [ ! -f "agente.c" ] || [ ! -f "Makefile" ]; then
        log "${RED}ERROR: Archivos fuente no encontrados en el directorio actual${NC}"
        exit 1
    fi
    
    # Ejecutar todas las pruebas
    test_compilation
    
    if [ $? -eq 0 ]; then
        test_simple_reservation
        test_multiple_agents
        test_max_capacity
        test_late_requests
        test_invalid_parameters
        test_exceed_capacity
        test_final_report
        test_out_of_range
        test_stress
    else
        log "${RED}La compilación falló. Abortando pruebas.${NC}"
    fi
    
    # Limpieza final
    cleanup
    
    # Imprimir resumen
    print_summary
    
    # Retornar código de salida apropiado
    if [ $FAILED_TESTS -eq 0 ]; then
        exit 0
    else
        exit 1
    fi
}

# Capturar Ctrl+C
trap cleanup INT TERM

# Ejecutar
main "$@"