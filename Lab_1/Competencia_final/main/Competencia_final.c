#include <stdio.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "esp_dsp.h"
#include "esp_heap_caps.h"

#define NFFT 1024
#define NOVERLAP 512
#define SAMPLE_RATE 16000
#define TAG "SPECTROGRAM"

// Variables globales
static float *audio_data = NULL;
static float *spectrogram = NULL;
static float *fft_input = NULL;
static uint64_t start_cycles = 0;
static uint64_t end_cycles = 0;

// Declaraciones de funciones
static esp_err_t init_spiffs(void);
static esp_err_t read_audio_data(void);
static esp_err_t generate_spectrogram(void);
static esp_err_t save_spectrogram(void);
static void cleanup_resources(void);
static esp_err_t cleanup_spiffs_files(void);

void app_main(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Starting application");
    
    // Inicializar SPIFFS
    ret = init_spiffs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS");
        return;
    }
    
    // Leer datos de audio
    ret = read_audio_data();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read audio data");
        cleanup_resources();
        return;
    }
    
    // Generar espectrograma
    ret = generate_spectrogram();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate spectrogram");
        cleanup_resources();
        return;
    }
    
    // Guardar espectrograma
    ret = save_spectrogram();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save spectrogram");
    }
    
    // Limpiar recursos
    cleanup_resources();
}

static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Inicializando SPIFFS");
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    
    // Si tenemos menos de 20% de espacio libre, intenta limpiar
    if ((total - used) < (total / 5)) {
        ESP_LOGW(TAG, "Espacio en disco bajo, intentando limpiar");
        ret = cleanup_spiffs_files();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to clean up files");
            return ret;
        }
        
        // Verificar espacio nuevamente después de la limpieza
        ret = esp_spiffs_info(NULL, &total, &used);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get SPIFFS info after cleanup");
            return ret;
        }
        
        // Si aún falta espacio, formatear la partición
        if ((total - used) < (total / 5)) {
            ESP_LOGW(TAG, "Aún falta espacio, formateando SPIFFS");
            esp_vfs_spiffs_unregister(NULL);
            ret = esp_vfs_spiffs_register(&conf);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to format SPIFFS");
                return ret;
            }
        }
    }
    
    return ESP_OK;
}

