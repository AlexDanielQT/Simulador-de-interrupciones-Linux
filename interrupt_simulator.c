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
        snprintf(idt[i].description, sizeof(idt[i].description), 
            "IRQ %d - Vector libre en IDT", i);
    }
    UNLOCK_IDT();
    
    add_trace("🚀 KERNEL: Tabla de Descriptores de Interrupción (IDT) inicializada");
    add_trace("🎯 KERNEL: 16 vectores de interrupción disponibles para asignación");
    add_trace("🔧 HARDWARE: Controlador de interrupciones (PIC/APIC) configurado");
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
        add_trace("❌ KERNEL: Error en registro ISR - IRQ fuera de rango válido");
        return ERROR_INVALID_IRQ;
    }
    
    LOCK_IDT();
    
    if (idt[irq_num].state == IRQ_STATE_EXECUTING) {
        UNLOCK_IDT();
        add_trace("⚠️  KERNEL: Registro ISR fallido - IRQ actualmente en ejecución");
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
    snprintf(trace_msg, sizeof(trace_msg), 
        "📝 KERNEL: ISR registrada en IDT[%d] -> Handler: \"%s\"", 
        irq_num, description);
    add_trace_with_irq(trace_msg, irq_num);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "🔗 HARDWARE: IRQ %d ahora conectada al kernel - Lista para recibir señales", 
        irq_num);
    add_trace_with_irq(trace_msg, irq_num);
    
    return SUCCESS;
}

// Desregistrar ISR
int unregister_isr(int irq_num) {
    if (validate_irq_num(irq_num) != SUCCESS) {
        add_trace("❌ KERNEL: Error en desregistro ISR - IRQ fuera de rango válido");
        return ERROR_INVALID_IRQ;
    }
    
    LOCK_IDT();
    
    if (idt[irq_num].state == IRQ_STATE_EXECUTING) {
        UNLOCK_IDT();
        add_trace("⚠️  KERNEL: Desregistro ISR fallido - IRQ actualmente en ejecución");
        return ERROR_ISR_EXECUTING;
    }
    
    char old_description[MAX_DESCRIPTION_LEN];
    strncpy(old_description, idt[irq_num].description, MAX_DESCRIPTION_LEN - 1);
    old_description[MAX_DESCRIPTION_LEN - 1] = '\0';
    
    
    idt[irq_num].isr = NULL;
    idt[irq_num].state = IRQ_STATE_FREE;
    idt[irq_num].call_count = 0;
    idt[irq_num].total_execution_time = 0;
    snprintf(idt[irq_num].description, sizeof(idt[irq_num].description), 
        "IRQ %d - Disponible para asignación", irq_num);
    
    UNLOCK_IDT();
    
    char trace_msg[MAX_TRACE_MSG_LEN];
    snprintf(trace_msg, sizeof(trace_msg), 
        "🗑️  KERNEL: ISR removida de IDT[%d] - Era: \"%s\"", 
        irq_num, old_description);
    add_trace_with_irq(trace_msg, irq_num);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "🚫 HARDWARE: IRQ %d desconectada - Interrupciones no serán procesadas", 
        irq_num);
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
        snprintf(trace_msg, sizeof(trace_msg), 
            "❌ HARDWARE: IRQ %d RECHAZADA - Número fuera del rango válido (0-%d)", 
            irq_num, MAX_INTERRUPTS-1);
        add_trace_smart(trace_msg, -1, 0);
        return;
    }
    
    LOCK_IDT();
    
    // ✅ VERIFICAR ESTADO CORRECTO
    if (idt[irq_num].state != IRQ_STATE_REGISTERED || idt[irq_num].isr == NULL) {
        UNLOCK_IDT();
        snprintf(trace_msg, sizeof(trace_msg), 
            "❌ KERNEL: IRQ %d SIN HANDLER - Estado: %s", 
            irq_num, get_irq_state_string(idt[irq_num].state));
        add_trace_smart(trace_msg, irq_num, is_timer_irq);
        return;
    }
    
    // ✅ VERIFICAR SI YA SE ESTÁ EJECUTANDO (protección contra reentrancy)
    if (idt[irq_num].state == IRQ_STATE_EXECUTING) {
        UNLOCK_IDT();
        snprintf(trace_msg, sizeof(trace_msg), 
            "⚠️  KERNEL: IRQ %d ya ejecutándose - Interrupción ignorada (reentrancy)", irq_num);
        add_trace_smart(trace_msg, irq_num, is_timer_irq);
        return;
    }
    
    // Simular el proceso real de Linux
    snprintf(trace_msg, sizeof(trace_msg), 
        "🔥 HARDWARE: IRQ %d disparada - Línea de interrupción activada", irq_num);
    add_trace_smart(trace_msg, irq_num, is_timer_irq);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "🚨 CPU: Guardando contexto actual - Registros y estado del procesador");
    add_trace_smart(trace_msg, irq_num, is_timer_irq);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "🔍 KERNEL: Consultando IDT[%d] - Vector de interrupción encontrado", irq_num);
    add_trace_smart(trace_msg, irq_num, is_timer_irq);
    
    // ✅ CAMBIAR ESTADO A EJECUTANDO
    idt[irq_num].state = IRQ_STATE_EXECUTING;
    idt[irq_num].call_count++;
    idt[irq_num].last_call = time(NULL);
    isr_function = idt[irq_num].isr;
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "⚡ KERNEL: Ejecutando ISR \"%s\" - Llamada #%d [Modo Kernel]", 
        idt[irq_num].description, idt[irq_num].call_count);
    
    UNLOCK_IDT();
    add_trace_smart(trace_msg, irq_num, is_timer_irq);
    
    // ✅ EJECUTAR LA ISR
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    if (isr_function) {
        isr_function(irq_num);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    unsigned long execution_time = 
        (end_time.tv_sec - start_time.tv_sec) * 1000000 +
        (end_time.tv_nsec - start_time.tv_nsec) / 1000;
    
    // ✅ RESTAURAR ESTADO A REGISTRADO
    LOCK_IDT();
    idt[irq_num].state = IRQ_STATE_REGISTERED;  // ✅ VOLVER A REGISTRADO
    idt[irq_num].total_execution_time += execution_time;
    UNLOCK_IDT();
    
    update_stats(irq_num, execution_time);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "🔄 CPU: Restaurando contexto - Volviendo al proceso interrumpido (%lu μs)", 
        execution_time);
    add_trace_smart(trace_msg, irq_num, is_timer_irq);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "✅ KERNEL: IRQ %d procesada - Sistema listo para nuevas interrupciones", irq_num);
    add_trace_smart(trace_msg, irq_num, is_timer_irq);
}



