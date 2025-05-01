#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "math.h"


#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28 // GPIO para o voltímetro
#define Botao_A 5  // GPIO para botão A

// Resistor de 670(680 é o valor comercial) conseguiu medir de 510ohms de 10k dentro dos 5% de tolerância
// Resistor de 67k(68k é o valor comercial) conseguiu medir de 47k a 100k respeitando os 5% de tolerância
// 661 67.000 valores reais dos resistores usados para medir 9 resistores na faixa de 510 a 100000

int R_conhecido = 67000;   // Resistor de 10k ohm medido com múltimetro deu este valor real
float R_x = 0.0;           // Resistor desconhecido
float ADC_VREF = 3.3;     // Tensão de referência do ADC
int ADC_RESOLUTION = 4095; // Resolução do ADC (12 bits)
char *faixas[3];          //Armazena a sequência de faixas correspondente ao resistor que estamos medindo


// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
  reset_usb_boot(0, 0);

  
}

// Valores comercias da tabela E24
const float E24[] = {10, 11, 12, 13, 15, 16, 18, 20, 22, 24, 27, 30,
    33, 36, 39, 43, 47, 51, 56, 62, 68, 75, 82, 91};



// Encontra o valor comercial mais proximo da tabela E24 baseado nas potências e menor diferença encontrada
float encontrar_E24_mais_proximo(float Rx) {
    float potencias[] = {1, 10, 100, 1e3, 1e4, 1e5, 1e6};
    float melhor = 0;
    float menor_diferenca = 1e9;

    for (int i = 0; i < sizeof(potencias)/sizeof(float); i++) {
        for (int j = 0; j < 24; j++) {
            float valor = E24[j] * potencias[i];
            float diferenca = fabsf(Rx - valor);
            if (diferenca < menor_diferenca) {
                menor_diferenca = diferenca;
                melhor = valor;
            }
        }
    }
    return melhor;
}
    

// Array para nomes das faixas de cores
const char* cores[] = {
    "Preto", "Marrom", "Vermelho", "Laranja", "Amarelo",
    "Verde", "Azul", "Violeta", "Cinza", "Branco"
};

// Calculo das faixas baseado na normalização da resistencia entre 10 e 99 e no multiplicador
void calcular_faixas(float resistencia, char** faixas) {
    int multiplicador = 0;

    // Normaliza resistência para a faixa entre 10 e 99
    while (resistencia >= 100) {
        resistencia /= 10.0;// 51
        multiplicador++; // 1
    }
    while (resistencia < 10) {
        resistencia *= 10.0;
        multiplicador--;
    }

    // Arredonda para 2 dígitos significativos
    int valor = (int)(resistencia + 0.5); // 51.5

    if (valor >= 100) {
        valor /= 10;
        multiplicador++;
    }

    int primeiro = valor / 10;
    int segundo = valor % 10;

    if (primeiro > 9 || segundo > 9 || multiplicador > 9 || multiplicador < 0) {
        faixas[0] = "Erro";
        faixas[1] = "Erro";
        faixas[2] = "Erro";
        return;
    }

    faixas[0] = (char*)cores[primeiro];
    faixas[1] = (char*)cores[segundo];
    faixas[2] = (char*)cores[multiplicador];
}




int main()
{
    stdio_init_all();

    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

     // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_t ssd;                                                // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    adc_init();
    adc_gpio_init(ADC_PIN); // GPIO 28 como entrada analógica

        
    char str_x[5]; // Buffer para armazenar a string
    char str_y[5]; // Buffer para armazenar a string


    bool cor = true;
    while (true){
        adc_select_input(2); // GPIO 28
        float soma = 0.0f;
        for (int i = 0; i < 500; i++) {
            soma += adc_read();
            sleep_ms(1);
        }
        float media = soma / 500.0f;
        
        // Conversão ADC -> tensão
        float Vadc = (media / ADC_RESOLUTION) * ADC_VREF;
        
        // Cálculo corrigido de R_x
        R_x = R_conhecido * Vadc / (ADC_VREF - Vadc);
        

        float R_comercial = encontrar_E24_mais_proximo(R_x);
        printf("%f",R_comercial);
        calcular_faixas(R_comercial, faixas);

        sprintf(str_x, "%1.0f", media); // Converte o inteiro em string
        sprintf(str_y, "%1.0f", R_x);   // Converte o float em string

        // cor = !cor;
        //  Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor);                          // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);      // Desenha um retângulo
        //ssd1306_line(&ssd, 3, 25, 123, 25, cor);           // Desenha uma linha
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);           // Desenha uma linha
        ssd1306_draw_string(&ssd, faixas[0], 40, 6); // Desenha uma string
        ssd1306_draw_string(&ssd, faixas[1], 40, 16);  // Desenha uma string
        ssd1306_draw_string(&ssd, faixas[2], 40, 28);  // Desenha uma string
        ssd1306_draw_string(&ssd, "ADC", 13, 41);          // Desenha uma string
        ssd1306_draw_string(&ssd, "Ohms", 70, 41);    // Desenha uma string
        ssd1306_line(&ssd, 44, 37, 44, 60, cor);           // Desenha uma linha vertical
        ssd1306_draw_string(&ssd, str_x, 8, 52);           // Desenha uma string
        ssd1306_draw_string(&ssd, str_y, 70, 52);          // Desenha uma string
        ssd1306_send_data(&ssd);                           // Atualiza o display
        sleep_ms(500);
    }
}
