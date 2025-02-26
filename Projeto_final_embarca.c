#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"  
#include "hardware/pwm.h"  
#include "lib/ssd1306.h"
#include "lib/font.h"


#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define JOYSTICK_X_PIN 26  // GPIO para eixo X
#define JOYSTICK_Y_PIN 27  // GPIO para eixo Y
#define JOYSTICK_PB 22 // GPIO para botão do Joystick
#define Botao_A 5 // GPIO para botão A
#define LEDB_PIN 13  //GPIO led azul, no eixo y
#define LEDR_PIN 12  //GPIO led vermelho, no eixo x
#define BUZZER_PIN 10 //Buzzer acionamento em niveis altissimos de poluicao do ar

// Função para inicializar o PWM
uint pwm_init_gpio(uint gpio, uint wrap) {
    // Define o pino GPIO como saída de PWM
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    
    // Obtém o número do "slice" de PWM associado ao pino
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    
    // Define o "wrap" do PWM, que define o valor máximo para o contador
    // Isso afeta a resolução do PWM (quanto maior, maior a precisão do duty cycle)
    pwm_set_wrap(slice_num, wrap);
    
    // Habilita o PWM no slice
    pwm_set_enabled(slice_num, true);  
    return slice_num;  // Retorna o número do slice para uso posterior
}

// Função para configurar o PWM no pino do buzzer
void setup_pwm_on_buzzer(uint pin) {
  gpio_set_function(pin, GPIO_FUNC_PWM);  // Configurar o pino como saída de PWM
  uint slice_num = pwm_gpio_to_slice_num(pin);  // Obter o número do slice PWM
  pwm_set_wrap(slice_num, 255);  // Definir o valor máximo para o ciclo de trabalho
  pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);  // Inicializar com o PWM desligado
  pwm_set_enabled(slice_num, true);  // Habilitar PWM no pino
}

// Função para gerar um tom
void play_tone(uint pin, uint frequency) {
  uint slice_num = pwm_gpio_to_slice_num(pin);  // Obter o número do slice PWM
  uint period = 125000000 / frequency;  // Calcular o período do PWM para a frequência desejada
  pwm_set_wrap(slice_num, period);  // Ajustar o período do PWM
  pwm_set_chan_level(slice_num, PWM_CHAN_A, period * 3 / 4);  // Aumentar o ciclo de trabalho para 75%
}

//variaveis para a funcao gpio_irq_handler
volatile int contador = 0;  // Contador exibido
volatile uint32_t last_interrupt_time_A = 0;
volatile uint32_t last_interrupt_time_J = 0;

void gpio_irq_handler(uint gpio, uint32_t events){ // Função de interrupção para os botões com debounce
  uint32_t current_time = to_ms_since_boot(get_absolute_time());

  if (gpio == Botao_A) {
      if (current_time - last_interrupt_time_A >= 200) {  // Debounce 200ms
          contador++;
          printf("Botão A pressionado! Contador: %d\n", contador);
          last_interrupt_time_A = current_time;
      }
  } 
  else if (gpio == JOYSTICK_PB) {
      if (current_time - last_interrupt_time_J >= 200) {  // Debounce 200ms
          contador--;
          printf("Botão do JOYSTICK pressionado! Contador: %d\n", contador);
          last_interrupt_time_J = current_time;
      }
  }
}

