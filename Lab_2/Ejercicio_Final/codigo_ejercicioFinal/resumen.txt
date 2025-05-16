# Resultados del Análisis de Rendimiento ESP32-CAM

## 1. Imágenes Consecutivas

Las imágenes fueron capturadas, procesadas y almacenadas en PSRAM. Para cada imagen se realizó:
- Captura a través de la cámara OV2640
- Escalado a 320x240 para optimizar el uso de memoria
- Aplicación de ecualización de histograma
- Aplicación de filtro Sobel para detección de bordes

El programa almacena en memoria un buffer circular de 10 imágenes procesadas.

## 2. Frames Por Segundo (FPS)

El rendimiento en FPS varía según la frecuencia del CPU:

| Frecuencia CPU | FPS    |
|----------------|--------|
| 240 MHz        | 3.87   |
| 160 MHz        | 3.85   |

El sistema es capaz de procesar aproximadamente 3.85-3.87 frames por segundo.

## 3. Análisis del Cuello de Botella

Distribución del tiempo de procesamiento a 240 MHz:

| Operación            | Tiempo (ms) | Porcentaje |
|----------------------|-------------|------------|
| Captura de imagen    | 0.19        | 0.1%       |
| Ecualización histograma | 2.73     | 1.1%       |
| Filtro Sobel         | 5.26        | 2.1%       |
| Guardado en memoria  | 0.30        | 0.1%       |
| **Total**            | **248.83**  | **100%**   |

La suma de estas operaciones representa apenas el 3.4% del tiempo total de procesamiento por frame. El tiempo restante (96.6%) corresponde a:
- Transferencia de datos entre la cámara y el ESP32
- Operaciones de memoria (asignación/liberación)
- Escalado de imagen
- Operaciones del sistema operativo FreeRTOS

El cuello de botella no está en ninguna de las operaciones medidas específicamente, sino en la transferencia de datos y manejo general de memoria.

## 4. Potencia y Energía

A 240 MHz:
- Potencia estimada: 370.00 mW
- Energía por frame: 0.026582 mWh

A 160 MHz:
- Potencia estimada: 330.00 mW
- Energía por frame: 0.023833 mWh

## 5. Rendimiento vs Frecuencia

### FPS vs Frecuencia

| Frecuencia CPU | FPS    |
|----------------|--------|
| 240 MHz        | 3.87   |
| 160 MHz        | 3.85   |

El rendimiento en términos de FPS prácticamente no varía al reducir la frecuencia de 240 MHz a 160 MHz, indicando que el sistema está limitado por otros factores como la velocidad de la cámara o la transferencia de datos, no por la velocidad del CPU.

### Energía vs Frecuencia

| Frecuencia CPU | Energía por frame (mWh) |
|----------------|--------------------|
| 240 MHz        | 0.026582           |
| 160 MHz        | 0.023833           |

Reducir la frecuencia disminuye el consumo de energía por frame en aproximadamente 10.3%.

## 6. Requisitos de Batería para 10 Días

### A 240 MHz:
- Energía total requerida: 88800.00 Wh
- Capacidad de batería: 24000000.00 mAh a 3.7V

### A 160 MHz:
- Energía total requerida: 79200.00 Wh
- Capacidad de batería: 21405404.00 mAh a 3.7V

Estos cálculos asumen operación continua a máximo FPS durante 10 días, lo cual requeriría una batería extremadamente grande. En un caso real, se necesitaría implementar ciclos de sueño y estrategias de bajo consumo.

## 7. Medida de Desempeño (FPS/W)

| Frecuencia CPU | FPS/W  |
|----------------|--------|
| 240 MHz        | 10.45  |
| 160 MHz        | 11.66  |

**Configuración óptima: 160 MHz con 11.66 FPS/W**

Esta configuración proporciona el mejor equilibrio entre rendimiento y consumo de energía. La frecuencia de 160 MHz es la más eficiente, ofreciendo casi el mismo rendimiento que 240 MHz pero con un consumo de energía significativamente menor.