static esp_err_t read_audio_data(void)
{
    ESP_LOGI(TAG, "Reading audio data");
    
    FILE *f = fopen("/spiffs/audio.wav", "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open audio file");
        return ESP_FAIL;
    }

    // Saltar encabezado WAV
    fseek(f, 44, SEEK_SET);

    // Asignar memoria para datos de audio
    audio_data = heap_caps_aligned_alloc(16, NFFT * sizeof(float), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    if (audio_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio data");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    // Leer datos de audio sin procesar
    int16_t *raw_audio = heap_caps_aligned_alloc(16, NFFT * sizeof(int16_t), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    if (raw_audio == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for raw audio");
        heap_caps_free(audio_data);
        audio_data = NULL;
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t read = fread(raw_audio, sizeof(int16_t), NFFT, f);
    fclose(f);

    if (read != NFFT) {
        ESP_LOGE(TAG, "Failed to read audio data (read %d samples)", read);
        heap_caps_free(raw_audio);
        heap_caps_free(audio_data);
        audio_data = NULL;
        return ESP_FAIL;
    }

    // Convertir a float sin normalizar para mantener la amplitud
    float max_val = 0.0f;
    float min_val = 0.0f;
    for (int i = 0; i < NFFT; i++) {
        audio_data[i] = (float)raw_audio[i];
        max_val = fmaxf(max_val, audio_data[i]);
        min_val = fminf(min_val, audio_data[i]);
    }
    ESP_LOGI(TAG, "Audio data range: min=%f, max=%f", min_val, max_val);

    heap_caps_free(raw_audio);
    ESP_LOGI(TAG, "Successfully read audio data");
    return ESP_OK;
}

static esp_err_t generate_spectrogram(void)
{
    ESP_LOGI(TAG, "Generating spectrogram");
    
    if (audio_data == NULL) {
        ESP_LOGE(TAG, "Audio data not available");
        return ESP_FAIL;
    }

    // Inicializar biblioteca DSP
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, NFFT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DSP library");
        return ret;
    }

    // Asignar memoria para espectrograma
    spectrogram = heap_caps_aligned_alloc(16, (NFFT/2 + 1) * sizeof(float), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    if (spectrogram == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for spectrogram");
        return ESP_ERR_NO_MEM;
    }

    // Asignar memoria para FFT input
    fft_input = heap_caps_aligned_alloc(16, NFFT * 2 * sizeof(float), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    if (fft_input == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for FFT input");
        heap_caps_free(spectrogram);
        spectrogram = NULL;
        return ESP_ERR_NO_MEM;
    }

    // Crear ventana Hamming (como en Arduino)
    float *window = heap_caps_aligned_alloc(16, NFFT * sizeof(float), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    if (window == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for window function");
        heap_caps_free(fft_input);
        heap_caps_free(spectrogram);
        fft_input = NULL;
        spectrogram = NULL;
        return ESP_ERR_NO_MEM;
    }

    // Crear ventana Hamming sin normalización
    for (int i = 0; i < NFFT; i++) {
        window[i] = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (NFFT - 1));
    }

    // Aplicar ventana y preparar FFT input
    for (int i = 0; i < NFFT; i++) {
        fft_input[i * 2] = audio_data[i] * window[i];  // Parte real
        fft_input[i * 2 + 1] = 0.0f;                   // Parte imaginaria
    }

    heap_caps_free(window);

    start_cycles = esp_cpu_get_cycle_count();

    // Computar FFT
    ret = dsps_fft2r_fc32(fft_input, NFFT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FFT computation failed");
        return ret;
    }

    // Reversión de bits
    ret = dsps_bit_rev_fc32(fft_input, NFFT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bit reversal failed");
        return ret;
    }

    // Convertir a espectro de magnitud sin escalar por NFFT
    for (int i = 0; i < NFFT/2 + 1; i++) {
        float real = fft_input[i * 2];
        float imag = fft_input[i * 2 + 1];
        spectrogram[i] = sqrtf(real * real + imag * imag);
    }

    end_cycles = esp_cpu_get_cycle_count();
    
    // Log some statistics
    float spec_max = 0.0f;
    float spec_min = INFINITY;
    for (int i = 0; i < NFFT/2 + 1; i++) {
        spec_max = fmaxf(spec_max, spectrogram[i]);
        spec_min = fminf(spec_min, spectrogram[i]);
    }
    ESP_LOGI(TAG, "Spectrogram range: min=%f, max=%f", spec_min, spec_max);
    
    ESP_LOGI(TAG, "Spectrogram generation completed");
    return ESP_OK;
}

static esp_err_t save_spectrogram(void)
{
    ESP_LOGI(TAG, "Saving spectrogram");
    
    if (spectrogram == NULL) {
        ESP_LOGE(TAG, "Spectrogram data not available");
        return ESP_FAIL;
    }

    // Check SPIFFS status
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS info: %s", esp_err_to_name(ret));
        return ret;
    }
    
    size_t free_space = total - used;
    size_t required_space = (NFFT/2 + 1) * 20; // Approximate space needed (20 bytes per line)
    
    ESP_LOGI(TAG, "SPIFFS: Total: %d bytes, Used: %d bytes, Free: %d bytes, Required: %d bytes", 
             total, used, free_space, required_space);
    
    if (free_space < required_space) {
        ESP_LOGE(TAG, "Not enough space in SPIFFS. Need %d bytes, have %d bytes", 
                 required_space, free_space);
        return ESP_ERR_NO_MEM;
    }

    // First, try to remove the file if it exists
    remove("/spiffs/spectrogram.txt");

    // Create the file with write permissions
    FILE *f = fopen("/spiffs/spectrogram.txt", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open spectrogram file: %s", strerror(errno));
        return ESP_FAIL;
    }

    // Print header to serial monitor
    printf("\n===SPECTROGRAM_START===\n");
    
    // Save spectrogram data
    for (int i = 0; i < NFFT/2 + 1; i++) {
        // Print to serial monitor in a format easy to capture
        printf("%.6f\n", spectrogram[i]);
        
        // Save to file
        int written = fprintf(f, "%.6f\n", spectrogram[i]);
        if (written < 0) {
            ESP_LOGE(TAG, "Failed to write to spectrogram file at index %d: %s", i, strerror(errno));
            fclose(f);
            return ESP_FAIL;
        }
        // Flush after each write to ensure data is written
        if (fflush(f) != 0) {
            ESP_LOGE(TAG, "Failed to flush spectrogram file: %s", strerror(errno));
            fclose(f);
            return ESP_FAIL;
        }
    }
    
    printf("===SPECTROGRAM_END===\n");

    if (fclose(f) != 0) {
        ESP_LOGE(TAG, "Failed to close spectrogram file: %s", strerror(errno));
        return ESP_FAIL;
    }

    // Verify file was created and has content
    struct stat st;
    if (stat("/spiffs/spectrogram.txt", &st) != 0) {
        ESP_LOGE(TAG, "Failed to verify spectrogram file: %s", strerror(errno));
        return ESP_FAIL;
    }

    if (st.st_size == 0) {
        ESP_LOGE(TAG, "Spectrogram file is empty");
        return ESP_FAIL;
    }
    
    uint64_t total_cycles = end_cycles - start_cycles;
    ESP_LOGI(TAG, "Total cycles used: %llu", total_cycles);
    ESP_LOGI(TAG, "Spectrogram saved successfully, file size: %d bytes", (int)st.st_size);
    return ESP_OK;
}

static void cleanup_resources(void)
{
    if (audio_data) {
        heap_caps_free(audio_data);
        audio_data = NULL;
    }
    if (spectrogram) {
        heap_caps_free(spectrogram);
        spectrogram = NULL;
    }
    if (fft_input) {
        heap_caps_free(fft_input);
        fft_input = NULL;
    }
}

static esp_err_t cleanup_spiffs_files(void)
{
    ESP_LOGI(TAG, "Cleaning up SPIFFS files");
    
    // List of files to keep
    const char *keep_files[] = {"audio.wav"};
    const int num_keep_files = sizeof(keep_files) / sizeof(keep_files[0]);
    
    // Open directory
    DIR *dir = opendir("/spiffs");
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open SPIFFS directory: %s", strerror(errno));
        return ESP_FAIL;
    }
    
    struct dirent *entry;
    int files_deleted = 0;
    
    // Iterate through all files
    while ((entry = readdir(dir)) != NULL) {
        // Skip directories
        if (entry->d_type != DT_REG) {
            continue;
        }
        
        // Check filename length
        size_t name_len = strlen(entry->d_name);
        if (name_len > 128) {  // Reasonable limit for filename length
            ESP_LOGW(TAG, "Skipping file with too long name: %s", entry->d_name);
            continue;
        }
        
        // Check if this file should be kept
        bool should_keep = false;
        for (int i = 0; i < num_keep_files; i++) {
            if (strcmp(entry->d_name, keep_files[i]) == 0) {
                should_keep = true;
                break;
            }
        }
        
        if (!should_keep) {
            char full_path[512];  // Increased buffer size
            int written = snprintf(full_path, sizeof(full_path), "/spiffs/%s", entry->d_name);
            if (written >= sizeof(full_path)) {
                ESP_LOGW(TAG, "Path too long for file: %s", entry->d_name);
                continue;
            }
            
            if (remove(full_path) == 0) {
                files_deleted++;
                ESP_LOGI(TAG, "Deleted file: %s", entry->d_name);
            } else {
                ESP_LOGW(TAG, "Failed to delete file %s: %s", entry->d_name, strerror(errno));
            }
        }
    }
    
    closedir(dir);
    ESP_LOGI(TAG, "Deleted %d files", files_deleted);
    return ESP_OK;
}
