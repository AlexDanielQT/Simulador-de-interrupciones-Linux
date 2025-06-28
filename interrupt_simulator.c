#define _GNU_SOURCE
#include "interrupt_simulator.h"

// Tabla de Descriptores de Interrupción (IDT)
irq_descriptor_t idt[MAX_INTERRUPTS];

// Sistema de trazabilidad
trace_entry_t trace_log[MAX_TRACE_LINES];
int trace_index = 0;

// Variables globales del sistema
int system_running = 1;
int timer_counter = 0;
pthread_t timer_thread;
pthread_mutex_t idt_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t trace_mutex = PTHREAD_MUTEX_INITIALIZER;
system_stats_t stats;

// Variables globales adicionales
log_level_t current_log_level = LOG_LEVEL_USER_ONLY;  // Por defecto, solo acciones del usuario
int show_timer_logs = 0;  // Timer logs ocultos por defecto


// Función para obtener timestamp
void get_timestamp(char *buffer, size_t size) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, size, "%H:%M:%S", timeinfo);
}

// Función para agregar entrada a la traza (thread-safe)
void add_trace(const char *event) {
    pthread_mutex_lock(&trace_mutex);
    get_timestamp(trace_log[trace_index].timestamp, sizeof(trace_log[trace_index].timestamp));
    strncpy(trace_log[trace_index].event, event, sizeof(trace_log[trace_index].event) - 1);
    trace_log[trace_index].event[sizeof(trace_log[trace_index].event) - 1] = '\0';
    trace_log[trace_index].irq_num = -1;
    trace_index = (trace_index + 1) % MAX_TRACE_LINES;
    pthread_mutex_unlock(&trace_mutex);
    
    printf("[%s] %s\n", trace_log[(trace_index - 1 + MAX_TRACE_LINES) % MAX_TRACE_LINES].timestamp, event);
    fflush(stdout);
}

// Función para agregar entrada a la traza con IRQ específico (thread-safe)
void add_trace_with_irq(const char *event, int irq_num) {
    pthread_mutex_lock(&trace_mutex);
    get_timestamp(trace_log[trace_index].timestamp, sizeof(trace_log[trace_index].timestamp));
    strncpy(trace_log[trace_index].event, event, sizeof(trace_log[trace_index].event) - 1);
    trace_log[trace_index].event[sizeof(trace_log[trace_index].event) - 1] = '\0';
    trace_log[trace_index].irq_num = irq_num;
    trace_index = (trace_index + 1) % MAX_TRACE_LINES;
    pthread_mutex_unlock(&trace_mutex);
    
    printf("[%s] %s\n", trace_log[(trace_index - 1 + MAX_TRACE_LINES) % MAX_TRACE_LINES].timestamp, event);
    fflush(stdout);
}

// Función para logging silencioso (solo guarda en traza, no imprime)
void add_trace_silent(const char *event) {
    pthread_mutex_lock(&trace_mutex);
    get_timestamp(trace_log[trace_index].timestamp, sizeof(trace_log[trace_index].timestamp));
    strncpy(trace_log[trace_index].event, event, sizeof(trace_log[trace_index].event) - 1);
    trace_log[trace_index].event[sizeof(trace_log[trace_index].event) - 1] = '\0';
    trace_log[trace_index].irq_num = -1;
    trace_index = (trace_index + 1) % MAX_TRACE_LINES;
    pthread_mutex_unlock(&trace_mutex);
    // NO imprime nada
}

void add_trace_with_irq_silent(const char *event, int irq_num) {
    pthread_mutex_lock(&trace_mutex);
    get_timestamp(trace_log[trace_index].timestamp, sizeof(trace_log[trace_index].timestamp));
    strncpy(trace_log[trace_index].event, event, sizeof(trace_log[trace_index].event) - 1);
    trace_log[trace_index].event[sizeof(trace_log[trace_index].event) - 1] = '\0';
    trace_log[trace_index].irq_num = irq_num;
    trace_index = (trace_index + 1) % MAX_TRACE_LINES;
    pthread_mutex_unlock(&trace_mutex);
    // NO imprime nada
}

