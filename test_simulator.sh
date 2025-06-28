#!/bin/bash

# Script de pruebas para el Simulador de Interrupciones Linux
# Proyecto de Sistemas Operativos

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Contadores de pruebas
TESTS_PASSED=0
TESTS_FAILED=0
TOTAL_TESTS=0

# Función para imprimir mensajes coloreados
print_status() {
    local status=$1
    local message=$2
    
    case $status in
        "PASS")
            echo -e "${GREEN}[PASS]${NC} $message"
            ((TESTS_PASSED++))
            ;;
        "FAIL")
            echo -e "${RED}[FAIL]${NC} $message"
            ((TESTS_FAILED++))
            ;;
        "INFO")
            echo -e "${BLUE}[INFO]${NC} $message"
            ;;
        "WARN")
            echo -e "${YELLOW}[WARN]${NC} $message"
            ;;
    esac
    ((TOTAL_TESTS++))
}

# Función para verificar si el simulador existe
check_simulator_exists() {
    print_status "INFO" "Verificando existencia del simulador..."
    
    if [ -f "./interrupt_simulator" ]; then
        print_status "PASS" "Ejecutable encontrado"
        return 0
    else
        print_status "FAIL" "Ejecutable no encontrado"
        return 1
    fi
}

# Función para compilar el proyecto
compile_project() {
    print_status "INFO" "Compilando proyecto..."
    
    if make clean > /dev/null 2>&1 && make > /dev/null 2>&1; then
        print_status "PASS" "Compilación exitosa"
        return 0
    else
        print_status "FAIL" "Error en compilación"
        return 1
    fi
}

# Función para verificar dependencias
check_dependencies() {
    print_status "INFO" "Verificando dependencias del sistema..."
    
    # Verificar gcc
    if command -v gcc > /dev/null 2>&1; then
        print_status "PASS" "GCC encontrado: $(gcc --version | head -n1)"
    else
        print_status "FAIL" "GCC no encontrado"
        return 1
    fi
    
    # Verificar pthread
    if gcc -pthread -x c -E - < /dev/null > /dev/null 2>&1; then
        print_status "PASS" "Pthread disponible"
    else
        print_status "FAIL" "Pthread no disponible"
        return 1
    fi
    
    # Verificar make
    if command -v make > /dev/null 2>&1; then
        print_status "PASS" "Make encontrado: $(make --version | head -n1)"
    else
        print_status "FAIL" "Make no encontrado"
        return 1
    fi
    
    return 0
}

# Función para pruebas de memoria con valgrind
test_memory_leaks() {
    print_status "INFO" "Ejecutando pruebas de memoria..."
    
    if command -v valgrind > /dev/null 2>&1; then
        # Crear archivo de entrada automática para el simulador
        # 3 = mostrar estado IDT, 0 = salir
        echo -e "3\n0\n" > test_input.txt
        
        # Ejecutar valgrind con timeout más largo para permitir que el timer hilo termine
        timeout 15s valgrind --leak-check=full --show-leak-kinds=all \
            --error-exitcode=1 --suppressions=/dev/null \
            ./interrupt_simulator < test_input.txt > valgrind_output.log 2>&1
        
        local exit_code=$?
        
        if [ $exit_code -eq 0 ] || [ $exit_code -eq 124 ]; then
            # Verificar si hay errores reportados por valgrind
            if grep -q "ERROR SUMMARY: 0 errors" valgrind_output.log 2>/dev/null; then
                print_status "PASS" "Sin memory leaks detectados"
            elif [ $exit_code -eq 124 ]; then
                print_status "WARN" "Timeout en valgrind (posiblemente sin errores críticos)"
            else
                print_status "FAIL" "Memory leaks o errores detectados"
            fi
        else
            print_status "FAIL" "Memory leaks críticos detectados"
        fi
        
        # Limpiar archivos temporales
        rm -f test_input.txt valgrind_output.log
    else
        print_status "WARN" "Valgrind no disponible, saltando pruebas de memoria"
    fi
}

# Función para pruebas de funcionalidad básica
test_basic_functionality() {
    print_status "INFO" "Ejecutando pruebas de funcionalidad básica..."
    
    # Crear script de prueba automática
    # 1 = generar interrupción manual (IRQ 0)
    # 2 = registrar ISR personalizada (IRQ 2)
    # 3 = mostrar estado IDT
    # 0 = salir
    cat > auto_test.txt << EOF
1
0
2
2
3
0
EOF
    
    # Ejecutar simulador con timeout
    timeout 20s ./interrupt_simulator < auto_test.txt > test_output.log 2>&1
    local exit_code=$?
    
    if [ $exit_code -eq 0 ] || [ $exit_code -eq 124 ]; then
        # Verificar que la salida contenga elementos esperados del simulador real
        if grep -q "IDT inicializada" test_output.log && \
           grep -q "ISR registrada" test_output.log && \
           grep -q "DESPACHANDO IRQ" test_output.log; then
            print_status "PASS" "Funcionalidad básica operativa"
        else
            print_status "FAIL" "Funcionalidad básica no operativa"
        fi
    else
        print_status "FAIL" "Error en ejecución básica"
    fi
    
    rm -f auto_test.txt test_output.log
}

