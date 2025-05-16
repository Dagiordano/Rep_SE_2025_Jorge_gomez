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
| 240 MHz        | 2.98   |
| 160 MHz        | 2.79   |
| 80 MHz         | 2.51   |

El sistema es capaz de procesar entre 2.51 y 2.98 frames por segundo, dependiendo de la frecuencia del CPU.

## 3. Análisis del Cuello de Botella

Distribución del tiempo de procesamiento a 80 MHz:

| Operación            | Tiempo (ms) | Porcentaje |
|----------------------|-------------|------------|
| Captura de imagen    | 0.55        | 0.1%       |
| Ecualización histograma | 3.02     | 0.8%       |
| Filtro Sobel         | 5.53        | 1.4%       |
| Guardado en memoria  | 4.39        | 1.1%       |
| **Total**            | **388.20**  | **100%**   |

La suma de estas operaciones representa apenas el 3.4% del tiempo total de procesamiento por frame. El tiempo restante (96.6%) corresponde a:
- Transferencia de datos entre la cámara y el ESP32
- Operaciones de memoria (asignación/liberación)
- Escalado de imagen
- Operaciones del sistema operativo FreeRTOS

El cuello de botella no está en ninguna de las operaciones medidas específicamente, sino en la transferencia de datos y manejo general de memoria.

## 4. Potencia y Energía

A 240 MHz:
- Potencia estimada: 370.00 mW
- Energía por frame: 0.031033 mWh

A 160 MHz:
- Potencia estimada: 330.00 mW
- Energía por frame: 0.029569 mWh

A 80 MHz:
- Potencia estimada: 290.00 mW
- Energía por frame: 0.032060 mWh

## 5. Rendimiento vs Frecuencia

### FPS vs Frecuencia

| Frecuencia CPU | FPS    |
|----------------|--------|
| 240 MHz        | 2.98   |
| 160 MHz        | 2.79   |
| 80 MHz         | 2.51   |

El rendimiento en términos de FPS disminuye gradualmente al reducir la frecuencia, pero la reducción no es proporcional a la reducción de frecuencia, indicando que el sistema está limitado por otros factores como la velocidad de la cámara o la transferencia de datos.

### Energía vs Frecuencia

| Frecuencia CPU | Energía por frame (mWh) |
|----------------|--------------------|
| 240 MHz        | 0.031033           |
| 160 MHz        | 0.029569           |
| 80 MHz         | 0.032060           |

La energía por frame es más eficiente a 160 MHz, mientras que tanto 240 MHz como 80 MHz muestran un consumo ligeramente mayor.

## 6. Requisitos de Batería para 10 Días

### A 240 MHz:
- Energía total requerida: 88800.00 Wh
- Capacidad de batería: 24000000.00 mAh a 3.7V

### A 160 MHz:
- Energía total requerida: 79200.00 Wh
- Capacidad de batería: 21405404.00 mAh a 3.7V

### A 80 MHz:
- Energía total requerida: 69599.99 Wh
- Capacidad de batería: 18810808.00 mAh a 3.7V

Estos cálculos asumen operación continua a máximo FPS durante 10 días, lo cual requeriría una batería extremadamente grande. En un caso real, se necesitaría implementar ciclos de sueño y estrategias de bajo consumo.

## 7. Medida de Desempeño (FPS/W)

| Frecuencia CPU | FPS/W  |
|----------------|--------|
| 240 MHz        | 8.04   |
| 160 MHz        | 8.46   |
| 80 MHz         | 8.66   |

**Configuración óptima: 80 MHz con 8.66 FPS/W**

Esta configuración proporciona el mejor equilibrio entre rendimiento y consumo de energía. La frecuencia de 80 MHz es la más eficiente, ofreciendo un rendimiento aceptable con el menor consumo de energía. Aunque el FPS es más bajo que en las otras configuraciones, la eficiencia energética es significativamente mejor.