// Función para controlar el nivel de logging
void set_log_level(log_level_t level) {
    current_log_level = level;
    const char* level_names[] = {"SILENCIOSO", "SOLO USUARIO", "VERBOSE"};
    printf("Nivel de logging cambiado a: %s\n", level_names[level]);
}

void toggle_timer_logs(void) {
    show_timer_logs = !show_timer_logs;
    printf("Logs del timer: %s\n", show_timer_logs ? "HABILITADOS" : "DESHABILITADOS");
}

// Función de logging inteligente
void add_trace_smart(const char *event, int irq_num, int is_timer_related) {
    // Siempre guardar en la traza para el historial
    if (irq_num >= 0) {
        add_trace_with_irq_silent(event, irq_num);
    } else {
        add_trace_silent(event);
    }
    
    // Decidir si mostrar en pantalla
    int should_print = 0;
    
    switch (current_log_level) {
        case LOG_LEVEL_SILENT:
            should_print = 0;
            break;
            
        case LOG_LEVEL_USER_ONLY:
            // Solo mostrar si no es del timer, o si los logs del timer están habilitados
            if (is_timer_related) {
                should_print = show_timer_logs;
            } else {
                should_print = 1;
            }
            break;
            
        case LOG_LEVEL_VERBOSE:
            should_print = 1;
            break;
    }
    
    if (should_print) {
        if (irq_num >= 0) {
            printf("[%s] [IRQ%d] %s\n", 
                   trace_log[(trace_index - 1 + MAX_TRACE_LINES) % MAX_TRACE_LINES].timestamp,
                   irq_num, event);
        } else {
            printf("[%s] %s\n", 
                   trace_log[(trace_index - 1 + MAX_TRACE_LINES) % MAX_TRACE_LINES].timestamp,
                   event);
        }
        fflush(stdout);
    }
}

// Validación de número de IRQ
int validate_irq_num(int irq_num) {
    return IS_VALID_IRQ(irq_num) ? SUCCESS : ERROR_INVALID_IRQ;
}

// Verificar si IRQ está disponible
int is_irq_available(int irq_num) {
    if (!IS_VALID_IRQ(irq_num)) return 0;
    LOCK_IDT();
    int available = (idt[irq_num].state == IRQ_STATE_FREE);
    UNLOCK_IDT();
    return available;
}

// Obtener string del estado del IRQ
const char* get_irq_state_string(irq_state_t state) {
    switch (state) {
        case IRQ_STATE_FREE: return "LIBRE";
        case IRQ_STATE_REGISTERED: return "REGISTRADO";
        case IRQ_STATE_EXECUTING: return "EJECUTANDO";
        default: return "DESCONOCIDO";
    }
}

// Inicialización de la IDT
void init_idt() {
    LOCK_IDT();
    for (int i = 0; i < MAX_INTERRUPTS; i++) {
        idt[i].isr = NULL;
        idt[i].state = IRQ_STATE_FREE;
        idt[i].call_count = 0;
        idt[i].last_call = 0;
        idt[i].total_execution_time = 0;
        snprintf(idt[i].description, sizeof(idt[i].description), "IRQ %d - No asignado", i);
    }
    UNLOCK_IDT();
    add_trace("IDT inicializada con éxito");
}

// Inicialización de estadísticas del sistema
void init_system_stats() {
    memset(&stats, 0, sizeof(system_stats_t));
    stats.system_start_time = time(NULL);
}

// Actualizar estadísticas (thread-safe)
void update_stats(int irq_num, unsigned long execution_time) {
    static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_mutex_lock(&stats_mutex);
    stats.total_interrupts++;
    
    if (irq_num == IRQ_TIMER) {
        stats.timer_interrupts++;
    } else if (irq_num == IRQ_KEYBOARD) {
        stats.keyboard_interrupts++;
    } else {
        stats.custom_interrupts++;
    }
    
    // Calcular tiempo promedio de respuesta
    if (stats.total_interrupts > 0) {
        stats.average_response_time = 
            (stats.average_response_time * (stats.total_interrupts - 1) + execution_time) / 
            stats.total_interrupts;
    }
    pthread_mutex_unlock(&stats_mutex);
}

