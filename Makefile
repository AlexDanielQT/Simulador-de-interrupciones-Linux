# Makefile para el Simulador de Interrupciones Linux
# Proyecto de Sistemas Operativos

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -O2 -g -D_POSIX_C_SOURCE=200809L
LDFLAGS = -pthread -lrt
TARGET = interrupt_simulator
SOURCES = interrupt_simulator.c
HEADERS = interrupt_simulator.h
OBJECTS = $(SOURCES:.c=.o)

# Regla principal
all: $(TARGET)

# Compilación del ejecutable
$(TARGET): $(OBJECTS) $(HEADERS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "✓ Simulador compilado exitosamente"

# Compilación de archivos objeto
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Ejecutar el simulador
run: $(TARGET)
	@echo "Iniciando simulador de interrupciones..."
	./$(TARGET)

# Ejecutar con valgrind para detección de memory leaks
valgrind: $(TARGET)
	@echo "Ejecutando con Valgrind..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--suppressions=/usr/share/gdb/auto-load/usr/lib/x86_64-linux-gnu/libthread_db-1.0.so \
		./$(TARGET)

# Generar documentación con doxygen (si está disponible)
docs:
	@if command -v doxygen >/dev/null 2>&1; then \
		if [ -f "Doxyfile" ]; then \
			doxygen Doxyfile; \
			echo "✓ Documentación generada en docs/"; \
		else \
			echo "Archivo Doxyfile no encontrado"; \
		fi; \
	else \
		echo "Doxygen no está instalado"; \
	fi

# Limpiar archivos compilados
clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -rf docs/
	rm -f *.log *.txt core
	@echo "✓ Archivos limpiados"

# Limpiar todo incluyendo archivos de backup
distclean: clean
	rm -f *~ *.bak
	@echo "✓ Limpieza completa realizada"

# Instalación de dependencias en Debian/Ubuntu
install-deps:
	@echo "Instalando dependencias..."
	@if command -v apt-get >/dev/null 2>&1; then \
		sudo apt-get update; \
		sudo apt-get install -y build-essential gcc libc6-dev valgrind; \
	elif command -v yum >/dev/null 2>&1; then \
		sudo yum groupinstall "Development Tools"; \
		sudo yum install gcc glibc-devel valgrind; \
	elif command -v pacman >/dev/null 2>&1; then \
		sudo pacman -S base-devel gcc valgrind; \
	else \
		echo "Gestor de paquetes no reconocido"; \
	fi
	@echo "✓ Dependencias instaladas"

# Crear versión de debug
debug: CFLAGS += -DDEBUG -g3 -O0 -fsanitize=address
debug: LDFLAGS += -fsanitize=address
debug: clean $(TARGET)
	@echo "✓ Versión de debug compilada"

# Crear versión release optimizada
release: CFLAGS += -DNDEBUG -O3 -march=native
release: clean $(TARGET)
	@echo "✓ Versión release compilada"

# Verificar sintaxis sin compilar
check:
	$(CC) $(CFLAGS) -fsyntax-only $(SOURCES)
	@echo "✓ Sintaxis verificada"

# Análisis estático con cppcheck (si está disponible)
static-analysis:
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --std=c99 $(SOURCES); \
		echo "✓ Análisis estático completado"; \
	else \
		echo "cppcheck no está instalado"; \
	fi

# Mostrar información del sistema
info:
	@echo "=== Información del Sistema ==="
	@echo "Compilador: $(CC) $(shell $(CC) --version | head -n1)"
	@echo "Flags: $(CFLAGS)"
	@echo "Linker flags: $(LDFLAGS)"
	@echo "Fuentes: $(SOURCES)"
	@echo "Headers: $(HEADERS)"
	@echo "Objeto: $(OBJECTS)"
	@echo "Target: $(TARGET)"
	@echo "SO: $(shell uname -a)"
	@echo "Threads: $(shell nproc) cores disponibles"

# Ejecutar tests automáticos
test: $(TARGET)
	@if [ -f "test_simulator.sh" ]; then \
		chmod +x test_simulator.sh; \
		./test_simulator.sh; \
	else \
		echo "Script de tests no encontrado"; \
	fi

# Crear paquete tar.gz
package: clean
	@mkdir -p interrupt_simulator_project
	@cp *.c *.h Makefile README.md test_simulator.sh interrupt_simulator_project/ 2>/dev/null || true
	tar -czf interrupt_simulator.tar.gz interrupt_simulator_project/
	@rm -rf interrupt_simulator_project
	@echo "✓ Paquete creado: interrupt_simulator.tar.gz"

# Formatear código con indent (si está disponible)
format:
	@if command -v indent >/dev/null 2>&1; then \
		indent -kr -i4 -ts4 -sob -l80 -ss -ncs -cp1 $(SOURCES) $(HEADERS); \
		rm -f *.c~ *.h~; \
		echo "✓ Código formateado"; \
	else \
		echo "indent no está instalado"; \
	fi

# Benchmark del simulador
benchmark: $(TARGET)
	@echo "Ejecutando benchmark..."
	@time ./$(TARGET) < /dev/null || true
	@echo "✓ Benchmark completado"

# Reglas que no generan archivos
.PHONY: all run clean distclean install-deps debug release check info docs valgrind package test format benchmark static-analysis

# Ayuda
help:
	@echo "Simulador de Interrupciones Linux - Makefile"
	@echo ""
	@echo "Comandos disponibles:"
	@echo "  make             - Compila el simulador"
	@echo "  make run         - Compila y ejecuta el simulador"
	@echo "  make debug       - Compila versión de debug con AddressSanitizer"
	@echo "  make release     - Compila versión optimizada"
	@echo "  make check       - Verifica sintaxis"
	@echo "  make test        - Ejecuta tests automáticos"
	@echo "  make clean       - Limpia archivos compilados"
	@echo "  make distclean   - Limpieza completa"
	@echo "  make valgrind    - Ejecuta con detección de memory leaks"
	@echo "  make docs        - Genera documentación"
	@echo "  make package     - Crea paquete tar.gz"
	@echo "  make format      - Formatea el código fuente"
	@echo "  make benchmark   - Ejecuta benchmark de rendimiento"
	@echo "  make install-deps- Instala dependencias del sistema"
	@echo "  make info        - Muestra información del sistema"
	@echo "  make help        - Muestra esta ayuda"