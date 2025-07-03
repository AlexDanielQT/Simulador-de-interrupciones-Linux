# Simulador de Interrupciones del Sistema Operativo

## Tabla de Contenidos
1. [Introducción](#introducción)
2. [Arquitectura del Sistema](#arquitectura-del-sistema)
3. [Estructuras de Datos](#estructuras-de-datos)
4. [Funcionalidades Principales](#funcionalidades-principales)
5. [Sistema de Trazabilidad](#sistema-de-trazabilidad)
6. [Gestión de la IDT](#gestión-de-la-idt)
7. [Manejo de Interrupciones](#manejo-de-interrupciones)
8. [ISRs Implementadas](#isrs-implementadas)
9. [Concurrencia y Sincronización](#concurrencia-y-sincronización)
10. [Interface de Usuario](#interface-de-usuario)
11. [Sistema de Pruebas](#sistema-de-pruebas)
12. [Manejo de Errores](#manejo-de-errores)
13. [Optimizaciones de Rendimiento](#optimizaciones-de-rendimiento)
14. [Funciones Auxiliares](#funciones-auxiliares)
15. [Casos de Uso](#casos-de-uso)
16. [Ejemplo de Ejecución](#ejemplo-de-ejecución)
17. [Consideraciones Técnicas](#consideraciones-técnicas)

## Introducción

Este proyecto implementa un simulador educativo del sistema de interrupciones de un sistema operativo tipo Unix/Linux. El simulador reproduce fielmente el comportamiento de la **Tabla de Descriptores de Interrupción (IDT)**, el manejo de **Interrupt Service Routines (ISRs)**, y el ciclo completo de procesamiento de interrupciones.

### Características Principales

- **Simulación realista** del hardware de interrupciones (PIC/APIC)
- **Implementación completa de la IDT** con 16 vectores de interrupción
- **Sistema de trazabilidad** con logging inteligente y filtros
- **Concurrencia thread-safe** usando mutexes
- **Estadísticas detalladas** de rendimiento del sistema
- **ISRs predefinidas** para timer, teclado y dispositivos personalizados

## Arquitectura del Sistema

### Componentes Principales

```
┌─────────────────────────────────────────────────────────────┐
│                    HARDWARE SIMULADO                       │
├─────────────────────────────────────────────────────────────┤
│  Timer PIT  │  Teclado  │  Dispositivos Personalizados    │
│    (IRQ0)   │   (IRQ1)  │        (IRQ2-IRQ15)            │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                 CONTROLADOR DE INTERRUPCIONES              │
├─────────────────────────────────────────────────────────────┤
│              dispatch_interrupt(irq_num)                   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│            TABLA DE DESCRIPTORES (IDT)                     │
├─────────────────────────────────────────────────────────────┤
│  IRQ0: Timer ISR     │  IRQ1: Keyboard ISR               │
│  IRQ2: Custom ISR    │  IRQ3-15: Disponibles             │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                 KERNEL SPACE                               │
├─────────────────────────────────────────────────────────────┤
│  Sistema de Trazabilidad │ Estadísticas │ Thread Safety   │
└─────────────────────────────────────────────────────────────┘
```

## Estructuras de Datos

### Descriptor de IRQ (`irq_descriptor_t`)

```c
typedef struct {
    void (*isr)(int);                    // Puntero a la función ISR
    irq_state_t state;                   // Estado actual del IRQ
    int call_count;                      // Número de llamadas realizadas
    time_t last_call;                    // Timestamp de la última llamada
    unsigned long total_execution_time;  // Tiempo total de ejecución (μs)
    char description[MAX_DESCRIPTION_LEN]; // Descripción del handler
} irq_descriptor_t;
```

### Estados de IRQ (`irq_state_t`)

- **`IRQ_STATE_FREE`**: Vector disponible para asignación
- **`IRQ_STATE_REGISTERED`**: ISR registrada y lista para ejecutar
- **`IRQ_STATE_EXECUTING`**: ISR actualmente en ejecución (protección reentrancy)

### Entrada de Traza (`trace_entry_t`)

```c
typedef struct {
    char timestamp[32];           // Timestamp en formato HH:MM:SS
    char event[MAX_TRACE_MSG_LEN]; // Descripción del evento
    int irq_num;                  // Número de IRQ (-1 si no aplica)
} trace_entry_t;
```

### Estadísticas del Sistema (`system_stats_t`)

```c
typedef struct {
    unsigned long total_interrupts;    // Total de interrupciones procesadas
    unsigned long timer_interrupts;    // Interrupciones del timer
    unsigned long keyboard_interrupts; // Interrupciones del teclado
    unsigned long custom_interrupts;   // Interrupciones personalizadas
    unsigned long average_response_time; // Tiempo promedio de respuesta (μs)
    time_t system_start_time;          // Tiempo de inicio del sistema
} system_stats_t;
```

## Funcionalidades Principales

### Inicialización del Sistema

```c
void init_idt();              // Inicializa la IDT con 16 vectores
void init_system_stats();     // Inicializa estadísticas del sistema
```

La función `init_idt()` configura todos los vectores de interrupción en estado `IRQ_STATE_FREE` y establece descripciones por defecto.

### Registro y Desregistro de ISRs

```c
int register_isr(int irq_num, void (*isr_function)(int), const char *description);
int unregister_isr(int irq_num);
```

**Validaciones implementadas:**
- Verificación de rango válido de IRQ (0-15)
- Protección contra registro durante ejecución
- Actualización atómica del estado
- Logging detallado de operaciones

### Despacho de Interrupciones

```c
void dispatch_interrupt(int irq_num);
```

**Proceso de despacho:**
1. **Validación del IRQ**: Verificar rango y disponibilidad
2. **Simulación de hardware**: Activación de línea de interrupción
3. **Guardado de contexto**: Simular el cambio de contexto del CPU
4. **Consulta de IDT**: Buscar el handler correspondiente
5. **Ejecución de ISR**: Llamar a la función registrada
6. **Restauración**: Volver al contexto anterior
7. **Actualización de estadísticas**: Métricas de rendimiento

## Sistema de Trazabilidad

### Niveles de Logging

```c
typedef enum {
    LOG_LEVEL_SILENT,      // Sin salida por consola
    LOG_LEVEL_USER_ONLY,   // Solo eventos del usuario
    LOG_LEVEL_VERBOSE      // Todos los eventos
} log_level_t;
```

### Funciones de Logging

```c
void add_trace(const char *event);                    // Logging normal
void add_trace_with_irq(const char *event, int irq_num); // Con IRQ específico
void add_trace_silent(const char *event);             // Solo almacenar
void add_trace_smart(const char *event, int irq_num, int is_timer_related); // Inteligente
```

### Control de Visualización

```c
void set_log_level(log_level_t level);  // Cambiar nivel de logging
void toggle_timer_logs(void);           // Alternar logs del timer
```

La función `add_trace_smart()` implementa un sistema inteligente que:
- Siempre almacena eventos en el historial
- Filtra la salida según el nivel de logging
- Permite control separado para eventos del timer
- Mantiene thread-safety con mutexes

### Funciones de Visualización Avanzadas

```c
void show_last_trace(void);                          // Última traza no-timer
void show_last_n_non_timer_traces(int n);           // Últimas N trazas no-timer
void debug_trace_buffer(void);                      // Debug del buffer circular
```

#### Filtrado Inteligente de Trazas

El sistema implementa un filtro inteligente que distingue entre:
- **Eventos del timer**: Generados automáticamente por el hilo del timer
- **Eventos del usuario**: Generados por interacciones manuales
- **Eventos del sistema**: Operaciones críticas del kernel

```c
int is_timer_related_trace(const trace_entry_t *entry);
```

Esta función detecta patrones específicos en las trazas para clasificarlas correctamente.

## Gestión de la IDT

### Funciones de Estado

```c
int validate_irq_num(int irq_num);           // Validar número de IRQ
int is_irq_available(int irq_num);           // Verificar disponibilidad
const char* get_irq_state_string(irq_state_t state); // Obtener string del estado
```

### Protección de Concurrencia

```c
#define LOCK_IDT()   pthread_mutex_lock(&idt_mutex)
#define UNLOCK_IDT() pthread_mutex_unlock(&idt_mutex)
```

Todas las operaciones críticas sobre la IDT están protegidas con mutexes para garantizar:
- **Atomicidad** en cambios de estado
- **Consistencia** de datos compartidos
- **Prevención de race conditions**

### Funciones de Visualización

```c
void show_idt_status(void);                  // Estado completo de la IDT
void debug_all_irq_states(void);             // Debug detallado de estados
```

## Manejo de Interrupciones

### Ciclo Completo de Procesamiento

1. **Activación de Hardware**
   ```
   🔥 HARDWARE: IRQ X disparada - Línea de interrupción activada
   ```

2. **Intervención del CPU**
   ```
   🚨 CPU: Guardando contexto actual - Registros y estado del procesador
   ```

3. **Consulta del Kernel**
   ```
   🔍 KERNEL: Consultando IDT[X] - Vector de interrupción encontrado
   ```

4. **Ejecución del Handler**
   ```
   ⚡ KERNEL: Ejecutando ISR "Descripción" - Llamada #N [Modo Kernel]
   ```

5. **Restauración del Sistema**
   ```
   🔄 CPU: Restaurando contexto - Volviendo al proceso interrumpido (Xμs)
   ✅ KERNEL: IRQ X procesada - Sistema listo para nuevas interrupciones
   ```

### Medición de Rendimiento

El sistema utiliza `clock_gettime(CLOCK_MONOTONIC)` para medir con precisión de microsegundos:
- **Tiempo de ejecución** de cada ISR
- **Tiempo de respuesta** del sistema
- **Estadísticas acumuladas** por tipo de interrupción

## ISRs Implementadas

### Timer ISR (IRQ 0)

```c
void timer_isr(int irq_num);
```

**Funcionalidad:**
- Incrementa contador global del sistema
- Simula actualización de jiffies del kernel
- Verifica quantum de procesos (scheduler)
- Delay simulado: `ISR_SIMULATION_DELAY_US`

**Mensajes de traza:**
```
⏰ TIMER_ISR: Tick del sistema #N - Actualizando jiffies del kernel
📊 SCHEDULER: Verificando quantum de procesos - Time slice check
🔄 TIMER_ISR: Completada - Sistema de tiempo actualizado
```

### Keyboard ISR (IRQ 1)

```c
void keyboard_isr(int irq_num);
```

**Funcionalidad:**
- Simula lectura de scancode del controlador 8042
- Traduce scancode a keycode
- Envía evento a la cola de entrada del sistema
- Delay simulado: `KEYBOARD_DELAY_US`

**Mensajes de traza:**
```
⌨️ KEYBOARD_ISR: Leyendo scancode del controlador 8042
🔤 INPUT_LAYER: Traduciendo scancode a keycode
📤 EVENT_QUEUE: Enviando evento de teclado a /dev/input/eventX
```

### Custom ISR (IRQ 2-15)

```c
void custom_isr(int irq_num);
```

**Funcionalidad:**
- Maneja dispositivos personalizados
- Simula intercambio de datos con hardware
- Prepara dispositivo para nuevas operaciones
- Delay simulado: `CUSTOM_DELAY_US`

**Mensajes de traza:**
```
🔧 CUSTOM_ISR: Procesando interrupción de dispositivo personalizado
💾 DEVICE_DRIVER: Intercambiando datos con hardware específico
✅ CUSTOM_ISR: Operación completada - Hardware listo para nuevas operaciones
```

## Concurrencia y Sincronización

### Mutexes Utilizados

```c
pthread_mutex_t idt_mutex;      // Protección de la IDT
pthread_mutex_t trace_mutex;    // Protección del sistema de traza
pthread_mutex_t stats_mutex;    // Protección de estadísticas (local)
```

### Thread del Timer

```c
void* timer_thread_func(void* arg);
```

**Características:**
- Ejecuta en hilo separado (`pthread_t timer_thread`)
- Genera IRQ0 cada `TIMER_INTERVAL_SEC` segundos
- Termina limpiamente cuando `system_running = 0`
- Simula el comportamiento del PIT (Programmable Interval Timer)

### Protección contra Reentrancy

El sistema previene la ejecución concurrente de la misma ISR mediante:
- Verificación de estado `IRQ_STATE_EXECUTING`
- Cambio atómico de estado durante ejecución
- Restauración a `IRQ_STATE_REGISTERED` al finalizar

## Interface de Usuario

### Menú Principal

```c
void show_menu(void);
```

El sistema presenta un menú interactivo con las siguientes opciones:

```
╔══════════════════════════════════════════════════════════════════════════════╗
║                    🐧 SIMULADOR KERNEL LINUX - INTERRUPCIONES 🐧             ║
╠══════════════════════════════════════════════════════════════════════════════╣
║  1. 🔥 Generar interrupción manual     │  6. 🗑️  Desregistrar ISR            ║
║  2. 📝 Registrar ISR personalizada     │  7. 📊 Estadísticas del sistema     ║
║  3. 🎯 Estado de la IDT                │  8. ⚙️  Configurar logging          ║
║  4. 📜 Mostrar traza reciente          │  9. ❓ Ayuda del simulador          ║
║  5. 🧪 Suite de pruebas múltiples      │  0. 🚪 Salir del programa           ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

### Submenú de Logging

```c
void logging_submenu(void);
```

Permite configurar el nivel de detalle de los logs:

1. **Modo silencioso**: Solo almacenar en historial
2. **Modo usuario**: Solo acciones del usuario
3. **Modo verbose**: Mostrar todo
4. **Toggle logs del timer**: Activar/desactivar logs del timer
5. **Vista temporal**: Mostrar logs del timer por 30 segundos

### Funciones de Entrada

```c
int get_valid_input(int min, int max);    // Validar entrada numérica
void clear_input_buffer(void);           // Limpiar buffer de entrada
void wait_for_enter(void);               // Esperar confirmación
```

### Inicialización Mejorada

```c
void improved_main_initialization(void);
```

Presenta una secuencia de inicialización visual que muestra:
- Inicialización de la IDT
- Configuración de estadísticas
- Registro de ISRs del sistema
- Inicio del hilo del timer
- Confirmación de sistema listo

## Sistema de Pruebas

### Suite de Pruebas Aleatorias

```c
void run_interrupt_test_suite(void);
```

**Características:**
- Genera entre 3 y 8 interrupciones aleatorias
- Selecciona IRQs de una tabla predefinida
- Usa semilla aleatoria basada en tiempo y PID
- Restaura estado original de la IDT

### Tabla de IRQs para Pruebas

```c
static const struct {
    int irq;
    const char *desc;
} irq_table[] = {
    {IRQ_KEYBOARD, "Controlador de teclado PS/2"},
    {2, "Controlador de red Ethernet"},
    {3, "Puerto serie COM2"},
    {4, "Puerto serie COM1"},
    {5, "Controlador de sonido"},
    {6, "Controlador de disquete"},
    {7, "Puerto paralelo LPT1"},
    {8, "Reloj CMOS"},
    {9, "Controlador SCSI"},
    {10, "Controlador USB"},
    {11, "Tarjeta gráfica"},
    {12, "Puerto PS/2 ratón"},
    {13, "Coprocesador matemático"},
    {14, "Controlador IDE primario"},
    {15, "Controlador IDE secundario"}
};
```

### Suite de Pruebas Avanzadas

```c
void run_advanced_interrupt_test_suite(void);
```

**Funcionalidades:**
- **Prueba de ráfaga**: Interrupciones rápidas consecutivas
- **Prueba de patrones**: Delays variables entre interrupciones
- **Respaldo y restauración**: Preserva estado original

### Funciones de Respaldo

```c
void save_idt_state(irq_descriptor_t *backup);        // Guardar estado
void restore_idt_state(const irq_descriptor_t *backup); // Restaurar estado
void cleanup_test_isrs(void);                         // Limpiar ISRs de prueba
```

## Manejo de Errores

### Códigos de Retorno

```c
#define SUCCESS 0
#define ERROR_INVALID_IRQ -1
#define ERROR_IRQ_BUSY -2
#define ERROR_IRQ_NOT_REGISTERED -3
```

### Validaciones Implementadas

- **Rango de IRQ**: Verificar que esté entre 0 y 15
- **Estado del IRQ**: Verificar disponibilidad antes de registro
- **Protección de reentrancy**: Prevenir ejecución concurrente
- **Inicialización de punteros**: Verificar ISR no nula

### Manejo de Errores Críticos

```c
void handle_critical_error(const char *error_msg);
```

Esta función maneja errores críticos del sistema:
- Logging de emergencia
- Terminación controlada
- Limpieza de recursos

## Optimizaciones de Rendimiento

### Buffer Circular de Trazas

El sistema utiliza un buffer circular para las trazas con las siguientes características:
- **Tamaño fijo**: `MAX_TRACE_LINES` entradas
- **Índice circular**: `trace_index` con módulo automático
- **Sobrescritura inteligente**: Reemplaza entradas más antiguas
- **Acceso thread-safe**: Protegido con `trace_mutex`

### Medición de Precisión

```c
struct timespec start_time, end_time;
clock_gettime(CLOCK_MONOTONIC, &start_time);
// ... ejecución de ISR ...
clock_gettime(CLOCK_MONOTONIC, &end_time);
```

Utiliza `CLOCK_MONOTONIC` para mediciones precisas independientes de cambios de hora del sistema.

### Estadísticas Acumuladas

Las estadísticas se mantienen en memoria para acceso rápido:
- **Contadores por tipo**: Timer, teclado, personalizadas
- **Tiempo promedio**: Calculado dinámicamente
- **Uptime del sistema**: Desde inicio de ejecución

## Funciones Auxiliares

### Gestión de Descripciones

```c
const char *get_irq_description(int irq_num);
```

Obtiene la descripción estándar de hardware para un IRQ específico basándose en la tabla de dispositivos comunes.

### Funciones de Debug

```c
void debug_trace_buffer(void);         // Analizar buffer de trazas
void debug_all_irq_states(void);      // Estados detallados de IRQs
```

Estas funciones proporcionan información detallada para:
- Análisis de rendimiento
- Debugging del sistema
- Verificación de consistencia

### Funciones de Estadísticas

```c
void show_system_stats(void);         // Mostrar estadísticas completas
void update_stats(int irq_num);       // Actualizar estadísticas específicas
```

### Funciones de Ayuda

```c
void show_help(void);                 // Mostrar ayuda completa del sistema
```

Presenta información educativa sobre:
- Arquitectura del sistema de interrupciones
- Mapa de IRQs estándar
- Flujo de procesamiento de interrupciones
- Conceptos técnicos fundamentales

## Casos de Uso

### Caso 1: Simulación de Teclado

```c
// Registrar ISR de teclado (ya registrada por defecto)
register_isr(IRQ_KEYBOARD, keyboard_isr, "Controlador de teclado PS/2");

// Simular pulsación de tecla
dispatch_interrupt(IRQ_KEYBOARD);
```

### Caso 2: Dispositivo Personalizado

```c
// Registrar ISR personalizada
register_isr(5, custom_isr, "Controlador de sonido");

// Simular interrupción de sonido
dispatch_interrupt(5);
```

### Caso 3: Pruebas de Stress

```c
// Ejecutar múltiples interrupciones
for (int i = 0; i < 10; i++) {
    dispatch_interrupt(i % MAX_INTERRUPTS);
    usleep(100000); // 100ms
}
```

## Ejemplo de Ejecución

### Secuencia Típica

1. **Inicialización**:
   ```
   🚀 INICIANDO SIMULADOR KERNEL LINUX
   📋 Inicializando IDT (Interrupt Descriptor Table)...
   ⏰ Registrando handler del Timer PIT (IRQ0)...
   ⌨️ Registrando handler del teclado (IRQ1)...
   🕐 Iniciando hilo del timer automático...
   ✅ KERNEL INICIADO CORRECTAMENTE
   ```

2. **Interrupción Manual**:
   ```
   🔥 HARDWARE: IRQ1 disparada - Línea de interrupción activada
   🚨 CPU: Guardando contexto actual - Registros y estado del procesador
   🔍 KERNEL: Consultando IDT[1] - Vector de interrupción encontrado
   ⚡ KERNEL: Ejecutando ISR "Controlador de teclado PS/2" - Llamada #1
   ⌨️ KEYBOARD_ISR: Leyendo scancode del controlador 8042
   🔤 INPUT_LAYER: Traduciendo scancode a keycode
   📤 EVENT_QUEUE: Enviando evento de teclado a /dev/input/eventX
   🔄 CPU: Restaurando contexto - Volviendo al proceso interrumpido (245μs)
   ✅ KERNEL: IRQ1 procesada - Sistema listo para nuevas interrupciones
   ```

3. **Timer Automático**:
   ```
   ⏰ TIMER_ISR: Tick del sistema #15 - Actualizando jiffies del kernel
   📊 SCHEDULER: Verificando quantum de procesos - Time slice check
   🔄 TIMER_ISR: Completada - Sistema de tiempo actualizado
   ```

4. **Estadísticas**:
   ```
   ╔══════════════════════════════════════════════════════════════════════════════╗
   ║                        ESTADÍSTICAS DEL KERNEL                              ║
   ║                     Simulando: /proc/stat y /proc/uptime                    ║
   ╠══════════════════════════════════════════════════════════════════════════════╣
   ║ 🕐 Uptime del sistema:           00:02:45 (165 segundos)                    ║
   ║ 📊 Total de interrupciones:      73                                         ║
   ║ ⏰ Interrupciones de timer:       55 (IRQ 0)                                ║
   ║ ⌨️ Interrupciones de teclado:     12 (IRQ 1)                                ║
   ║ 🔧 Interrupciones personalizadas: 6 (IRQ 2-15)                              ║
   ║ ⚡ Tiempo promedio de ISR:        234.50 μs                                  ║
   ║ 📈 Tasa de interrupciones:        0.44 IRQs/segundo                         ║
   ╚══════════════════════════════════════════════════════════════════════════════╝
   ```

## Consideraciones Técnicas

### Arquitectura del Sistema

El simulador está basado en la arquitectura x86 estándar:
- **IDT**: Tabla de 16 entradas (0-15)
- **PIC**: Controlador de interrupciones programable
- **Timer PIT**: Temporizador programable a intervalos
- **Controlador 8042**: Controlador de teclado PS/2

### Limitaciones Conocidas

1. **Número de IRQs**: Limitado a 16 (compatible con PIC estándar)
2. **Precisión de timing**: Dependiente del sistema operativo host
3. **Simulación de hardware**: No incluye aspectos de bajo nivel como DMA
4. **Persistencia**: Los datos se pierden al cerrar el programa

### Extensiones Futuras

1. **Soporte para APIC**: Más de 16 IRQs
2. **Simulación de DMA**: Acceso directo a memoria
3. **Interfaz gráfica**: GUI para visualización
4. **Persistencia**: Guardar/cargar configuraciones
5. **Red**: Simulación de interrupciones de red
6. **Profiling**: Análisis detallado de rendimiento

### Compatibilidad

- **Sistemas operativos**: Linux, macOS, Windows (con pthread)
- **Compiladores**: GCC, Clang
- **Arquitecturas**: x86, x86_64, ARM (con adaptaciones)

### Dependencias

- **pthread**: Para concurrencia
- **time.h**: Para mediciones de tiempo
- **sys/time.h**: Para precisión de microsegundos
- **stdio.h, stdlib.h, string.h**: Funciones estándar de C

---

## Conclusión

Este simulador proporciona una herramienta educativa completa para entender el funcionamiento interno del sistema de interrupciones de un kernel moderno. Su diseño modular, interfaz intuitiva y logging detallado lo hacen ideal para:

- **Educación**: Enseñanza de conceptos de sistemas operativos
- **Desarrollo**: Prototipado de controladores de dispositivos
- **Investigación**: Análisis de rendimiento de sistemas de interrupciones
- **Debugging**: Comprensión de problemas de concurrencia

La implementación thread-safe y las extensivas funciones de debug garantizan un entorno de aprendizaje seguro y productivo.