// ISR del Timer del Sistema (IRQ 0)
void timer_isr(int irq_num) {
    timer_counter++;
    char trace_msg[MAX_TRACE_MSG_LEN];
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "    ⏰ TIMER_ISR: Tick del sistema #%d - Actualizando jiffies del kernel", 
        timer_counter);
    add_trace_smart(trace_msg, irq_num, 1);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "    📊 SCHEDULER: Verificando quantum de procesos - Time slice check");
    add_trace_smart(trace_msg, irq_num, 1);
    
    usleep(ISR_SIMULATION_DELAY_US);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "    🔄 TIMER_ISR: Completada - Sistema de tiempo actualizado");
    add_trace_smart(trace_msg, irq_num, 1);
}

// ISR del Teclado (IRQ 1)
void keyboard_isr(int irq_num) {
    char trace_msg[MAX_TRACE_MSG_LEN];
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "    ⌨️  KEYBOARD_ISR: Leyendo scancode del controlador 8042");
    add_trace_with_irq(trace_msg, irq_num);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "    🔤 INPUT_LAYER: Traduciendo scancode a keycode");
    add_trace_with_irq(trace_msg, irq_num);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "    📤 EVENT_QUEUE: Enviando evento de teclado a /dev/input/eventX");
    add_trace_with_irq(trace_msg, irq_num);
    
    usleep(KEYBOARD_DELAY_US);
}

