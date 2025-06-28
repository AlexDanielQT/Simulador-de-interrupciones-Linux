# Simulador de Interrupciones Linux IDT

Un simulador educativo que replica el comportamiento del sistema de manejo de interrupciones basado en el modelo usado por el kernel de Linux, implementando una Tabla de Descriptores de Interrupción (IDT) funcional.

## Descripción

Este proyecto simula el mecanismo de interrupciones de un sistema operativo Linux, proporcionando una herramienta interactiva para comprender cómo funcionan las IRQs (Interrupt Request Lines), las ISRs (Interrupt Service Routines) y el dispatch de interrupciones en un entorno controlado.

## Características

- **IDT Completa**: Implementa una tabla de descriptores de interrupción con 16 IRQs disponibles
- **Timer Automático**: IRQ 0 configurada como timer del sistema que se ejecuta automáticamente
- **Threading Seguro**: Uso de mutexes para operaciones thread-safe
- **Trazabilidad Completa**: Sistema de logging con timestamps para seguir el flujo de ejecución
- **Estadísticas en Tiempo Real**: Métricas de rendimiento y contadores de interrupciones
- **Interfaz Interactiva**: Menú completo para gestión de interrupciones
- **Sistema de Logging Configurable**: Múltiples niveles de verbosidad
- **ISRs Personalizables**: Registro y desregistro dinámico de rutinas de servicio

## Componentes del Sistema

### IRQs Predefinidas
- **IRQ 0**: Timer del sistema (automático cada 3 segundos)
- **IRQ 1**: Controlador de teclado
- **IRQ 2-15**: Disponibles para ISRs personalizadas

### Estados de IRQ
- `FREE`: IRQ disponible para registro
- `REGISTERED`: IRQ con ISR registrada
- `EXECUTING`: IRQ actualmente en ejecución

### Niveles de Logging
- **SILENCIOSO**: Solo logging interno
- **SOLO USUARIO**: Muestra únicamente acciones del usuario
- **VERBOSE**: Muestra toda la actividad del sistema

## Compilación e Instalación

### Requisitos
- Compilador GCC con soporte C99
- Biblioteca pthread
- Sistema Unix/Linux (o WSL en Windows)
- Make (opcional, para usar Makefile)

### Compilación

#### Opción 1: Usando Makefile
```bash
make
```

#### Opción 2: Compilación manual
```bash
gcc -o interrupt_simulator interrupt_simulator.c -lpthread -lrt
```

### Ejecución

#### Opción 1: Usando script de lanzamiento
```bash
./interrupt_simulator.sh
```

#### Opción 2: Ejecución directa
```bash
./interrupt_simulator
```

## Uso

### Inicio Rápido

1. **Clonar/descargar el proyecto**
2. **Compilar**:
   ```bash
   make
   ```
3. **Ejecutar**:
   ```bash
   ./interrupt_simulator.sh
   ```
4. **Explorar el menú interactivo** y comenzar a experimentar con interrupciones

### Archivos del Proyecto

- **`Makefile`**: Automatiza la compilación con las flags correctas
- **`interrupt_simulator.c`**: Código fuente principal con toda la lógica
- **`interrupt_simulator.h`**: Definiciones, estructuras y prototipos
- **`interrupt_simulator.sh`**: Script para facilitar el lanzamiento
- **`README.md`**: Documentación completa del proyecto

### Menú Principal
El simulador presenta un menú interactivo con las siguientes opciones:

1. **Generar interrupción manual**: Dispara una IRQ específica
2. **Registrar ISR personalizada**: Asocia una rutina a un IRQ
3. **Mostrar estado de la IDT**: Vista del estado actual de todas las IRQs
4. **Mostrar traza reciente**: Log de eventos del sistema
5. **Generar múltiples interrupciones**: Suite de pruebas automatizada
6. **Desregistrar ISR**: Remueve una ISR de un IRQ
7. **Estadísticas del sistema**: Métricas de rendimiento
8. **Configurar logging**: Control de verbosidad del sistema
9. **Ayuda**: Información detallada del simulador

### Ejemplo de Uso Básico

1. **Registrar una ISR personalizada**:
   ```
   Seleccione opción: 2
   Ingrese IRQ (2-15): 5
   ✓ ISR registrada para IRQ 5
   ```

