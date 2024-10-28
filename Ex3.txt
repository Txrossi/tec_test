#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Define macros para encontrar o mínimo e máximo entre dois valores
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

// Define o tamanho máximo do array que armazena números pares
#define MAX_EVEN_ARRAY_SIZE 100

// Estrutura para armazenar valores estatísticos
typedef struct
{
    int32_t max;   // Valor máximo
    int32_t min;   // Valor mínimo
    float avr;     // Média dos valores
} statistic;

// Estrutura para armazenar os dados do array e valores estatísticos
typedef struct 
{
    uint8_t is_valid; // Flag indicando se os dados são válidos
    
    statistic raw_array; // Estrutura com estatísticas do array bruto
    
    struct
    {
        int32_t array[MAX_EVEN_ARRAY_SIZE]; // Array para armazenar números pares
        int32_t qnt_elements;               // Quantidade de elementos pares armazenados
    } even;

} array_data;

// Função auxiliar que processa cada valor, atualizando os valores de min, max e somando para calcular a média
static void process_statistic(array_data* data, int32_t value)
{
    // Atualiza o mínimo e o máximo usando macros
    data->raw_array.min = MIN(data->raw_array.min, value);
    data->raw_array.max = MAX(data->raw_array.max, value);
    
    // Incrementa a soma para calcular a média posteriormente
    data->raw_array.avr += value;
}

// Função principal que calcula os valores estatísticos e identifica números pares
array_data func(int32_t* vec, size_t size)
{
    array_data return_value;
    
    // Inicializa a estrutura de retorno com zeros
    memset(&return_value, 0x00, sizeof(return_value));
    
    if (vec != NULL && size > 0) // Verifica se o array de entrada é válido
    {
        // Inicializa min e max com o primeiro elemento do array
        return_value.raw_array.max = vec[0];
        return_value.raw_array.min = vec[0];
        return_value.raw_array.avr = 0;
        
        // Percorre cada elemento do array de entrada
        for (uint16_t i = 0; i < size; i++)
        {
            // Processa o valor atual para atualizar min, max e soma
            process_statistic(&return_value, vec[i]);
            
            // Se o valor é par
            if (vec[i] % 2 == 0)
            {
                // E o array de pares ainda não atingiu o limite
                if(return_value.even.qnt_elements < MAX_EVEN_ARRAY_SIZE)
                {
                    // Adiciona o número par ao array e incrementa o contador de pares
                    return_value.even.array[return_value.even.qnt_elements++] = vec[i];
                }
                else
                {
                    return_value.is_valid = 0;
                    goto exit;
                }

            }
        }
        
        // Calcula a média dividindo a soma pelo tamanho do array
        return_value.raw_array.avr /= size;
        
        // Define a flag de validação como verdadeira
        return_value.is_valid = 1;
    }
    else
    {
        // Caso o array de entrada seja nulo, define a flag de validação como falsa
        return_value.is_valid = 0;
    }
exit:   
    return return_value;
}

int main()
{
    array_data resposta;
    
    // Array de entrada com valores de exemplo
    int32_t my_vec[] = {-2, 2, -3, 3};
    
    // Chama a função que processa o array e armazena o resultado em resposta
    resposta = func(my_vec, sizeof(my_vec) / sizeof(int32_t));
    
    // Exibe os valores de mínimo, máximo e média calculados
    printf("min=%d max=%d avr=%f \n", resposta.raw_array.min, resposta.raw_array.max, resposta.raw_array.avr);

    // Exibe todos os números pares encontrados
    for (int i = 0; i < resposta.even.qnt_elements; i++)
    {
        printf("%d ", resposta.even.array[i]);
    }
    return 0;
}