// Registro de ISR en la IDT
int register_isr(int irq_num, void (*isr_function)(int), const char *description) {
    if (validate_irq_num(irq_num) != SUCCESS) {
        add_trace("Error: Número de IRQ fuera de rango");
        return ERROR_INVALID_IRQ;
    }
    
    LOCK_IDT();
    
    if (idt[irq_num].state == IRQ_STATE_EXECUTING) {
        UNLOCK_IDT();
        add_trace("Advertencia: No se puede registrar ISR mientras se ejecuta");
        return ERROR_ISR_EXECUTING;
    }
    
    idt[irq_num].isr = isr_function;
    idt[irq_num].state = IRQ_STATE_REGISTERED;
    idt[irq_num].call_count = 0;
    idt[irq_num].total_execution_time = 0;
    strncpy(idt[irq_num].description, description, sizeof(idt[irq_num].description) - 1);
    idt[irq_num].description[sizeof(idt[irq_num].description) - 1] = '\0';
    
    UNLOCK_IDT();
    
    char trace_msg[MAX_TRACE_MSG_LEN];
    snprintf(trace_msg, sizeof(trace_msg), "ISR registrada para IRQ %d: %s", irq_num, description);
    add_trace_with_irq(trace_msg, irq_num);
    
    return SUCCESS;
}

// Desregistrar ISR
int unregister_isr(int irq_num) {
    if (validate_irq_num(irq_num) != SUCCESS) {
        add_trace("Error: Número de IRQ fuera de rango");
        return ERROR_INVALID_IRQ;
    }
    
    LOCK_IDT();
    
    if (idt[irq_num].state == IRQ_STATE_EXECUTING) {
        UNLOCK_IDT();
        add_trace("Error: No se puede desregistrar ISR mientras se ejecuta");
        return ERROR_ISR_EXECUTING;
    }
    
    idt[irq_num].isr = NULL;
    idt[irq_num].state = IRQ_STATE_FREE;
    idt[irq_num].call_count = 0;
    idt[irq_num].total_execution_time = 0;
    snprintf(idt[irq_num].description, sizeof(idt[irq_num].description), "IRQ %d - No asignado", irq_num);
    
    UNLOCK_IDT();
    
    char trace_msg[MAX_TRACE_MSG_LEN];
    snprintf(trace_msg, sizeof(trace_msg), "ISR desregistrada para IRQ %d", irq_num);
    add_trace_with_irq(trace_msg, irq_num);
    
    return SUCCESS;
}

