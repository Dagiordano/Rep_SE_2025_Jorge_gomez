import serial as pyserial
import tkinter as tk
from collections import deque
import time

# Configuración
PORT = '/dev/cu.usbserial-110'
BAUD_RATE = 9600
MAX_POINTS = 500  # Cantidad de puntos a mostrar

class ECGFSRVisualizer:
    def __init__(self, root):
        self.root = root
        self.root.title("ECG y FSR Monitor")
        self.root.geometry("800x600")
        
        # Variables para almacenar datos
        self.ecg_data = deque([0] * MAX_POINTS, maxlen=MAX_POINTS)
        self.fsr_data = deque([0] * MAX_POINTS, maxlen=MAX_POINTS)
        
        # Información de estado
        self.status_frame = tk.Frame(root)
        self.status_frame.pack(fill=tk.X, padx=10, pady=5)
        
        self.ecg_status = tk.Label(self.status_frame, text="ECG: --", font=("Arial", 12))
        self.ecg_status.pack(side=tk.LEFT, padx=10)
        
        self.fsr_status = tk.Label(self.status_frame, text="FSR: --", font=("Arial", 12))
        self.fsr_status.pack(side=tk.LEFT, padx=10)
        
        self.pressure_status = tk.Label(self.status_frame, text="Presión: --", font=("Arial", 12))
        self.pressure_status.pack(side=tk.LEFT, padx=10)
        
        self.rate_status = tk.Label(self.status_frame, text="Freq. Compresión: -- BPM", font=("Arial", 12))
        self.rate_status.pack(side=tk.LEFT, padx=10)
        
        self.leads_status = tk.Label(self.status_frame, text="Leads: OK", font=("Arial", 12), fg="green")
        self.leads_status.pack(side=tk.LEFT, padx=10)
        
        # Canvas para ECG
        self.ecg_label = tk.Label(root, text="Señal ECG", font=("Arial", 14))
        self.ecg_label.pack(pady=(20,5))
        
        self.ecg_canvas = tk.Canvas(root, bg="white", height=200)
        self.ecg_canvas.pack(fill=tk.X, padx=10, pady=5)
        
        # Canvas para FSR
        self.fsr_label = tk.Label(root, text="Señal FSR (Presión)", font=("Arial", 14))
        self.fsr_label.pack(pady=(20,5))
        
        self.fsr_canvas = tk.Canvas(root, bg="white", height=200)
        self.fsr_canvas.pack(fill=tk.X, padx=10, pady=5)
        
        # Conexión serial
        try:
            self.ser = pyserial.Serial(PORT, BAUD_RATE, timeout=0.1)
            print(f"Conectado a {PORT}")
            
            # Iniciar actualización
            self.update_plot()
            
        except Exception as e:
            print(f"Error al conectar: {e}")
            error_label = tk.Label(root, text=f"Error: {e}", font=("Arial", 14), fg="red")
            error_label.pack(pady=20)
            
            available_ports = tk.Label(root, text="Puertos disponibles:", font=("Arial", 12))
            available_ports.pack(pady=(20,5))
            
            # Mostrar puertos disponibles
            import serial.tools.list_ports
            ports = serial.tools.list_ports.comports()
            for port in ports:
                port_label = tk.Label(root, text=f"  {port.device} - {port.description}", font=("Arial", 10))
                port_label.pack()
    
    def update_plot(self):
        # Leer datos seriales disponibles
        if hasattr(self, 'ser') and self.ser.is_open:
            while self.ser.in_waiting:
                try:
                    line = self.ser.readline().decode('utf-8', errors='replace').strip()
                    
                    if line.startswith('ECG:'):
                        value = int(line.split(':')[1])
                        self.ecg_data.append(value)
                        self.ecg_status.config(text=f"ECG: {value}")
                        
                    elif line.startswith('FSR:'):
                        value = int(line.split(':')[1])
                        self.fsr_data.append(value)
                        self.fsr_status.config(text=f"FSR: {value}")
                        
                    elif line.startswith('RATE:'):
                        rate = line.split(':')[1]
                        self.rate_status.config(text=f"Freq. Compresión: {rate} BPM")
                        
                    elif line.startswith('PRESSURE:'):
                        pressure = line.split(':')[1]
                        self.pressure_status.config(text=f"Presión: {pressure}")
                        
                    elif line == "!ECG_LEADS_OFF!":
                        self.leads_status.config(text="LEADS: ¡DESCONECTADO!", fg="red")
                    else:
                        self.leads_status.config(text="LEADS: OK", fg="green")
                        
                except Exception as e:
                    print(f"Error al procesar datos: {e}")
        
        # Dibujar gráficos
        self.draw_ecg()
        self.draw_fsr()
        
        # Programar próxima actualización
        self.root.after(50, self.update_plot)
    
    def draw_ecg(self):
        canvas = self.ecg_canvas
        canvas.delete("all")
        
        width = canvas.winfo_width()
        height = canvas.winfo_height()
        
        if width < 10:  # Evitar división por cero
            return
        
        # Encontrar mín y máx para escalar
        if self.ecg_data:
            min_val = min(self.ecg_data)
            max_val = max(self.ecg_data)
            
            # Evitar división por cero
            if max_val == min_val:
                max_val = min_val + 1
                
            # Dibujar línea
            points = []
            for i, value in enumerate(self.ecg_data):
                x = i * width / len(self.ecg_data)
                y = height - ((value - min_val) / (max_val - min_val) * height)
                points.append(x)
                points.append(y)
            
            if len(points) >= 4:  # Necesitamos al menos 2 puntos
                canvas.create_line(points, fill="red", width=2)
    
    def draw_fsr(self):
        canvas = self.fsr_canvas
        canvas.delete("all")
        
        width = canvas.winfo_width()
        height = canvas.winfo_height()
        
        if width < 10:  # Evitar división por cero
            return
        
        # Encontrar mín y máx para escalar
        if self.fsr_data:
            min_val = min(self.fsr_data)
            max_val = max(self.fsr_data)
            
            # Si min_val y max_val son iguales, añadir un pequeño valor para evitar división por cero
            if max_val == min_val:
                max_val = min_val + 1
                
            # Dibujar línea
            points = []
            for i, value in enumerate(self.fsr_data):
                x = i * width / len(self.fsr_data)
                y = height - ((value - min_val) / (max_val - min_val) * height)
                points.append(x)
                points.append(y)
            
            if len(points) >= 4:  # Necesitamos al menos 2 puntos
                canvas.create_line(points, fill="blue", width=2)

    def __del__(self):
        if hasattr(self, 'ser') and self.ser.is_open:
            self.ser.close()
            print("Conexión serial cerrada")

# Iniciar aplicación
if __name__ == "__main__":
    root = tk.Tk()
    app = ECGFSRVisualizer(root)
    root.mainloop() 