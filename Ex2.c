#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

// Define o GPIO utilizado para a entrada e flag de interrupção padrão
#define GPIO_INPUT_IO_0     5
#define ESP_INTR_FLAG_DEFAULT 0

// Define valores booleanos
#define FALSE 0
#define TRUE 1

// Macro para conversão de milissegundos em microssegundos
#define DEBOUNCE_MS(ms) ((ms) * 1000)

static QueueHandle_t gpio_evt_queue = NULL; // Fila para eventos de GPIO

// Enumeração dos estados possíveis para a máquina de estados (FSM)
typedef enum
{
    OFF,
    ON,
    PROTECTED,
} state;

// Estrutura para armazenar dados da FSM
typedef struct
{
    state state;             // Estado atual
    state old_state;         // Estado anterior
    state target;            // Estado alvo
    uint8_t condition;       // Condição de transição
    uint64_t debounce;       // Tempo de debounce em microssegundos
    uint64_t time_to_transition; // Tempo para próxima transição
} fsm_t;

// Função de tratamento de interrupção ISR para o GPIO
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg; // Obtém o número do GPIO como argumento
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL); // Envia o número do GPIO para a fila
}

// Processa as transições de estado da FSM
static void process_fsm(fsm_t* this)
{
    // Verifica se o estado mudou; se sim, registra o tempo de transição
    if (this->old_state != this->state)
    {
        this->old_state = this->state;
        this->time_to_transition = esp_timer_get_time();
    }

    // Verifica a condição de transição
    if (this->condition)
    {
        // Se o tempo de debounce tiver passado, realiza a transição
        if (esp_timer_get_time() >= this->time_to_transition + this->debounce)
        {
            this->state = this->target;
            this->time_to_transition = esp_timer_get_time(); // Atualiza o tempo de transição
        }
    }
    else
    {
        this->time_to_transition = esp_timer_get_time(); // Reseta o tempo de transição se a condição não for atendida
    }

    printf("state:%d\n", this->state); // Exibe o estado atual
    vTaskDelay(10 / portTICK_PERIOD_MS); // Aguarda por 10ms
}

// Atualiza as condições da FSM com base no estado do GPIO
static void update_fsm_conditions(fsm_t* fsm, uint8_t gpio_level)
{
    switch (fsm->state)
    {
        case OFF:
            fsm->condition = !gpio_level;        // Condição: nível baixo no GPIO
            fsm->debounce = DEBOUNCE_MS(300);    // Tempo de debounce para o estado OFF -> ON
            fsm->target = ON;                    // Define o estado alvo como ON
            break;

        case ON:
            fsm->condition = gpio_level;         // Condição: nível alto no GPIO
            fsm->debounce = DEBOUNCE_MS(300);    // Tempo de debounce para o estado ON -> PROTECTED
            fsm->target = PROTECTED;             // Define o estado alvo como PROTECTED
            break;

        case PROTECTED:
            fsm->condition = 1;                  // Condição constante (sempre verdadeira)
            fsm->debounce = DEBOUNCE_MS(10000);  // Tempo de debounce para o estado PROTECTED -> OFF
            fsm->target = OFF;                   // Define o estado alvo como OFF
            break;

        default:
            fsm->condition = 1;                  // Condição constante (sempre verdadeira)
            fsm->debounce = DEBOUNCE_MS(1000);   // Tempo de debounce padrão
            fsm->target = OFF;                   // Define o estado alvo como OFF
            break;
    }
}

// Função principal da FSM
static void simple_fsm(void* arg)
{
    static uint32_t io_num;
    // Inicializa a estrutura da FSM com o estado OFF e debounce de 300ms
    static fsm_t local_fsm = {OFF, OFF, OFF, 0, DEBOUNCE_MS(300), 0};

    while (1)
    {
        // Recebe eventos da fila de interrupção de GPIO com timeout de 10ms
        if (xQueueReceive(gpio_evt_queue, &io_num, pdMS_TO_TICKS(10)))
        {
            // printf("GPIO[%"PRIu32"] interrupt, val: %d\n", io_num, gpio_get_level(io_num));
        }

        // Atualiza as condições da FSM de acordo com o nível do GPIO
        update_fsm_conditions(&local_fsm, gpio_get_level(io_num));
        
        // Processa a FSM para verificar se uma transição de estado é necessária
        process_fsm(&local_fsm);
        
    }
}

// Função principal da aplicação, configuração de GPIO e criação de tarefas
void app_main(void)
{
    gpio_config_t io_conf = {};

    // Configuração de GPIO para entrada e interrupção de borda de descida
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_IO_0;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 1;
    gpio_config(&io_conf);

    // Configura interrupção para qualquer borda
    gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    // Cria uma fila para os eventos de interrupção do GPIO
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    
    // Cria uma tarefa para executar a FSM simples
    xTaskCreate(simple_fsm, "simple_fsm", 2048, NULL, 10, NULL);

    // Instala o serviço de interrupção e registra o manipulador de interrupção para o GPIO
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);
    
    while (1) {
        // Exemplo de loop principal (para depuração)
        //static int cnt = 0;
        //printf("cnt: %d\n", cnt++);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Aguarda 1 segundo
    }
}