// Despacho de interrupciones - VERSIÓN CORREGIDA
void dispatch_interrupt(int irq_num) {
    char trace_msg[MAX_TRACE_MSG_LEN];
    struct timespec start_time, end_time;
    void (*isr_function)(int) = NULL;
    int is_timer_irq = (irq_num == IRQ_TIMER);
    
    if (validate_irq_num(irq_num) != SUCCESS) {
        snprintf(trace_msg, sizeof(trace_msg), "Error: IRQ %d fuera de rango", irq_num);
        add_trace_smart(trace_msg, -1, 0);
        return;
    }
    
    LOCK_IDT();
    
    if (idt[irq_num].state != IRQ_STATE_REGISTERED || idt[irq_num].isr == NULL) {
        UNLOCK_IDT();
        snprintf(trace_msg, sizeof(trace_msg), "Error: No hay ISR registrada para IRQ %d", irq_num);
        add_trace_smart(trace_msg, irq_num, is_timer_irq);
        return;
    }
    
    idt[irq_num].state = IRQ_STATE_EXECUTING;
    idt[irq_num].call_count++;
    idt[irq_num].last_call = time(NULL);
    isr_function = idt[irq_num].isr;
    
    snprintf(trace_msg, sizeof(trace_msg), ">>> DESPACHANDO IRQ %d (%s) - Llamada #%d", 
             irq_num, idt[irq_num].description, idt[irq_num].call_count);
    
    UNLOCK_IDT();
    
    add_trace_smart(trace_msg, irq_num, is_timer_irq);
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    if (isr_function) {
        isr_function(irq_num);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    unsigned long execution_time = 
        (end_time.tv_sec - start_time.tv_sec) * 1000000 +
        (end_time.tv_nsec - start_time.tv_nsec) / 1000;
    
    LOCK_IDT();
    idt[irq_num].state = IRQ_STATE_REGISTERED;
    idt[irq_num].total_execution_time += execution_time;
    UNLOCK_IDT();
    
    update_stats(irq_num, execution_time);
    
    snprintf(trace_msg, sizeof(trace_msg), "<<< FINALIZANDO IRQ %d - Retorno al flujo principal (Tiempo: %lu μs)", 
             irq_num, execution_time);
    add_trace_smart(trace_msg, irq_num, is_timer_irq);
}

// ISR del Timer del Sistema (IRQ 0)
void timer_isr(int irq_num) {
    timer_counter++;
    char trace_msg[MAX_TRACE_MSG_LEN];
    snprintf(trace_msg, sizeof(trace_msg), "    TIMER ISR: Tick del sistema #%d", timer_counter);
    add_trace_smart(trace_msg, irq_num, 1);  // 1 = es relacionado al timer
    
    usleep(ISR_SIMULATION_DELAY_US);
}

// ISR del Teclado (IRQ 1)
void keyboard_isr(int irq_num) {
    char trace_msg[MAX_TRACE_MSG_LEN];
    snprintf(trace_msg, sizeof(trace_msg), "    KEYBOARD ISR: Procesando entrada del teclado");
    add_trace_with_irq(trace_msg, irq_num);
    
    // Simular procesamiento de tecla
    usleep(KEYBOARD_DELAY_US);
}

// ISR personalizada de ejemplo
void custom_isr(int irq_num) {
    char trace_msg[MAX_TRACE_MSG_LEN];
    snprintf(trace_msg, sizeof(trace_msg), "    CUSTOM ISR: Rutina personalizada para IRQ %d", irq_num);
    add_trace_with_irq(trace_msg, irq_num);
    
    usleep(CUSTOM_DELAY_US);
}

// ISR de error
void error_isr(int irq_num) {
    char trace_msg[MAX_TRACE_MSG_LEN];
    snprintf(trace_msg, sizeof(trace_msg), "    ERROR ISR: Manejando error en IRQ %d", irq_num);
    add_trace_with_irq(trace_msg, irq_num);
    
    usleep(50000); // 50ms
}

// Hilo del timer automático
void* timer_thread_func(void* arg) {
    (void)arg; // Evitar warning de parámetro no usado
    
    add_trace("Hilo del timer iniciado");
    
    while (system_running) {
        sleep(TIMER_INTERVAL_SEC);
        if (system_running) {
            dispatch_interrupt(IRQ_TIMER);
        }
    }
    
    add_trace("Hilo del timer finalizando");
    return NULL;
}

// Mostrar estado de la IDT
void show_idt_status() {
    printf("\n=== ESTADO ACTUAL DE LA IDT ===\n");
    printf("IRQ | Estado      | Llamadas | Tiempo Total (μs) | Descripción\n");
    printf("----|-------------|----------|-------------------|----------------------------------\n");
    
    LOCK_IDT();
    for (int i = 0; i < MAX_INTERRUPTS; i++) {
        const char* state_str = get_irq_state_string(idt[i].state);
        printf("%3d | %-11s | %8d | %17lu | %s\n", 
               i, state_str, idt[i].call_count, idt[i].total_execution_time, idt[i].description);
    }
    UNLOCK_IDT();
    printf("\n");
}

// Mostrar traza reciente
void show_recent_trace() {
    printf("\n=== TRAZA RECIENTE (últimas 10 entradas) ===\n");
    
    pthread_mutex_lock(&trace_mutex);
    int entries_to_show = (trace_index < 10) ? trace_index : 10;
    int start = (trace_index - entries_to_show + MAX_TRACE_LINES) % MAX_TRACE_LINES;
    
    for (int i = 0; i < entries_to_show; i++) {
        int idx = (start + i) % MAX_TRACE_LINES;
        if (strlen(trace_log[idx].event) > 0) {
            if (trace_log[idx].irq_num >= 0) {
                printf("[%s] [IRQ%d] %s\n", 
                       trace_log[idx].timestamp, trace_log[idx].irq_num, trace_log[idx].event);
            } else {
                printf("[%s] %s\n", trace_log[idx].timestamp, trace_log[idx].event);
            }
        }
    }
    pthread_mutex_unlock(&trace_mutex);
    printf("\n");
}

// Mostrar estadísticas del sistema
void show_system_stats() {
    printf("\n=== ESTADÍSTICAS DEL SISTEMA ===\n");
    printf("Tiempo de funcionamiento: %ld segundos\n", time(NULL) - stats.system_start_time);
    printf("Total de interrupciones: %lu\n", stats.total_interrupts);
    printf("Interrupciones de timer: %lu\n", stats.timer_interrupts);
    printf("Interrupciones de teclado: %lu\n", stats.keyboard_interrupts);
    printf("Interrupciones personalizadas: %lu\n", stats.custom_interrupts);
    printf("Tiempo promedio de respuesta: %.2f μs\n", stats.average_response_time);
    printf("\n");
}

// Mostrar ayuda
void show_help() {
    printf("\n=== AYUDA DEL SIMULADOR ===\n");
    printf("Este simulador replica el comportamiento de un manejador de interrupciones\n");
    printf("basado en el modelo usado por el kernel de Linux.\n\n");
    printf("Comandos disponibles:\n");
    printf("1. Generar interrupción manual - Dispara una IRQ específica\n");
    printf("2. Registrar ISR personalizada - Asocia una rutina a un IRQ\n");
    printf("3. Estado de IDT - Muestra el estado actual de todas las IRQs\n");
    printf("4. Traza reciente - Muestra el log de eventos recientes\n");
    printf("5. Múltiples interrupciones - Ejecuta una secuencia de prueba\n");
    printf("6. Desregistrar ISR - Remueve una ISR de un IRQ\n");
    printf("7. Estadísticas - Muestra estadísticas del sistema\n");
    printf("8. Ayuda - Muestra esta información\n");
    printf("0. Salir - Termina el simulador\n\n");
    printf("IRQs del sistema:\n");
    printf("- IRQ 0: Timer del sistema (automático cada %d segundos)\n", TIMER_INTERVAL_SEC);
    printf("- IRQ 1: Controlador de teclado\n");
    printf("- IRQ 2-15: Disponibles para ISRs personalizadas\n\n");
}

// Menú interactivo
void show_menu() {
    printf("\n=== SIMULADOR DE INTERRUPCIONES LINUX ===\n");
    printf("1. Generar interrupción manual\n");
    printf("2. Registrar ISR personalizada\n");
    printf("3. Mostrar estado de la IDT\n");
    printf("4. Mostrar traza reciente\n");
    printf("5. Generar múltiples interrupciones de prueba\n");
    printf("6. Desregistrar ISR\n");
    printf("7. Mostrar estadísticas del sistema\n");
    printf("8. Configurar logging\n");  // Nueva opción
    printf("9. Mostrar ayuda\n");
    printf("0. Salir\n");
    printf("Seleccione una opción: ");
    fflush(stdout);
}

void logging_submenu() {
    int option;
    
    while (1) {
        printf("\n=== CONFIGURACIÓN DE LOGGING ===\n");
        printf("Estado actual: ");
        
        switch (current_log_level) {
            case LOG_LEVEL_SILENT:
                printf("SILENCIOSO");
                break;
            case LOG_LEVEL_USER_ONLY:
                printf("SOLO USUARIO (Timer logs: %s)", show_timer_logs ? "ON" : "OFF");
                break;
            case LOG_LEVEL_VERBOSE:
                printf("VERBOSE");
                break;
        }
        
        printf("\n\n1. Modo silencioso (solo guardar en historial)\n");
        printf("2. Modo usuario (solo acciones del usuario)\n");
        printf("3. Modo verbose (mostrar todo)\n");
        printf("4. Toggle logs del timer (actual: %s)\n", show_timer_logs ? "ON" : "OFF");
        printf("5. Mostrar logs del timer en tiempo real por 30 segundos\n");
        printf("0. Volver al menú principal\n");
        printf("Seleccione una opción: ");
        fflush(stdout);
        
        option = get_valid_input(0, 5);
        
        switch (option) {
            case 1:
                set_log_level(LOG_LEVEL_SILENT);
                break;
            case 2:
                set_log_level(LOG_LEVEL_USER_ONLY);
                break;
            case 3:
                set_log_level(LOG_LEVEL_VERBOSE);
                break;
            case 4:
                toggle_timer_logs();
                break;
            case 5:
                printf("Mostrando logs del timer por 30 segundos...\n");
                int old_show_timer = show_timer_logs;
                log_level_t old_level = current_log_level;
                show_timer_logs = 1;
                current_log_level = LOG_LEVEL_USER_ONLY;
                sleep(30);
                show_timer_logs = old_show_timer;
                current_log_level = old_level;
                printf("Volviendo a la configuración anterior.\n");
                break;
            case 0:
                return;
        }
    }
}


// Funciones de testing
void run_interrupt_test_suite() {
    printf("Ejecutando suite de pruebas...\n");
    
    // Registrar ISRs de prueba
    register_isr(2, custom_isr, "Test ISR 1");
    register_isr(3, custom_isr, "Test ISR 2");
    
    // Generar interrupciones de prueba
    for (int i = 0; i < 3; i++) {
        dispatch_interrupt(2);
        usleep(500000); // 500ms
        dispatch_interrupt(3);
        usleep(500000);
    }
    
    printf("Suite de pruebas completada.\n");
}

void test_concurrent_interrupts() {
    printf("Probando interrupciones concurrentes...\n");
    
    // Generar múltiples interrupciones rápidamente
    for (int i = 0; i < 5; i++) {
        dispatch_interrupt(IRQ_TIMER);
        dispatch_interrupt(IRQ_KEYBOARD);
        usleep(100000); // 100ms
    }
    
    printf("Prueba de concurrencia completada.\n");
}

void test_stress_interrupts() {
    printf("Ejecutando prueba de stress...\n");
    
    for (int i = 0; i < 20; i++) {
        dispatch_interrupt(i % MAX_INTERRUPTS);
        usleep(50000); // 50ms
    }
    
    printf("Prueba de stress completada.\n");
}

// Función para limpiar entrada inválida del buffer
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Función para obtener entrada numérica válida
int get_valid_input(int min, int max) {
    int input;
    char buffer[256];
    char *endptr;
    
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("Error leyendo entrada. Intente de nuevo: ");
            continue;
        }
        
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        
        char *trimmed = buffer;
        while (*trimmed == ' ' || *trimmed == '\t') {
            trimmed++;
        }
        
        if (*trimmed == '\0') {
            printf("Entrada inválida. Ingrese un número entre %d y %d: ", min, max);
            continue;
        }
        
        input = (int)strtol(trimmed, &endptr, 10);
        
        if (*endptr != '\0') {
            printf("Entrada inválida. Ingrese un número entre %d y %d: ", min, max);
            continue;
        }
        
        if (input >= min && input <= max) {
            return input;
        }
        
        printf("Número fuera de rango. Ingrese un número entre %d y %d: ", min, max);
    }
}