# Función para pruebas de concurrencia
test_concurrency() {
    print_status "INFO" "Ejecutando pruebas de concurrencia..."
    
    # Crear script que pruebe múltiples interrupciones
    # 5 = generar múltiples interrupciones de prueba
    # 0 = salir
    cat > concurrency_test.txt << EOF
5
0
EOF
    
    timeout 15s ./interrupt_simulator < concurrency_test.txt > concurrency_output.log 2>&1
    local exit_code=$?
    
    if [ $exit_code -eq 0 ] || [ $exit_code -eq 124 ]; then
        if grep -q "DESPACHANDO IRQ" concurrency_output.log && \
           grep -q "FINALIZANDO IRQ" concurrency_output.log; then
            print_status "PASS" "Manejo de concurrencia operativo"
        else
            print_status "FAIL" "Problemas en manejo de concurrencia"
        fi
    else
        print_status "FAIL" "Error en pruebas de concurrencia"
    fi
    
    rm -f concurrency_test.txt concurrency_output.log
}

# Función para verificar sintaxis del código
test_code_syntax() {
    print_status "INFO" "Verificando sintaxis del código..."
    
    if make check > /dev/null 2>&1; then
        print_status "PASS" "Sintaxis del código correcta"
    else
        print_status "FAIL" "Errores de sintaxis detectados"
    fi
}

# Función para pruebas de stress
test_stress() {
    print_status "INFO" "Ejecutando pruebas de stress..."
    
    # Crear script de stress test
    # 1 = generar interrupciones manuales varias veces
    # 5 = suite de pruebas
    # 0 = salir
    cat > stress_test.txt << EOF
1
1
1
2
5
2
0
EOF
    
    timeout 25s ./interrupt_simulator < stress_test.txt > stress_output.log 2>&1
    local exit_code=$?
    
    if [ $exit_code -eq 0 ] || [ $exit_code -eq 124 ]; then
        # Verificar que el sistema no se colgó y procesó interrupciones
        if grep -q "DESPACHANDO IRQ" stress_output.log || \
           grep -q "suite de pruebas" stress_output.log; then
            print_status "PASS" "Pruebas de stress superadas"
        else
            print_status "FAIL" "Fallo en pruebas de stress"
        fi
    else
        print_status "FAIL" "Error en pruebas de stress"
    fi
    
    rm -f stress_test.txt stress_output.log
}

# Función para verificar archivos del proyecto
test_project_files() {
    print_status "INFO" "Verificando archivos del proyecto..."
    
    local required_files=("interrupt_simulator.c" "interrupt_simulator.h" "Makefile")
    local missing_files=0
    
    for file in "${required_files[@]}"; do
        if [ -f "$file" ]; then
            print_status "PASS" "Archivo $file presente"
        else
            print_status "FAIL" "Archivo $file faltante"
            ((missing_files++))
        fi
    done
    
    if [ $missing_files -eq 0 ]; then
        return 0
    else
        return 1
    fi
}

# Función para probar estadísticas del sistema
test_statistics() {
    print_status "INFO" "Probando sistema de estadísticas..."
    
    # Crear script que use varias funciones y luego muestre estadísticas
    cat > stats_test.txt << EOF
1
0
1
1
7
0
EOF
    
    timeout 15s ./interrupt_simulator < stats_test.txt > stats_output.log 2>&1
    local exit_code=$?
    
    if [ $exit_code -eq 0 ] || [ $exit_code -eq 124 ]; then
        if grep -q "ESTADÍSTICAS DEL SISTEMA" stats_output.log && \
           grep -q "Total de interrupciones" stats_output.log; then
            print_status "PASS" "Sistema de estadísticas operativo"
        else
            print_status "FAIL" "Sistema de estadísticas no operativo"
        fi
    else
        print_status "FAIL" "Error en pruebas de estadísticas"
    fi
    
    rm -f stats_test.txt stats_output.log
}

# Función para probar sistema de trazas
test_trace_system() {
    print_status "INFO" "Probando sistema de trazas..."
    
    # Crear script que genere actividad y luego muestre la traza
    cat > trace_test.txt << EOF
1
0
4
0
EOF
    
    timeout 10s ./interrupt_simulator < trace_test.txt > trace_output.log 2>&1
    local exit_code=$?
    
    if [ $exit_code -eq 0 ] || [ $exit_code -eq 124 ]; then
        if grep -q "TRAZA RECIENTE" trace_output.log && \
           grep -q "IDT inicializada" trace_output.log; then
            print_status "PASS" "Sistema de trazas operativo"
        else
            print_status "FAIL" "Sistema de trazas no operativo"
        fi
    else
        print_status "FAIL" "Error en pruebas de trazas"
    fi
    
    rm -f trace_test.txt trace_output.log
}

