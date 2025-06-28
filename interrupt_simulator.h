#ifndef INTERRUPT_SIMULATOR_H
#define INTERRUPT_SIMULATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

// Constantes del sistema
#define MAX_INTERRUPTS 16
#define MAX_TRACE_LINES 100
#define MAX_TRACE_MSG_LEN 256
#define MAX_DESCRIPTION_LEN 64

// IRQs predefinidas del sistema
#define IRQ_TIMER 0
#define IRQ_KEYBOARD 1

// Intervalos de tiempo
#define TIMER_INTERVAL_SEC 3
#define ISR_SIMULATION_DELAY_US 10000  // 10ms
#define KEYBOARD_DELAY_US 5000         // 5ms
#define CUSTOM_DELAY_US 8000           // 8ms

// Códigos de retorno
#define SUCCESS 0
#define ERROR_INVALID_IRQ -1
#define ERROR_ISR_EXECUTING -2
#define ERROR_THREAD_CREATE -3

// Macros para validación y threading
#define IS_VALID_IRQ(irq) ((irq) >= 0 && (irq) < MAX_INTERRUPTS)
#define LOCK_IDT() pthread_mutex_lock(&idt_mutex)
#define UNLOCK_IDT() pthread_mutex_unlock(&idt_mutex)

// Estados de IRQ
typedef enum {
    IRQ_STATE_FREE = 0,
    IRQ_STATE_REGISTERED,
    IRQ_STATE_EXECUTING
} irq_state_t;

// Niveles de logging
typedef enum {
    LOG_LEVEL_SILENT = 0,    // Solo logging interno, sin output
    LOG_LEVEL_USER_ONLY,     // Solo acciones del usuario
    LOG_LEVEL_VERBOSE        // Todo (comportamiento actual)
} log_level_t;

// Descriptor de interrupción
typedef struct {
    void (*isr)(int);              // Puntero a la función ISR
    irq_state_t state;             // Estado actual del IRQ
    int call_count;                // Número de veces llamada
    time_t last_call;              // Timestamp de última llamada
    unsigned long total_execution_time;  // Tiempo total de ejecución en μs
    char description[MAX_DESCRIPTION_LEN];  // Descripción de la ISR
} irq_descriptor_t;

// Entrada de traza
typedef struct {
    char timestamp[16];            // Timestamp de la entrada
    char event[MAX_TRACE_MSG_LEN]; // Descripción del evento
    int irq_num;                   // Número de IRQ (-1 si no aplica)
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

// Variables globales
extern irq_descriptor_t idt[MAX_INTERRUPTS];
extern trace_entry_t trace_log[MAX_TRACE_LINES];
extern int trace_index;
extern int system_running;
extern int timer_counter;
extern pthread_t timer_thread;
extern pthread_mutex_t idt_mutex;
extern system_stats_t stats;

// Variable global para controlar el nivel de logging
extern log_level_t current_log_level;
extern int show_timer_logs;  // Control específico para logs del timer


// Funciones de utilidad
void get_timestamp(char *buffer, size_t size);
void add_trace(const char *event);
void add_trace_with_irq(const char *event, int irq_num);

// Nuevas funciones de logging
void add_trace_silent(const char *event);
void add_trace_with_irq_silent(const char *event, int irq_num);
void set_log_level(log_level_t level);
void toggle_timer_logs(void);

// Funciones de validación
int validate_irq_num(int irq_num);
int is_irq_available(int irq_num);
const char* get_irq_state_string(irq_state_t state);

// Funciones de inicialización
void init_idt(void);
void init_system_stats(void);

// Funciones de gestión de ISR
int register_isr(int irq_num, void (*isr_function)(int), const char *description);
int unregister_isr(int irq_num);
void dispatch_interrupt(int irq_num);

// ISRs predefinidas
void timer_isr(int irq_num);
void keyboard_isr(int irq_num);
void custom_isr(int irq_num);
void error_isr(int irq_num);

// Funciones de threading
void* timer_thread_func(void* arg);

// Funciones de interfaz
void show_idt_status(void);
void show_recent_trace(void);
void show_system_stats(void);
void show_help(void);
void show_menu(void);

// Funciones de testing
void run_interrupt_test_suite(void);
void test_concurrent_interrupts(void);
void test_stress_interrupts(void);

// Funciones de entrada/salida
void clear_input_buffer(void);
int get_valid_input(int min, int max);

// Funciones de estadísticas
void update_stats(int irq_num, unsigned long execution_time);

#endif // INTERRUPT_SIMULATOR_H