// Función simple para esperar Enter
void wait_for_enter() {
    printf("\nPresione Enter para continuar...");
    fflush(stdout);
    
    char buffer[10];
    fgets(buffer, sizeof(buffer), stdin);
}

// Función principal
int main() {
    int option, irq_num;
    
    printf("Iniciando Simulador de Interrupciones Linux IDT...\n");
    fflush(stdout);
    
    // Inicializar sistema
    init_idt();
    init_system_stats();
    
    // Registrar ISRs predeterminadas
    register_isr(IRQ_TIMER, timer_isr, "Timer del Sistema");
    register_isr(IRQ_KEYBOARD, keyboard_isr, "Controlador de Teclado");
    
    // Iniciar hilo del timer
    if (pthread_create(&timer_thread, NULL, timer_thread_func, NULL) != 0) {
        add_trace("Error: No se pudo crear el hilo del timer");
        return ERROR_THREAD_CREATE;
    }
    
    add_trace("Sistema de interrupciones iniciado correctamente");
    
    // Bucle principal del menú
    while (system_running) {
        show_menu();
        
        option = get_valid_input(0, 9);
        
        printf("\n");
        
        switch (option) {
            case 1:
                printf("Ingrese el número de IRQ (0-%d): ", MAX_INTERRUPTS - 1);
                fflush(stdout);
                irq_num = get_valid_input(0, MAX_INTERRUPTS - 1);
                printf("Despachando IRQ %d...\n", irq_num);
                dispatch_interrupt(irq_num);
                wait_for_enter();
                break;
                
            case 2:
                printf("Ingrese el número de IRQ para registrar ISR personalizada (2-%d): ", MAX_INTERRUPTS - 1);
                fflush(stdout);
                irq_num = get_valid_input(2, MAX_INTERRUPTS - 1);
                char desc[MAX_DESCRIPTION_LEN];
                snprintf(desc, sizeof(desc), "ISR Personalizada %d", irq_num);
                printf("Registrando ISR para IRQ %d...\n", irq_num);
                if (register_isr(irq_num, custom_isr, desc) == SUCCESS) {
                    printf("✓ ISR registrada exitosamente para IRQ %d.\n", irq_num);
                } else {
                    printf("✗ Error al registrar ISR para IRQ %d.\n", irq_num);
                }
                wait_for_enter();
                break;
                
            case 3:
                printf("Mostrando estado actual de la IDT...\n");
                show_idt_status();
                wait_for_enter();
                break;
                
            case 4:
                printf("Mostrando traza reciente...\n");
                show_recent_trace();
                wait_for_enter();
                break;
                
            case 5:
                printf("Ejecutando suite de pruebas de interrupciones...\n");
                run_interrupt_test_suite();
                printf("✓ Suite de pruebas completada.\n");
                wait_for_enter();
                break;
                
            case 6:
                printf("Ingrese el número de IRQ a desregistrar (0-%d): ", MAX_INTERRUPTS - 1);
                fflush(stdout);
                irq_num = get_valid_input(0, MAX_INTERRUPTS - 1);
                printf("Desregistrando ISR para IRQ %d...\n", irq_num);
                if (unregister_isr(irq_num) == SUCCESS) {
                    printf("✓ ISR desregistrada exitosamente para IRQ %d.\n", irq_num);
                } else {
                    printf("✗ Error al desregistrar ISR para IRQ %d.\n", irq_num);
                }
                wait_for_enter();
                break;
                
            case 7:
                printf("Mostrando estadísticas del sistema...\n");
                show_system_stats();
                wait_for_enter();
                break;
            case 8:
                printf("Configurando sistema de logging...\n");
                logging_submenu();
                break;

            case 9:
                printf("Mostrando ayuda...\n");
                show_help();
                wait_for_enter();
                break;
                
            case 0:
                printf("Finalizando simulador...\n");
                system_running = 0;
                break;
                
            default:
                printf("Opción inválida: %d\n", option);
                printf("Por favor, seleccione una opción válida (0-8).\n");
                wait_for_enter();
                break;
        }
        
        if (system_running) {
            printf("\n");
        }
    }
    
    // Limpiar recursos
    add_trace("Finalizando sistema de interrupciones");
    
    // Esperar a que termine el hilo del timer
    if (pthread_join(timer_thread, NULL) != 0) {
        printf("Advertencia: Error al finalizar hilo del timer\n");
    }
    
    pthread_mutex_destroy(&idt_mutex);
    pthread_mutex_destroy(&trace_mutex);
    
    printf("Simulador finalizado correctamente.\n");
    return SUCCESS;
}