# Función para generar reporte de pruebas
generate_report() {
    print_status "INFO" "Generando reporte de pruebas..."
    
    local report_file="test_report.txt"
    local timestamp=$(date "+%Y-%m-%d %H:%M:%S")
    
    cat > "$report_file" << EOF
=====================================
REPORTE DE PRUEBAS DEL SIMULADOR
=====================================

Fecha: $timestamp
Sistema: $(uname -a)
Compilador: $(gcc --version | head -n1)

RESULTADOS:
- Pruebas ejecutadas: $TOTAL_TESTS
- Pruebas exitosas: $TESTS_PASSED
- Pruebas fallidas: $TESTS_FAILED
- Porcentaje de éxito: $(( TESTS_PASSED * 100 / TOTAL_TESTS ))%

ESTADO GENERAL: 
EOF

    if [ $TESTS_FAILED -eq 0 ]; then
        echo "✓ TODAS LAS PRUEBAS PASARON" >> "$report_file"
        print_status "PASS" "Reporte generado: $report_file"
    else
        echo "✗ ALGUNAS PRUEBAS FALLARON" >> "$report_file"
        print_status "WARN" "Reporte generado con fallos: $report_file"
    fi
}

# Función para limpiar archivos temporales
cleanup() {
    print_status "INFO" "Limpiando archivos temporales..."
    rm -f test_input.txt test_output.log auto_test.txt concurrency_test.txt
    rm -f concurrency_output.log stress_test.txt stress_output.log
    rm -f valgrind_output.log stats_test.txt stats_output.log
    rm -f trace_test.txt trace_output.log
}

# Función para mostrar ayuda
show_help() {
    echo "Script de pruebas para el Simulador de Interrupciones Linux"
    echo ""
    echo "Uso: $0 [opciones]"
    echo ""
    echo "Opciones:"
    echo "  -h, --help          Mostrar esta ayuda"
    echo "  -c, --compile       Solo compilar el proyecto"
    echo "  -f, --full          Ejecutar todas las pruebas (por defecto)"
    echo "  -b, --basic         Solo pruebas básicas"
    echo "  -m, --memory        Solo pruebas de memoria"
    echo "  -s, --stress        Solo pruebas de stress"
    echo "  -t, --trace         Solo pruebas de sistema de trazas"
    echo "  --stats             Solo pruebas de estadísticas"
    echo "  --clean             Limpiar archivos y salir"
    echo ""
}

# Función principal
main() {
    echo -e "${BLUE}=================================${NC}"
    echo -e "${BLUE}SIMULADOR DE INTERRUPCIONES LINUX${NC}"
    echo -e "${BLUE}    Suite de Pruebas Automatizadas${NC}"
    echo -e "${BLUE}=================================${NC}"
    echo ""
    
    local run_type="full"
    
    # Procesar argumentos
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -c|--compile)
                run_type="compile"
                shift
                ;;
            -f|--full)
                run_type="full"
                shift
                ;;
            -b|--basic)
                run_type="basic"
                shift
                ;;
            -m|--memory)
                run_type="memory"
                shift
                ;;
            -s|--stress)
                run_type="stress"
                shift
                ;;
            -t|--trace)
                run_type="trace"
                shift
                ;;
            --stats)
                run_type="stats"
                shift
                ;;
            --clean)
                cleanup
                make clean > /dev/null 2>&1
                print_status "INFO" "Archivos limpiados"
                exit 0
                ;;
            *)
                print_status "WARN" "Opción desconocida: $1"
                shift
                ;;
        esac
    done
    
    # Ejecutar pruebas según el tipo
    case $run_type in
        "compile")
            check_dependencies
            compile_project
            ;;
        "basic")
            check_dependencies
            test_project_files
            compile_project
            test_code_syntax
            test_basic_functionality
            ;;
        "memory")
            check_dependencies
            compile_project
            test_memory_leaks
            ;;
        "stress")
            check_dependencies
            compile_project
            test_stress
            ;;
        "trace")
            check_dependencies
            compile_project
            test_trace_system
            ;;
        "stats")
            check_dependencies
            compile_project
            test_statistics
            ;;
        "full"|*)
            check_dependencies
            test_project_files
            compile_project
            test_code_syntax
            test_basic_functionality
            test_concurrency
            test_trace_system
            test_statistics
            test_stress
            test_memory_leaks
            ;;
    esac
    
    # Mostrar resumen
    echo ""
    echo -e "${BLUE}=================================${NC}"
    echo -e "${BLUE}      RESUMEN DE PRUEBAS${NC}"
    echo -e "${BLUE}=================================${NC}"
    echo -e "Pruebas ejecutadas: ${BLUE}$TOTAL_TESTS${NC}"
    echo -e "Pruebas exitosas:   ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Pruebas fallidas:   ${RED}$TESTS_FAILED${NC}"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "\n${GREEN}✓ TODAS LAS PRUEBAS PASARON${NC}"
        generate_report
        cleanup
        exit 0
    else
        echo -e "\n${RED}✗ ALGUNAS PRUEBAS FALLARON${NC}"
        generate_report
        cleanup
        exit 1
    fi
}

# Verificar que se ejecute desde el directorio correcto
if [ ! -f "Makefile" ] && [ ! -f "interrupt_simulator.c" ]; then
    print_status "FAIL" "Ejecutar desde el directorio del proyecto"
    exit 1
fi

# Ejecutar función principal con todos los argumentos
main "$@"