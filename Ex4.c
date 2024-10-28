#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"

// Definindo a tag para logs
static const char *TAG = "uart_events";

// Definindo parâmetros de configuração da UART
#define EX_UART_NUM UART_NUM_0
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

// Declaração da fila de eventos UART
static QueueHandle_t uart0_queue;

// Estrutura para o protocolo de dados
typedef struct 
{
    uint8_t command;
    int32_t payload;
    uint8_t crc;
} protocol_t;

// Função de calculo de CRC-8 utilizando polinômio 0x07
static uint8_t calculate_crc(uint8_t *data, int length) 
{
    uint8_t crc = 0;
    for (int i = 0; i < length; i++) 
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) 
        {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
        }
    }
    crc = 'F';
    
    return crc;
}

// Executa um comando com base no valor de 'command' e imprime o payload
static void execute_command(protocol_t msg) {
    switch (msg.command) 
    {
        case 'A':
            ESP_LOGI(TAG,"Executando comando A com payload: %d\n", (int)msg.payload);
            break;
        case 'B':
            ESP_LOGI(TAG,"Executando comando B com payload: %d\n", (int)msg.payload);
            break;
        default:
            ESP_LOGI(TAG,"Comando desconhecido: %c\n", msg.command);
            break;
    }
}

// Função para interpretar uma mensagem recebida pela UART
void parse_message(uint8_t *data, int length) {
    if (length < 3) // Deve conter ao menos comando, payload e CRC
    { 
        ESP_LOGI(TAG,"Tamanho da mensagem invalido.\n");
        return;
    }

    uint8_t received_crc = data[length - 1];
    if (calculate_crc(data, length - 1) != received_crc) // Verificação do CRC
    { 
        ESP_LOGI(TAG,"CRC invalido.\n");
        return;
    }

    protocol_t msg;
    msg.command = data[0];
    msg.payload = 0;

    // Definindo ponto de início do payload (considerando sinal negativo)
    int start = (data[1] == '-') ? 2 : 1;

    for (int i = start; i < length - 1; i++) // Ignora último byte (CRC)
    {
        msg.payload = msg.payload * 10 + (data[i] - '0');
    }

    if (start == 2) // Aplicando sinal negativo, se necessario
    { 
        msg.payload = -msg.payload;
    }

    execute_command(msg);
}

// Função da tarefa que gerencia eventos UART
static void uart_event_task(void *pvParameters) 
{
    uart_event_t event;
    uint8_t dtmp[RD_BUF_SIZE];

    for (;;) 
    {
        if (xQueueReceive(uart0_queue, (void *)&event, portMAX_DELAY)) 
        {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "uart[%d] event:", EX_UART_NUM);

            switch (event.type) {
                case UART_DATA:
                    ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                    uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);

                    parse_message(dtmp, event.size);
                    uart_write_bytes(EX_UART_NUM, (const char *)dtmp, event.size - 1); // Ignora o CRC no echo
                    break;
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "Overflow do FIFO de hardware");
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart0_queue);
                    break;
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "Buffer da UART cheio");
                    uart_flush_input(EX_UART_NUM);
                    xQueueReset(uart0_queue);
                    break;
                default:
                    ESP_LOGI(TAG, "Evento UART desconhecido: %d", event.type);
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

// Função principal para configurar a UART e iniciar a tarefa de eventos
void app_main(void) 
{
	//A1234F
	//A1234D
	//A-1234F
	
    esp_log_level_set(TAG, ESP_LOG_INFO);

    uart_config_t uart_config = 
    {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);
    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(uart_event_task, "uart_event_task", 3072, NULL, 12, NULL);
}
