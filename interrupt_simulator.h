#ifndef INTERRUPT_SIMULATOR_H
#define INTERRUPT_SIMULATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>   // Para gettimeofday
#include <unistd.h>     // Para getpid

// Configuración del simulador
#define MAX_INTERRUPTS 16
#define MAX_TRACE_LINES 100
#define MAX_TRACE_MSG_LEN 256
#define MAX_DESCRIPTION_LEN 64

// Intervalos de tiempo (en segundos y microsegundos)
#define TIMER_INTERVAL_SEC 3
#define ISR_SIMULATION_DELAY_US 100000  // 100ms
#define KEYBOARD_DELAY_US 50000         // 50ms
#define CUSTOM_DELAY_US 75000           // 75ms

// IRQs estándar del sistema
#define IRQ_TIMER 0
#define IRQ_KEYBOARD 1

// Códigos de error
#define SUCCESS 0
#define ERROR_INVALID_IRQ -1
#define ERROR_ISR_EXECUTING -2
#define ERROR_NO_ISR -3

// Macros para validación y acceso seguro
#define IS_VALID_IRQ(irq) ((irq) >= 0 && (irq) < MAX_INTERRUPTS)
#define LOCK_IDT() pthread_mutex_lock(&idt_mutex)
#define UNLOCK_IDT() pthread_mutex_unlock(&idt_mutex)


// Estados de IRQ
typedef enum {
    IRQ_STATE_FREE,
    IRQ_STATE_REGISTERED,
    IRQ_STATE_EXECUTING
} irq_state_t;

// Tipos de IRQ según propósito
typedef enum {
    IRQ_TYPE_SYSTEM,   // IRQ0, IRQ1
    IRQ_TYPE_USER,     // IRQ2 - IRQ15
    IRQ_TYPE_INVALID   // Para valores fuera de rango (negativo o > 15)
} irq_type_t;

// Niveles de logging
typedef enum {
    LOG_LEVEL_SILENT,
    LOG_LEVEL_USER_ONLY,
    LOG_LEVEL_VERBOSE
} log_level_t;

// Descriptor de IRQ en la IDT
typedef struct {
    void (*isr)(int);                    // Puntero a la función ISR
    irq_state_t state;                   // Estado actual del IRQ
    int call_count;                      // Número de veces llamada
    time_t last_call;                    // Timestamp de última llamada
    unsigned long total_execution_time;  // Tiempo total de ejecución en μs
    char description[MAX_DESCRIPTION_LEN]; // Descripción del handler
} irq_descriptor_t;

// Entrada de traza
typedef struct {
    char timestamp[16];
    char event[MAX_TRACE_MSG_LEN];
    int irq_num;
} trace_entry_t;

// Estadísticas del sistema
typedef struct {
    unsigned long total_interrupts;
    unsigned long timer_interrupts;
    unsigned long keyboard_interrupts;
    unsigned long custom_interrupts;
    double average_response_time;
    time_t system_start_time;
} system_stats_t;

// Entrada para tabla de IRQs de prueba
typedef struct {
    int irq;
    const char *desc;
} irq_entry_t;

// Tabla de IRQs para pruebas (agregar esta definición)
static const irq_entry_t irq_table[] = {
    {2, "Tarjeta de red Ethernet"},
    {3, "Puerto serie COM2"},
    {4, "Puerto serie COM1"},
    {5, "Tarjeta de sonido"},
    {6, "Controlador de floppy"},
    {7, "Puerto paralelo LPT1"},
    {8, "Reloj de tiempo real (RTC)"},
    {9, "Controlador ACPI"},
    {10, "Dispositivo USB"},
    {11, "Controlador SCSI"}
};

// Variables globales
extern irq_descriptor_t idt[MAX_INTERRUPTS];
extern trace_entry_t trace_log[MAX_TRACE_LINES];
extern int trace_index;
extern int system_running;
extern int timer_counter;
extern pthread_t timer_thread;
extern pthread_mutex_t idt_mutex;
extern pthread_mutex_t trace_mutex;
extern system_stats_t stats;
extern log_level_t current_log_level;
extern int show_timer_logs;

// Funciones de utilidad
void get_timestamp(char *buffer, size_t size);
int validate_irq_num(int irq_num);
int is_irq_available(int irq_num);
const char* get_irq_state_string(irq_state_t state);
irq_type_t get_irq_type(int irq_num);

// Funciones de trazabilidad
void add_trace(const char *event);
void add_trace_with_irq(const char *event, int irq_num);
void add_trace_silent(const char *event);
void add_trace_with_irq_silent(const char *event, int irq_num);
void add_trace_smart(const char *event, int irq_num, int is_timer_related);

// Funciones de configuración
void set_log_level(log_level_t level);
void toggle_timer_logs(void);

// Funciones de inicialización
void init_idt(void);
void init_system_stats(void);
void update_stats(int irq_num, unsigned long execution_time);

// Funciones de manejo de ISR
int register_isr(int irq_num, void (*isr_function)(int), const char *description);
int unregister_isr(int irq_num);
void dispatch_interrupt(int irq_num);

// ISRs predefinidas
void timer_isr(int irq_num);
void keyboard_isr(int irq_num);
void custom_isr(int irq_num);
void error_isr(int irq_num);

// Funciones de hilo
void* timer_thread_func(void* arg);

// Funciones de visualización
void show_idt_status(void);
void show_recent_trace(void);
void show_last_trace(void);
void show_last_n_non_timer_traces(int n);
void debug_trace_buffer(void);
void show_system_stats(void);
void show_help(void);
void show_menu(void);
void logging_submenu(void);

// Funciones de pruebas
void run_interrupt_test_suite(void);
void test_concurrent_interrupts(void);
void test_stress_interrupts(void);

// Funciones auxiliares
void clear_input_buffer(void);
int get_valid_input(int min, int max);
void wait_for_enter(void);
void improved_main_initialization(void);

// Funciones de backup/restore
void save_idt_state(irq_descriptor_t *backup);
void restore_idt_state(const irq_descriptor_t *backup);
void cleanup_test_isrs(void);

// Funciones auxiliares para detección de trazas
int is_timer_related_trace(const trace_entry_t *entry);

#endif // INTERRUPT_SIMULATOR_H
