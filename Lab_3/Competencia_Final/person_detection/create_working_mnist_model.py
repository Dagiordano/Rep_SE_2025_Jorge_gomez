import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import numpy as np

print("TensorFlow version:", tf.__version__)

# --- 1. Cargar y preprocesar los datos MNIST ---
(x_train, y_train), (x_test, y_test) = keras.datasets.mnist.load_data()

# Normalizar las imágenes a un rango de [0, 1]
x_train = x_train.astype("float32") / 255.0
x_test = x_test.astype("float32") / 255.0

# Añadir una dimensión de canal
x_train = x_train.reshape(-1, 28, 28, 1)
x_test = x_test.reshape(-1, 28, 28, 1)

# NO usar one-hot encoding, usar sparse categorical crossentropy
# y_train y y_test se mantienen como enteros (0-9)

print(f"Training samples: {x_train.shape[0]}")
print(f"Test samples: {x_test.shape[0]}")
print(f"Image shape: {x_train.shape[1:]}")

# --- 2. Modelo SIMPLE pero FUNCIONAL para ESP32 ---
model = keras.Sequential([
    # Una sola capa convolucional
    layers.Conv2D(16, kernel_size=(3, 3), activation="relu", input_shape=(28, 28, 1)),
    layers.MaxPooling2D(pool_size=(2, 2)),
    
    # Aplanar
    layers.Flatten(),

    # Una capa densa
    layers.Dense(64, activation="relu"),
    
    # Capa de salida CON softmax
    layers.Dense(10, activation="softmax"),
])

# --- 3. Compilar con sparse categorical crossentropy ---
model.compile(
    optimizer="adam",
    loss="sparse_categorical_crossentropy",  # Esto es clave
    metrics=["accuracy"],
)

# Mostrar arquitectura del modelo
model.summary()

# --- 4. Entrenar el modelo ---
print("\nEntrenando el modelo funcional para ESP32...")
history = model.fit(
    x_train,
    y_train,  # Sin one-hot encoding
    batch_size=128,
    epochs=3,  # Solo 3 épocas para rapidez
    validation_split=0.1,
    verbose=1
)

# --- 5. Evaluar el modelo ---
print("\nEvaluando el modelo...")
test_loss, test_acc = model.evaluate(x_test, y_test, verbose=0)
print(f"Precisión en el conjunto de prueba: {test_acc:.4f}")

# Solo proceder si el modelo tiene buena precisión
if test_acc < 0.85:
    print("❌ Modelo no tiene suficiente precisión, entrenando más...")
    history = model.fit(
        x_train, y_train,
        batch_size=128,
        epochs=2,
        validation_split=0.1,
        verbose=1
    )
    test_loss, test_acc = model.evaluate(x_test, y_test, verbose=0)
    print(f"Nueva precisión: {test_acc:.4f}")

# --- 6. Convertir a TensorFlow Lite (CON quantización int8) ---
print("\nConvirtiendo a TensorFlow Lite INT8...")

def representative_data_gen():
    for i in range(100):
        img = x_train[i:i+1]
        yield [img.astype(np.float32)]

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_data_gen
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type = tf.int8
converter.inference_output_type = tf.int8

tflite_model_int8 = converter.convert()

with open('main/mnist_model_int8.tflite', 'wb') as f:
    f.write(tflite_model_int8)

print(f"✅ Modelo INT8 guardado como 'main/mnist_model_int8.tflite'")
print(f"📏 Tamaño del modelo INT8: {len(tflite_model_int8)} bytes")

# --- 7. Probar el modelo TFLite INT8 y mostrar parámetros de cuantización ---
print("\nProbando el modelo TFLite INT8...")
interpreter_int8 = tf.lite.Interpreter(model_content=tflite_model_int8)
interpreter_int8.allocate_tensors()
input_details_int8 = interpreter_int8.get_input_details()
output_details_int8 = interpreter_int8.get_output_details()

print("Detalles de entrada INT8:")
print(f"  Shape: {input_details_int8[0]['shape']}")
print(f"  Type: {input_details_int8[0]['dtype']}")
print(f"  Quantization: {input_details_int8[0]['quantization']}")

print("Detalles de salida INT8:")
print(f"  Shape: {output_details_int8[0]['shape']}")
print(f"  Type: {output_details_int8[0]['dtype']}")
print(f"  Quantization: {output_details_int8[0]['quantization']}")

# Probar con varias imágenes
correct_predictions_int8 = 0
total_tests_int8 = 10

for i in range(total_tests_int8):
    test_image = x_test[i:i+1].astype(np.float32)
    # Quantize input
    scale, zero_point = input_details_int8[0]['quantization']
    test_image_q = np.round(test_image / scale + zero_point).astype(np.int8)
    interpreter_int8.set_tensor(input_details_int8[0]['index'], test_image_q)
    interpreter_int8.invoke()
    output_data = interpreter_int8.get_tensor(output_details_int8[0]['index'])
    predicted_class = np.argmax(output_data[0])
    actual_class = y_test[i]
    if predicted_class == actual_class:
        correct_predictions_int8 += 1
    if i < 5:
        confidence = output_data[0][predicted_class]
        print(f"  Prueba {i+1}: Real={actual_class}, Predicho={predicted_class}, Valor bruto={confidence}, {'✅' if predicted_class == actual_class else '❌'}")

print(f"\nPrecisión del modelo TFLite INT8: {correct_predictions_int8}/{total_tests_int8} = {correct_predictions_int8/total_tests_int8*100:.1f}%")

print("\n🎯 ¡Modelo funcional listo para ESP32!")
print("💡 Siguiente paso: ejecutar ./convert_model.sh main/mnist_model_int8.tflite")
print("📱 Este modelo debería funcionar sin problemas de cuantización en ESP32") 