2. **Generar interrupción**:
   ```
   Seleccione opción: 1
   Ingrese IRQ (0-15): 5
   [14:23:15] >>> DESPACHANDO IRQ 5 (ISR Personalizada 5) - Llamada #1
   [14:23:15] CUSTOM ISR: Rutina personalizada para IRQ 5
   [14:23:15] <<< FINALIZANDO IRQ 5 - Retorno al flujo principal (Tiempo: 8456 μs)
   ```

3. **Ver estadísticas**:
   ```
   === ESTADÍSTICAS DEL SISTEMA ===
   Tiempo de funcionamiento: 120 segundos
   Total de interrupciones: 45
   Interrupciones de timer: 40
   Interrupciones de teclado: 2
   Interrupciones personalizadas: 3
   Tiempo promedio de respuesta: 8234.56 μs
   ```

## Configuración del Sistema

### Control de Logging
- **Modo Silencioso**: Solo guarda en historial
- **Modo Usuario**: Muestra acciones del usuario (por defecto)
- **Modo Verbose**: Muestra toda la actividad
- **Toggle Timer**: Control específico para logs del timer automático

### Personalización de Delays
Las constantes de tiempo pueden modificarse en el header:
```c
#define TIMER_INTERVAL_SEC 3           // Intervalo del timer automático
#define ISR_SIMULATION_DELAY_US 10000  // Delay de ISR estándar (10ms)
#define KEYBOARD_DELAY_US 5000         // Delay del teclado (5ms)
#define CUSTOM_DELAY_US 8000           // Delay de ISRs personalizadas (8ms)
```

## Métricas y Monitoreo

El sistema proporciona métricas detalladas:
- Contador de llamadas por IRQ
- Tiempo total de ejecución por IRQ
- Tiempo promedio de respuesta del sistema
- Estadísticas segregadas por tipo de interrupción
- Timestamps de última ejecución

## Testing y Validación

### Suite de Pruebas Incluida
- Registro y despacho de múltiples ISRs
- Pruebas de concurrencia
- Validación de estados de IRQ
- Verificación de thread-safety

### Casos de Prueba
```bash
# Ejecutar suite completa
Opción 5: Generar múltiples interrupciones de prueba

# Testing manual
1. Registrar ISR en IRQ 3
2. Generar 5 interrupciones consecutivas
3. Verificar contadores y tiempos
4. Desregistrar ISR
```

### Limpieza del Proyecto
```bash
make clean  # Elimina archivos objeto y ejecutable
```

## Estructura del Proyecto

```
interrupt_simulator/
├── Makefile                 # Configuración de compilación
├── interrupt_simulator.c    # Implementación principal
├── interrupt_simulator.h    # Definiciones y estructuras
├── interrupt_simulator.sh   # Script de lanzamiento
└── README.md               # Este archivo
```

### Estructuras Principales
- `irq_descriptor_t`: Descriptor de cada IRQ
- `trace_entry_t`: Entrada del sistema de trazabilidad
- `system_stats_t`: Estadísticas del sistema

## Limitaciones y Consideraciones

- **Simulación**: Este es un simulador educativo, no maneja interrupciones reales de hardware
- **Threading**: El timer automático ejecuta en un hilo separado
- **Memoria**: Traza limitada a 100 entradas (circular buffer)
- **IRQs**: Máximo 16 IRQs simultáneas
- **Concurrencia**: Las ISRs no son realmente concurrentes (simulación secuencial)

## Objetivos Educativos

Este simulador está diseñado para enseñar:
- Conceptos de interrupciones en sistemas operativos
- Manejo de la IDT (Interrupt Descriptor Table)
- Flujo de dispatch de interrupciones
- Threading y sincronización
- Métricas de rendimiento del sistema
- Debugging y trazabilidad de eventos

## Contribuciones

El proyecto está diseñado con fines educativos. Las mejoras sugeridas incluyen:
- Simulación de prioridades de interrupciones
- Implementación de interrupt masking
- Soporte para interrupciones anidadas
- Interfaz gráfica para visualización

## Licencia

Proyecto educativo de código abierto. Libre uso para propósitos académicos y de aprendizaje.

---

**Nota**: Este simulador replica el comportamiento conceptual del sistema de interrupciones de Linux con fines educativos, proporcionando una herramienta valiosa para comprender los mecanismos internos del kernel.