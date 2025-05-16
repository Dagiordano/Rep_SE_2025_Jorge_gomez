import serial
import time

print("Intentando conectar al Arduino...")
try:
    # Intenta conectar al puerto serial
    ser = serial.Serial('/dev/cu.usbserial-110', 9600, timeout=1)
    print("Conexión exitosa!")
    
    # Escucha por 10 segundos
    print("Escuchando datos por 10 segundos:")
    start_time = time.time()
    
    while time.time() - start_time < 10:
        if ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            print(f"Recibido: {line}")
    
    # Cierra la conexión
    ser.close()
    print("Conexión cerrada")
    
except Exception as e:
    print(f"Error: {e}") 