// ISR personalizada de ejemplo
void custom_isr(int irq_num) {
    char trace_msg[MAX_TRACE_MSG_LEN];
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "    🔧 CUSTOM_ISR: Procesando interrupción de dispositivo personalizado");
    add_trace_with_irq(trace_msg, irq_num);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "    💾 DEVICE_DRIVER: Intercambiando datos con hardware específico");
    add_trace_with_irq(trace_msg, irq_num);
    
    snprintf(trace_msg, sizeof(trace_msg), 
        "    ✅ CUSTOM_ISR: Operación completada - Hardware listo para nuevas operaciones");
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
    (void)arg;
    
    add_trace("🕐 HARDWARE: Hilo del timer PIT (Programmable Interval Timer) iniciado");
    add_trace("⚙️  TIMER: Configurado para generar IRQ0 cada 3 segundos");
    
    while (system_running) {
        sleep(TIMER_INTERVAL_SEC);
        if (system_running) {
            char trace_msg[MAX_TRACE_MSG_LEN];
            snprintf(trace_msg, sizeof(trace_msg), 
                "⏲️  HARDWARE: Timer PIT disparando IRQ0 - Señal de reloj del sistema");
            add_trace_smart(trace_msg, -1, 1);
            
            dispatch_interrupt(IRQ_TIMER);
        }
    }
    
    add_trace("🛑 HARDWARE: Timer PIT detenido - Hilo del timer finalizando");
    return NULL;
}

void show_idt_status() {
    printf("\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                ESTADO ACTUAL DE LA IDT (Solo IRQs utilizadas)              ║\n");
    printf("║                       Simulando: /proc/interrupts                          ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║ IRQ │    Estado     │ Llamadas │ Tiempo Total (μs) │ Handler Descripción    ║\n");
    printf("╠═════╪═══════════════╪══════════╪═══════════════════╪════════════════════════╣\n");

    int usados = 0;

    LOCK_IDT();
    for (int i = 0; i < MAX_INTERRUPTS; i++) {
        if (idt[i].call_count == 0)
            continue; // Mostrar solo si fue usada en esta ejecución

        const char* state_str = get_irq_state_string(idt[i].state);
        const char* icon = "";

        switch (idt[i].state) {
            case IRQ_STATE_FREE:       icon = "⚪"; break;
            case IRQ_STATE_REGISTERED: icon = "🟢"; break;
            case IRQ_STATE_EXECUTING:  icon = "🔴"; break;
        }

        printf("║ %s%2d │ %-12s │ %8d │ %17lu │ %-21s ║\n", 
               icon, i, state_str, idt[i].call_count, 
               idt[i].total_execution_time, idt[i].description);
        usados++;
    }
    UNLOCK_IDT();

    if (usados == 0) {
        printf("║                             ⚠️  Ninguna IRQ activa                            ║\n");
    }

    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
    printf("🟢 = Registrada y lista  🔴 = Ejecutándose  ⚪ = Disponible\n");
}