//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler_B(uint gpio, uint32_t events)
{
  reset_usb_boot(0, 0);
}
//funcao auxiliar
void gpio_config(){
  //inicializa botao A e botao do analogico
  gpio_init(JOYSTICK_PB);
  gpio_set_dir(JOYSTICK_PB, GPIO_IN);
  gpio_pull_up(JOYSTICK_PB); 
  
  gpio_init(Botao_A);
  gpio_set_dir(Botao_A, GPIO_IN);
  gpio_pull_up(Botao_A);

  setup_pwm_on_buzzer(BUZZER_PIN);
  
  // Para ser utilizado o modo BOOTSEL com botão B
  gpio_init(botaoB);
  gpio_set_dir(botaoB, GPIO_IN);
  gpio_pull_up(botaoB);

  // Ativa as interrupções nos botões para chamar gpio_callback()
  gpio_set_irq_enabled_with_callback(Botao_A, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  gpio_set_irq_enabled_with_callback(JOYSTICK_PB, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler_B);
}

int main(){
    // Inicializa a comunicação serial para permitir o uso de printf
    // Isso permite enviar dados via USB para depuração no console
    stdio_init_all();
    
    gpio_config();
    // Variáveis de controle
    bool led_estado = false;  // Flag para controlar o estado do LED (false = desligado, true = ligado), para o led verde
    bool pwm_enabled = true;  // Controle do PWM (se habilitado ou desabilitado)
    bool last_button_state = true;
    bool fan_carv_20 = false, fan_carv_40 = false, fan_carv_60 = false, fan_carv_80 = false, fan_carv_100 = false;
    bool fan_hepa_20 = false, fan_hepa_40 = false, fan_hepa_60 = false, fan_hepa_80 = false, fan_hepa_100 = false;
    // Posição inicial do quadrado do display
    int y = 28;
    int x = 60;
    bool cor = true; // Cor do quadrado (pode ser ajustada conforme necessário)
    
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);
    // configura o I2C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA); // Pull up the data line
    gpio_pull_up(I2C_SCL); // Pull up the clock line
    ssd1306_t ssd; // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display
  
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
  
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);  
    
    uint16_t adc_value_x; //variaveis definidas, mas nao inicializadas
    uint16_t adc_value_y;  
    char str_x[5];  // Buffer para armazenar a string do MQ135 (qualidade do ar) 
    char str_y[5];  // Buffer para armazenar a string do MQ7 (monóxido de carbono).

    // Inicializa o PWM para o LED no pino GP12
    uint pwm_wrap = 4096;  // Define o valor máximo para o contador do PWM (influencia o duty cycle)
    uint pwm_slice_x = pwm_init_gpio(LEDR_PIN, pwm_wrap);  // Inicializa o PWM no pino LEDR_PIN e retorna o número do slice, red eixo y (x varia), cima (+) e baixo (-)
    uint pwm_slice_y = pwm_init_gpio(LEDB_PIN, pwm_wrap);  // Inicializa o PWM no pino LEDB_PIN e retorna o número do slice, azul eixo x (y varia), esquerda(+) e direita(-)
    //analogico no eixo y: cima (+) x = 4095, y = 2047, red = 100%, blue = 50% e baixo (-) x = 0, y = 2047, red =0%, blue = 50% 
    //eixo x: esquerda(+) x = 2047 , y = 0, red =50%, blue = 0% e direita(-) x = 2047, y = 4095, red =50%, blue = 100%
    //cores saindo eixo y: cima (+) = roxo,  baixo (-) = azul
    //eixo x: esquerda(+) = vermelho  e direita(-) = roxo
    //ta invertido no eixo y o x ta variando e no eixo x o y ta variando, e o x (eixo y) vai de 4095 a 0, baixo->cima, no y (eixo x) vai de 0 a 4095, esquerda->direita

    //analogico no eixo y: cima (+) x = 4095, y = 2047, red = 100%, blue = 50% e baixo (-) x = 0, y = 2047, red =0%, blue = 50% 
    //eixo x: esquerda(+) x = 2047 , y = 0, red =50%, blue = 0% e direita(-) x = 2047, y = 4095, red =50%, blue = 100%
    //cores saindo eixo y: cima (+) = roxo (mais azul),  baixo (-) = vermelho
    //eixo x: esquerda(+) = azul  e direita(-) = roxo (mais verm)

    uint32_t last_print_time = 0; // Variável para armazenar o tempo da última impressão na serial

    while (true)
    {
      adc_select_input(0); // Seleciona o ADC para eixo X. O pino 26 (VRX) como entrada analógica
      adc_value_x = adc_read(); // Lê o valor analógico do eixo X, retornando um valor entre 0 e 4095---
      //uint16_t adc_value_x = adc_read();  
      adc_select_input(1); // Seleciona o ADC para eixo Y. O pino 27 como entrada analógica
      adc_value_y = adc_read(); // Lê o valor analógico do eixo Y, retornando um valor entre 0 e 4095---    
      //uint16_t adc_value_y = adc_read();  
      bool joy_pressed = !gpio_get(JOYSTICK_PB);
      
      // Converte os valores ADC para ppm (0-1000)
      int ppm_x = (adc_value_x * 1000) / 4095;
      int ppm_y = (adc_value_y * 1000) / 4095;

      // Verifique se o valor de ppm_x ou ppm_y está abaixo ou igual a 50, que é considerado seguro
      if (ppm_x <= 50) {
        pwm_set_gpio_level(LEDR_PIN, 0);  // Desliga o LED vermelho (pino LEDR_PIN)
      } else {
        pwm_set_gpio_level(LEDR_PIN, adc_value_x);  // Ajusta o duty cycle do LED vermelho com base no valor de ppm_x
      }

      if (ppm_y <= 50) {
        pwm_set_gpio_level(LEDB_PIN, 0);  // Desliga o LED azul (pino LEDB_PIN)
      } else {
        pwm_set_gpio_level(LEDB_PIN, adc_value_y);  // Ajusta o duty cycle do LED azul com base no valor de ppm_y
      }
      
      // Converte os valores para string para exibição
      sprintf(str_x, "%d", ppm_x);  // Converte o inteiro (valor de MQ135) em string
      sprintf(str_y, "%d", ppm_y);  // Converte o inteiro (valor de MQ7) em string

      // Ativa ou desativa os LEDs PWM com o botão A
      bool button_state = gpio_get(Botao_A);
      if (!button_state && last_button_state) {  // Detecta mudança de estado (falling edge)
          pwm_enabled = !pwm_enabled;
      }
      last_button_state = button_state;

      // Cálculo de PWM com base na posição do joystick
      if (pwm_enabled) {
        // Controle dos LEDs com PWM baseado no valor do ADC0 (VRX) e ADC1 (VRY)
        pwm_set_gpio_level(LEDR_PIN, adc_value_x); // Ajusta o duty cycle do LED proporcional ao valor lido de VRX
        pwm_set_gpio_level(LEDB_PIN, adc_value_y); // Ajusta o duty cycle do LED proporcional ao valor lido de VRY
      } else {
        pwm_set_gpio_level(LEDR_PIN, 0);
        pwm_set_gpio_level(LEDB_PIN, 0);
      }      

      // Calcula o duty cycle em porcentagem para impressão
      float duty_cycle_x = (adc_value_x / 4095.0) * 100;  // Converte o valor lido do ADC em uma porcentagem do duty cycle, vermelho----------
      float duty_cycle_y = (adc_value_y / 4095.0) * 100;  // Converte o valor lido do ADC em uma porcentagem do duty cycle, azul

      // Imprime os valores lidos e o duty cycle proporcional na comunicação serial a cada 1 segundo
      uint32_t current_time = to_ms_since_boot(get_absolute_time());  // Obtém o tempo atual desde o boot do sistema
      if (current_time - last_print_time >= 1000) {  // Verifica se passou 1 segundo desde a última impressão
          printf("VRX: %u\n", adc_value_x);  // Imprime o valor lido de VRX no console serial
          printf("Duty Cycle LED_R: %.2f%%\n", duty_cycle_x);  // Imprime o duty cycle calculado em porcentagem, vermelho
          printf("VRY: %u\n", adc_value_y);  // Imprime o valor lido de VRY no console serial
          printf("Duty Cycle LED_B: %.2f%%\n\n", duty_cycle_y);  // Imprime o duty cycle calculado em porcentagem, azul
          last_print_time = current_time;  // Atualiza o tempo da última impressão-----------
      }
      //instrucoes display ssd1306
      // Atualiza o conteúdo do display com as novas informações

      //ssd1306_fill(&ssd, false); // Limpa o display
      ssd1306_fill(&ssd, !cor); // Limpa o display
      //ssd1306_rect(&ssd, 3, 3, 122, 60, true, false); // Desenha um retângulo externo
      ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo externo
      //ssd1306_line(&ssd, 3, 25, 123, 25, true); // Desenha uma linha horizontal parte de cima
      ssd1306_line(&ssd, 3, 25, 123, 25, cor); // Desenha uma linha horizontal parte de cima
      ssd1306_line(&ssd, 3, 47, 123, 47, true); // Desenha uma linha horizontal meio (mudar coordenadas)

      //void ssd1306_draw_string(ssd1306_t *ssd, const char *str, uint8_t x, uint8_t y)
      //x: A coordenada horizontal (posição da coluna) onde o texto começará, x pode variar de 0 a 127, representando a posição horizontal no display. 
      //y: A coordenada vertical (posição da linha) onde o texto será desenhado.

      // Desenha as informações no display
      ssd1306_draw_string(&ssd, "Poluicao: ", 6, 5); // Desenha a string especifica
      //ssd1306_draw_string(&ssd, ";", 110, 5); // Desenha uma string com simbolo de caveira representado por ";"
      if (ppm_x > 800) {
        ssd1306_draw_string(&ssd, ";", 110, 5); // Desenha a caveira se ppm_x > 800
      }
      ssd1306_draw_string(&ssd, "Nivel CO: ", 6, 16); // Desenha a string especifica
      //ssd1306_draw_string(&ssd, ";", 110, 16); // Desenha uma string com simbolo de caveira representado por ";"
      if (ppm_y > 800) {
        ssd1306_draw_string(&ssd, ";", 110, 16); // Desenha a caveira se ppm_y > 800
      }
      ssd1306_draw_string(&ssd, str_x, 85, 5); // Desenha uma string (valor de poluicao ar)
      ssd1306_draw_string(&ssd, str_y, 85, 16); // Desenha uma string  (valor de nivel co) 
      ssd1306_draw_string(&ssd, "Fan Carv: ", 6, 27); // Desenha a string especifica
      ssd1306_rect(&ssd, 33, 78, 8, 3, cor, fan_carv_20); // Desenha o primeiro retângulo de Fan Carv menos a direita, 20%
      ssd1306_rect(&ssd, 32, 87, 8, 4, cor, fan_carv_40); // Desenha o segundo retângulo de Fan Carv, entre o meio e o menos a direita, 40%
      ssd1306_rect(&ssd, 31, 96, 8, 5, cor, fan_carv_60); // Desenha o terceiro retângulo de Fan Carv nom meio,60%
      ssd1306_rect(&ssd, 30, 105, 8, 6, cor, fan_carv_80); // Desenha o quarto retângulo de Fan Carv, entre o meio e o mais a direita, 80%
      ssd1306_rect(&ssd, 29, 114, 8, 7, cor, fan_carv_100); // Desenha o quinto retângulo de Fan Carv mais a direita, 100%
      // Configuração dos níveis do ventilador de carvão com base em ppm_x
      if (ppm_x >= 200) fan_carv_20 = true; else fan_carv_20 = false;
      if (ppm_x >= 400) fan_carv_40 = true; else fan_carv_40 = false;
      if (ppm_x >= 600) fan_carv_60 = true; else fan_carv_60 = false;
      if (ppm_x >= 800) fan_carv_80 = true; else fan_carv_80 = false;
      if (ppm_x >= 1000) fan_carv_100 = true; else fan_carv_100 = false;
      ssd1306_draw_string(&ssd, "Fan HEPA: ", 6, 38); // Desenha a string especifica
      ssd1306_rect(&ssd, 42, 78, 8, 3, cor, fan_hepa_20); // Desenha o primeiro retângulo de Fan HEPA menos a direita, 20%
      ssd1306_rect(&ssd, 41, 87, 8, 4, cor, fan_hepa_40); // Desenha o segundo retângulo de Fan HEPA, entre o meio e o menos a direita, 40%
      ssd1306_rect(&ssd, 40, 96, 8, 5, cor, fan_hepa_60); // Desenha o terceiro retângulo de Fan HEPA nom meio,60%
      ssd1306_rect(&ssd, 39, 105, 8, 6, cor, fan_hepa_80); // Desenha o quarto retângulo de Fan HEPA, entre o meio e o mais a direita, 80%
      ssd1306_rect(&ssd, 38, 114, 8, 7, cor, fan_hepa_100); // Desenha o quinto retângulo de Fan HEPA mais a direita, 100%
      // Configuração dos níveis do ventilador HEPA com base em ppm_y
      if (ppm_y >= 200) fan_hepa_20 = true; else fan_hepa_20 = false;
      if (ppm_y >= 400) fan_hepa_40 = true; else fan_hepa_40 = false;
      if (ppm_y >= 600) fan_hepa_60 = true; else fan_hepa_60 = false;
      if (ppm_y >= 800) fan_hepa_80 = true; else fan_hepa_80 = false;
      if (ppm_y >= 1000) fan_hepa_100 = true; else fan_hepa_100 = false;
      
      if (ppm_x < 100 && ppm_y < 100) {
        ssd1306_draw_string(&ssd, "@@: OFF", 6, 52); // Desenha a string especifica
      } else {
        ssd1306_draw_string(&ssd, "@@: ON", 6, 52); // Desenha a string especifica
      }
      if (ppm_x > 900 && ppm_y > 900) {
        //ssd1306_draw_string(&ssd, "ALERTA", 54, 115); // Desenha a string especifica 
        //o display SSD1306 tem um limite de caracteres que pode exibir ao mesmo tempo, por isso essa linha nao escreve
        ssd1306_draw_string(&ssd, ";", 54, 115); // Desenha a caveira se os dois sensores estiverem acima de 900
        play_tone(BUZZER_PIN, 1000);  // Tocar 1000 Hz
      } else {
        pwm_set_chan_level(pwm_gpio_to_slice_num(BUZZER_PIN), PWM_CHAN_A, 0);  // Desligar o buzzer
      }
      
      // Atualiza o display
      ssd1306_send_data(&ssd);

      //void ssd1306_rect(ssd1306_t *ssd, uint8_t top, uint8_t left, uint8_t width, uint8_t height, bool value, bool fill)
      //uint8_t top: A coordenada Y do topo do retângulo (onde o retângulo começa verticalmente).
      //uint8_t left: A coordenada X da esquerda do retângulo (onde o retângulo começa horizontalmente).
      //uint8_t width: A largura do retângulo (quantos pixels ele ocupará horizontalmente).
      //uint8_t height: A altura do retângulo (quantos pixels ele ocupará verticalmente).
      //bool value: Um valor booleano que determina se o retângulo será desenhado com pixels acesos (true) ou apagados (false).
      //bool fill: Um valor booleano que determina se o retângulo será preenchido ou apenas contornado.
      

    // Introduz um atraso de 100 milissegundos antes de repetir a leitura
    sleep_ms(100);  // Pausa o programa por 100ms para evitar leituras e impressões muito rápidas
    }
  }