// Mostrar traza reciente
void show_recent_trace() {
    printf("\n=== TRAZA RECIENTE ===\n");
    
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

// Función auxiliar mejorada para detectar trazas del timer
int is_timer_related_trace(const trace_entry_t *entry) {
    // Verificar por IRQ número
    if (entry->irq_num == IRQ_TIMER) {
        return 1;
    }
    
    // Verificar por contenido del mensaje (patrones más completos)
    const char* timer_patterns[] = {
        "TIMER", "Timer", "timer",
        "TICK", "Tick", "tick",
        "Hilo del timer", "timer_thread",
        "DESPACHANDO IRQ 0", "FINALIZANDO IRQ 0",
        ">>> DESPACHANDO IRQ 0", "<<< FINALIZANDO IRQ 0",
        "iniciado", "finalizando"  // Para mensajes del hilo del timer
    };
    
    int num_patterns = sizeof(timer_patterns) / sizeof(timer_patterns[0]);
    
    for (int i = 0; i < num_patterns; i++) {
        if (strstr(entry->event, timer_patterns[i]) != NULL) {
            return 1;
        }
    }
    
    return 0;
}

// Función corregida para mostrar última traza (excluyendo timer)
void show_last_trace() {
    printf("\n=== ÚLTIMA TRAZA NO-TIMER ===\n");
    pthread_mutex_lock(&trace_mutex);
    
    int found = 0;
    int entries_checked = 0;
    
    // Calcular el número real de entradas en el buffer circular
    int total_entries = 0;
    int start_search = trace_index;
    
    // Si trace_index es 0, empezar desde el final del buffer
    if (trace_index == 0) {
        start_search = MAX_TRACE_LINES;
    }
    
    // Buscar hacia atrás desde la entrada más reciente
    for (int i = 0; i < MAX_TRACE_LINES && !found; i++) {
        int idx = (start_search - 1 - i + MAX_TRACE_LINES) % MAX_TRACE_LINES;
        entries_checked++;
        
        // Solo procesar entradas que tienen contenido válido
        if (strlen(trace_log[idx].event) > 0) {
            total_entries++;
            
            // Verificar si es traza del timer usando la función auxiliar
            if (!is_timer_related_trace(&trace_log[idx])) {
                printf("Entrada encontrada (posición %d desde el final):\n", i + 1);
                
                if (trace_log[idx].irq_num >= 0) {
                    printf("[%s] [IRQ%d] %s\n",
                           trace_log[idx].timestamp, 
                           trace_log[idx].irq_num, 
                           trace_log[idx].event);
                } else {
                    printf("[%s] %s\n", 
                           trace_log[idx].timestamp, 
                           trace_log[idx].event);
                }
                found = 1;
            }
        }
    }
    
    if (!found) {
        if (total_entries == 0) {
            printf("El log de trazas está vacío\n");
        } else {
            printf("No se encontraron trazas que no sean del timer\n");
            printf("Total de entradas válidas revisadas: %d\n", total_entries);
            printf("Todas las trazas recientes parecen ser del timer del sistema\n");
        }
    }
    
    pthread_mutex_unlock(&trace_mutex);
    printf("\n");
}

// Función adicional para mostrar las últimas N trazas no-timer
void show_last_n_non_timer_traces(int n) {
    printf("\n=== ÚLTIMAS %d TRAZAS NO-TIMER ===\n", n);
    pthread_mutex_lock(&trace_mutex);
    
    int found_count = 0;
    int entries_checked = 0;
    int start_search = trace_index;
    
    if (trace_index == 0) {
        start_search = MAX_TRACE_LINES;
    }
    
    printf("Buscando las últimas %d trazas que no sean del timer...\n\n", n);
    
    // Buscar hacia atrás desde la entrada más reciente
    for (int i = 0; i < MAX_TRACE_LINES && found_count < n; i++) {
        int idx = (start_search - 1 - i + MAX_TRACE_LINES) % MAX_TRACE_LINES;
        entries_checked++;
        
        // Solo procesar entradas que tienen contenido válido
        if (strlen(trace_log[idx].event) > 0) {
            // Verificar si es traza del timer
            if (!is_timer_related_trace(&trace_log[idx])) {
                found_count++;
                printf("%d. ", found_count);
                
                if (trace_log[idx].irq_num >= 0) {
                    printf("[%s] [IRQ%d] %s\n",
                           trace_log[idx].timestamp, 
                           trace_log[idx].irq_num, 
                           trace_log[idx].event);
                } else {
                    printf("[%s] %s\n", 
                           trace_log[idx].timestamp, 
                           trace_log[idx].event);
                }
            }
        }
    }
    
    if (found_count == 0) {
        printf("No se encontraron trazas que no sean del timer\n");
        printf("Entradas totales revisadas: %d\n", entries_checked);
    } else if (found_count < n) {
        printf("\nSolo se encontraron %d trazas no-timer (de %d solicitadas)\n", found_count, n);
    }
    
    pthread_mutex_unlock(&trace_mutex);
    printf("\n");
}

// Función mejorada para debug del buffer de trazas
void debug_trace_buffer() {
    printf("\n=== DEBUG DEL BUFFER DE TRAZAS ===\n");
    pthread_mutex_lock(&trace_mutex);
    
    printf("trace_index actual: %d\n", trace_index);
    printf("MAX_TRACE_LINES: %d\n\n", MAX_TRACE_LINES);
    
    int valid_entries = 0;
    int timer_entries = 0;
    int non_timer_entries = 0;
    
    printf("Análisis del contenido del buffer:\n");
    for (int i = 0; i < MAX_TRACE_LINES; i++) {
        if (strlen(trace_log[i].event) > 0) {
            valid_entries++;
            if (is_timer_related_trace(&trace_log[i])) {
                timer_entries++;
            } else {
                non_timer_entries++;
            }
        }
    }
    
    printf("Entradas válidas: %d\n", valid_entries);
    printf("Entradas del timer: %d\n", timer_entries);
    printf("Entradas no-timer: %d\n", non_timer_entries);
    
    // Mostrar las últimas 5 entradas con su clasificación
    printf("\nÚltimas 5 entradas (con clasificación):\n");
    int start = (trace_index - 5 + MAX_TRACE_LINES) % MAX_TRACE_LINES;
    for (int i = 0; i < 5; i++) {
        int idx = (start + i) % MAX_TRACE_LINES;
        if (strlen(trace_log[idx].event) > 0) {
            const char* type = is_timer_related_trace(&trace_log[idx]) ? "[TIMER]" : "[USER]";
            printf("%s [%s] %s\n", type, trace_log[idx].timestamp, trace_log[idx].event);
        }
    }
    
    pthread_mutex_unlock(&trace_mutex);
    printf("\n");
}

// Mostrar estadísticas del sistema
void show_system_stats() {
    printf("\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                        ESTADÍSTICAS DEL KERNEL                              ║\n");
    printf("║                     Simulando: /proc/stat y /proc/uptime                    ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
    
    time_t uptime = time(NULL) - stats.system_start_time;
    int hours = uptime / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;
    
    printf("║ 🕐 Uptime del sistema:           %02d:%02d:%02d (%ld segundos)        ║\n", 
           hours, minutes, seconds, uptime);
    printf("║ 📊 Total de interrupciones:      %-10lu                           ║\n", 
           stats.total_interrupts);
    printf("║ ⏰ Interrupciones de timer:       %-10lu (IRQ 0)                  ║\n", 
           stats.timer_interrupts);
    printf("║ ⌨️  Interrupciones de teclado:     %-10lu (IRQ 1)                  ║\n", 
           stats.keyboard_interrupts);
    printf("║ 🔧 Interrupciones personalizadas: %-10lu (IRQ 2-15)               ║\n", 
           stats.custom_interrupts);
    printf("║ ⚡ Tiempo promedio de ISR:        %.2f μs                          ║\n", 
           stats.average_response_time);
    
    // Calcular estadísticas adicionales
    float irq_rate = uptime > 0 ? (float)stats.total_interrupts / uptime : 0;
    printf("║ 📈 Tasa de interrupciones:        %.2f IRQs/segundo                ║\n", irq_rate);
    
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
}

// Mostrar ayuda
void show_help() {
    printf("\n╔════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    SIMULADOR DE INTERRUPCIONES LINUX                           ║\n");
    printf("║                        Basado en la arquitectura x86                           ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║                                                                                ║\n");
    printf("║ Este simulador replica el comportamiento del sistema de manejo de              ║\n");
    printf("║ interrupciones del kernel de Linux, incluyendo:                                ║\n");
    printf("║                                                                                ║\n");
    printf("║ 🔧 IDT (Interrupt Descriptor Table) - Tabla de vectores                        ║\n");
    printf("║ ⚡ ISR (Interrupt Service Routines) - Manejadores de interrupción              ║\n");
    printf("║ 🕐 Timer PIT - Generador automático de IRQ0 cada 3 segundos                    ║\n");
    printf("║ ⌨️  Controlador de teclado - Simulación de entrada de usuario                  ║\n");
    printf("║ 📊 Sistema de trazabilidad - Log detallado de eventos                          ║\n");
    printf("║                                                                                ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║                            MAPA DE INTERRUPCIONES                              ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║ IRQ 0  - Timer del sistema (PIT) - Automático cada 3 segundos                  ║\n");
    printf("║ IRQ 1  - Controlador de teclado (8042) - Manual                                ║\n");
    printf("║ IRQ 2  - Cascada del PIC secundario (reservada)                                ║\n");
    printf("║ IRQ 3-15 - Dispositivos personalizados - Disponibles                           ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║                              FLUJO DE INTERRUPCIÓN                             ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║ 1. 🔥 Hardware genera señal de interrupción                                    ║\n");
    printf("║ 2. 🚨 CPU guarda contexto actual (registros)                                   ║\n");
    printf("║ 3. 🔍 Kernel consulta IDT por el vector correspondiente                        ║\n");
    printf("║ 4. ⚡ Se ejecuta la ISR en modo kernel                                         ║\n");
    printf("║ 5. 🔄 CPU restaura contexto y continúa ejecución                               ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════════╝\n");
}

// Función adicional para debugging - mostrar todos los estados
void debug_all_irq_states() {
    printf("\n=== DEBUG: TODOS LOS ESTADOS DE IRQ ===\n");
    
    LOCK_IDT();
    
    int free_count = 0;
    int registered_count = 0;
    int executing_count = 0;
    
    for (int i = 0; i < MAX_INTERRUPTS; i++) {
        const char* state_str = get_irq_state_string(idt[i].state);
        const char* icon = "";
        
        switch (idt[i].state) {
            case IRQ_STATE_FREE:       icon = "⚪"; free_count++; break;
            case IRQ_STATE_REGISTERED: icon = "🟢"; registered_count++; break;
            case IRQ_STATE_EXECUTING:  icon = "🔴"; executing_count++; break;
        }
        
        printf("IRQ%2d: %s %-12s │ Calls: %3d │ %s\n", 
               i, icon, state_str, idt[i].call_count, 
               (idt[i].call_count > 0) ? idt[i].description : "Sin actividad");
    }
    
    UNLOCK_IDT();
    
    printf("\n📊 RESUMEN DE ESTADOS:\n");
    printf("  🟢 Registradas: %d\n", registered_count);
    printf("  🔴 Ejecutándose: %d\n", executing_count);
    printf("  ⚪ Libres: %d\n", free_count);
    printf("  📋 Total: %d\n", MAX_INTERRUPTS);
    printf("\n");
}

// Menú interactivo
void show_menu() {
    printf("\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    🐧 SIMULADOR KERNEL LINUX - INTERRUPCIONES 🐧             ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║  1. 🔥 Generar interrupción manual     │  6. 🗑️  Desregistrar ISR            ║\n");
    printf("║  2. 📝 Registrar ISR personalizada     │  7. 📊 Estadísticas del sistema     ║\n");
    printf("║  3. 🎯 Estado de la IDT                │  8. ⚙️  Configurar logging          ║\n");
    printf("║  4. 📜 Mostrar traza reciente          │  9. ❓ Ayuda del simulador          ║\n");
    printf("║  5. 🧪 Suite de pruebas múltiples      │  0. 🚪 Salir del programa           ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
    printf("Seleccione una opción [0-9]: ");
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

void improved_main_initialization() {
    printf("╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    🚀 INICIANDO SIMULADOR KERNEL LINUX                      ║\n");
    printf("║                          Versión 2.0 - Modo Educativo                       ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n");
    
    printf("\n🔧 FASE DE INICIALIZACIÓN DEL KERNEL:\n");
    printf("════════════════════════════════════════\n");
    
    // Inicializar sistema
    printf("📋 Inicializando IDT (Interrupt Descriptor Table)...\n");
    fflush(stdout);
    init_idt();
    
    printf("📈 Configurando sistema de estadísticas...\n");
    fflush(stdout);
    init_system_stats();
    
    // Registrar ISRs predeterminadas
    printf("⏰ Registrando handler del Timer PIT (IRQ0)...\n");
    fflush(stdout);
    register_isr(IRQ_TIMER, timer_isr, "Timer PIT - Reloj del sistema");
    
    printf("⌨️  Registrando handler del teclado (IRQ1)...\n");
    fflush(stdout);
    register_isr(IRQ_KEYBOARD, keyboard_isr, "Controlador de teclado 8042");
    
    // Iniciar hilo del timer
    printf("🕐 Iniciando hilo del timer automático...\n");
    fflush(stdout);
    if (pthread_create(&timer_thread, NULL, timer_thread_func, NULL) != 0) {
        add_trace("❌ KERNEL PANIC: Error creando hilo del timer");
        printf("❌ ERROR CRÍTICO: No se pudo iniciar el timer del sistema\n");
        return;
    }
    
    printf("\n✅ KERNEL INICIADO CORRECTAMENTE\n");
    printf("🎯 El sistema está listo para procesar interrupciones\n");
    printf("⏰ Timer automático generará IRQ0 cada 3 segundos\n\n");
    
    // Pequeña pausa para que el usuario vea la inicialización
    printf("Presione Enter para continuar al menú principal...");
    fflush(stdout);
    wait_for_enter();
}

// Función para guardar el estado actual de la IDT
void save_idt_state(irq_descriptor_t *backup) {
    LOCK_IDT();
    
    for (int i = 0; i < MAX_INTERRUPTS; i++) {
        backup[i].isr = idt[i].isr;
        backup[i].state = idt[i].state;
        backup[i].call_count = idt[i].call_count;
        backup[i].last_call = idt[i].last_call;
        backup[i].total_execution_time = idt[i].total_execution_time;
        strncpy(backup[i].description, idt[i].description, sizeof(backup[i].description) - 1);
        backup[i].description[sizeof(backup[i].description) - 1] = '\0';
    }
    
    UNLOCK_IDT();
    
    add_trace("💾 KERNEL: Estado de IDT guardado para respaldo");
}

// Función para restaurar el estado previo de la IDT
void restore_idt_state(const irq_descriptor_t *backup) {
    LOCK_IDT();
    
    for (int i = 0; i < MAX_INTERRUPTS; i++) {
        idt[i].isr = backup[i].isr;
        idt[i].state = backup[i].state;
        idt[i].call_count = backup[i].call_count;
        idt[i].last_call = backup[i].last_call;
        idt[i].total_execution_time = backup[i].total_execution_time;
        strncpy(idt[i].description, backup[i].description, sizeof(idt[i].description) - 1);
        idt[i].description[sizeof(idt[i].description) - 1] = '\0';
    }
    
    UNLOCK_IDT();
    
    add_trace("🧹 KERNEL: Estado de IDT restaurado tras pruebas");
}

// Función para limpiar ISRs de prueba (mantiene solo las del sistema)
void cleanup_test_isrs(void) {
    int cleaned_count = 0;
    
    LOCK_IDT();
    
    for (int i = 0; i < MAX_INTERRUPTS; i++) {
        // Preservar ISRs del sistema (IRQ0 Timer e IRQ1 Keyboard)
        if (i == IRQ_TIMER || i == IRQ_KEYBOARD) {
            continue;
        }
        
        // Limpiar cualquier otra ISR registrada
        if (idt[i].state != IRQ_STATE_FREE && idt[i].isr != NULL) {
            idt[i].isr = NULL;
            idt[i].state = IRQ_STATE_FREE;
            idt[i].call_count = 0;
            idt[i].total_execution_time = 0;
            snprintf(idt[i].description, sizeof(idt[i].description), 
                "IRQ %d - Disponible para asignación", i);
            cleaned_count++;
        }
    }
    
    UNLOCK_IDT();
    
    char trace_msg[MAX_TRACE_MSG_LEN];
    snprintf(trace_msg, sizeof(trace_msg), 
        "🧼 KERNEL: %d ISRs de prueba limpiadas - Solo ISRs del sistema preservadas", 
        cleaned_count);
    add_trace(trace_msg);
}
const char *get_irq_description(int irq_num) {
    for (size_t i = 0; i < sizeof(irq_table) / sizeof(irq_table[0]); ++i) {
        if (irq_table[i].irq == irq_num)
            return irq_table[i].desc;
    }
    return "Desconocido";
}

// Versión modificada de run_interrupt_test_suite()
void run_interrupt_test_suite(void) {
    printf("\n🧪 INICIANDO SUITE DE PRUEBAS DE INTERRUPCIONES ALEATORIAS\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    // Guardar estado actual de la IDT
    irq_descriptor_t idt_backup[MAX_INTERRUPTS];
    save_idt_state(idt_backup);

    // 1) Registrar todos los ISRs de la tabla (excluyendo IRQ0)
    printf("📝 Fase 1: Registrando controladores de interrupción...\n");
    for (size_t i = 0; i < sizeof(irq_table) / sizeof(irq_table[0]); ++i) {
        if (irq_table[i].irq == IRQ_TIMER) continue; // Evita IRQ0
        register_isr(irq_table[i].irq, custom_isr, irq_table[i].desc);
    }

    // 2) Preparar generador de números aleatorios
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned int seed = (unsigned int)(tv.tv_sec ^ tv.tv_usec ^ getpid());
    srand(seed);
    printf("🔢 Semilla aleatoria usada: %u\n", seed);

    // 3) Calcular cuántas interrupciones se dispararán (entre 3 y 8)
    int total_events = 3 + rand() % 6; // 3, 4, 5, 6, 7 o 8
    printf("\n🔥 Fase 2: Generando %d interrupciones aleatorias...\n\n", total_events);

    // 4) CORRECCIÓN PRINCIPAL: Disparar interrupciones seleccionando aleatoriamente de la tabla
    for (int ev = 1; ev <= total_events; ++ev) {
        // AQUÍ ESTÁ EL PROBLEMA CORREGIDO:
        // Seleccionar un índice aleatorio directamente de irq_table
        int table_idx = rand() % (sizeof(irq_table) / sizeof(irq_table[0]));
        int irq_num = irq_table[table_idx].irq;
        const char *irq_desc = irq_table[table_idx].desc;

        printf("\n🔔 Evento %d/%d → IRQ%d: %s\n",
               ev, total_events, irq_num, irq_desc);
        
        dispatch_interrupt(irq_num);
        
        // Esperar entre 100 ms y 800 ms para emular tiempos reales variables
        useconds_t delay_us = 100000 + (rand() % 701000); // 100,000–800,999 µs
        usleep(delay_us);
    }

    // ✅ Mostrar estado modificado de la IDT antes de limpiar
    printf("\n📋 Estado de la IDT tras ejecutar las interrupciones de prueba:\n");
    show_idt_status();

    // ✅ Restaurar estado original
    restore_idt_state(idt_backup);
    // Mostrar estadísticas finales
    printf("\n📊 Estadísticas de la suite de pruebas:\n");

    printf("\n🎉 SUITE DE PRUEBAS COMPLETADA CON ÉXITO\n");
    printf("📊 Revise los logs para las estadísticas de latencia y manejo\n");
    printf("🔄 Ejecute de nuevo para obtener una secuencia diferente\n");
}

// Función adicional para pruebas más avanzadas
void run_advanced_interrupt_test_suite(void) {
    printf("\n🚀 INICIANDO SUITE DE PRUEBAS AVANZADAS\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    // Guardar estado actual de la IDT
    irq_descriptor_t idt_backup[MAX_INTERRUPTS];
    save_idt_state(idt_backup);

    // Registrar ISRs
    printf("📝 Registrando controladores...\n");
    for (size_t i = 0; i < sizeof(irq_table) / sizeof(irq_table[0]); ++i) {
        if (irq_table[i].irq == IRQ_TIMER) continue;
        register_isr(irq_table[i].irq, custom_isr, irq_table[i].desc);
    }

    // Preparar aleatoriedad
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned int seed = (unsigned int)(tv.tv_sec ^ tv.tv_usec ^ getpid());
    srand(seed);
    printf("🔢 Semilla aleatoria: %u\n", seed);

    // Prueba 1: Ráfaga de interrupciones
    printf("\n🔥 Prueba 1: Ráfaga de interrupciones rápidas\n");
    int burst_count = 2 + rand() % 4; // 2-5 interrupciones
    for (int i = 0; i < burst_count; i++) {
        int idx = rand() % (sizeof(irq_table) / sizeof(irq_table[0]));
        printf("  💥 Ráfaga %d → IRQ%d: %s\n", i+1, irq_table[idx].irq, irq_table[idx].desc);
        dispatch_interrupt(irq_table[idx].irq);
        usleep(50000); // 50ms entre interrupciones
    }

    sleep(1); // Pausa entre pruebas

    // Prueba 2: Interrupciones con patrones variables
    printf("\n🎯 Prueba 2: Patrón de interrupciones variables\n");
    int pattern_count = 3 + rand() % 5; // 3-7 interrupciones
    for (int i = 0; i < pattern_count; i++) {
        int idx = rand() % (sizeof(irq_table) / sizeof(irq_table[0]));
        printf("  🎪 Patrón %d → IRQ%d: %s\n", i+1, irq_table[idx].irq, irq_table[idx].desc);
        dispatch_interrupt(irq_table[idx].irq);
        
        // Delay variable: corto, medio o largo
        int delay_type = rand() % 3;
        useconds_t delay = (delay_type == 0) ? 100000 : 
                          (delay_type == 1) ? 300000 : 600000;
        usleep(delay);
    }
    // ✅ Mostrar estado modificado de la IDT antes de limpiar
    printf("\n📋 Estado de la IDT tras ejecutar las interrupciones de prueba:\n");
    show_idt_status();

    // ✅ Restaurar estado original
    restore_idt_state(idt_backup);
    printf("\n🎉 SUITE AVANZADA COMPLETADA\n");
}



// Función principal
int main() {
    int option, irq_num;
    
    improved_main_initialization();
    
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
            
            // Mostrar última traza para explicar el proceso de interrupción
            printf("\n--- Proceso de interrupción ejecutado ---\n");
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
                
                // Mostrar última traza para confirmar el registro
                printf("\n--- Registro de ISR completado ---\n");
                show_last_trace();
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
                
                // Mostrar última traza para confirmar la desregistración
                printf("\n--- Desregistro de ISR completado ---\n");
                show_last_trace();
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
