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
#define LEDG_PIN 11  //GPIO led verde, acionado no clique do botao
#define LEDB_PIN 13  //GPIO led azul, no eixo y
#define LEDR_PIN 12  //GPIO led vermelho, no eixo x

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

// Mapeamento do ADC (0-4095) para a resolução do display (0-120 para X e 0-56 para Y)
int map_value(int value, int from_low, int from_high, int to_low, int to_high) {
  return to_low + ((value - from_low) * (to_high - to_low)) / (from_high - from_low);
}

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
//funcao auxiliar
void gpio_config(){
  //inicializa botao A e botao do analogico
  gpio_init(JOYSTICK_PB);
  gpio_set_dir(JOYSTICK_PB, GPIO_IN);
  gpio_pull_up(JOYSTICK_PB); 
  
  gpio_init(Botao_A);
  gpio_set_dir(Botao_A, GPIO_IN);
  gpio_pull_up(Botao_A);

  // Ativa as interrupções nos botões para chamar gpio_callback()
  gpio_set_irq_enabled_with_callback(Botao_A, GPIO_IRQ_EDGE_RISE, true, &gpio_irq_handler);
  gpio_set_irq_enabled_with_callback(JOYSTICK_PB, GPIO_IRQ_EDGE_RISE, true, &gpio_irq_handler);
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
    
    uint16_t adc_value_x;
    uint16_t adc_value_y;  
    //char str_x[5];  // Buffer para armazenar a string
    //char str_y[5];  // Buffer para armazenar a string  

    // Inicializa o PWM para o LED no pino GP12
    uint pwm_wrap = 4096;  // Define o valor máximo para o contador do PWM (influencia o duty cycle)
    uint pwm_slice_x = pwm_init_gpio(LEDR_PIN, pwm_wrap);  // Inicializa o PWM no pino LEDR_PIN e retorna o número do slice, red eixo x, esquerda(+) e direita(-)
    uint pwm_slice_y = pwm_init_gpio(LEDB_PIN, pwm_wrap);  // Inicializa o PWM no pino LEDB_PIN e retorna o número do slice, azul eixo y, cima (+) e baixo (-)
    uint pwm_slice_g = pwm_init_gpio(LEDG_PIN, pwm_wrap);  // Inicializa o PWM no pino LEDG_PIN e retorna o número do slice ---
    //analogico para o eixo y: cima (+) x = 2047, y = 4095 e baixo (-) x = 2047, y = 0, 
    //eixo x: esquerda(+) x = 0, y = 2047 e direita(-) x = 4095, y = 2047

    uint32_t last_print_time = 0; // Variável para armazenar o tempo da última impressão na serial

    while (true)
    {
      adc_select_input(0); // Seleciona o ADC para eixo X. O pino 26 (VRX) como entrada analógica
      adc_value_x = adc_read();
      uint16_t vrx_value = adc_read();  // Lê o valor analógico do eixo X, retornando um valor entre 0 e 4095
      adc_select_input(1); // Seleciona o ADC para eixo Y. O pino 27 como entrada analógica
      adc_value_y = adc_read();    
      uint16_t vry_value = adc_read();  // Lê o valor analógico do eixo Y, retornando um valor entre 0 e 4095
      bool joy_pressed = !gpio_get(JOYSTICK_PB);
      //sprintf(str_x, "%d", adc_value_x);  // Converte o inteiro em string
      //sprintf(str_y, "%d", adc_value_y);  // Converte o inteiro em string

      // Ativa ou desativa os LEDs PWM com o botão A
      bool button_state = gpio_get(Botao_A);
      if (!button_state && last_button_state) {  // Detecta mudança de estado (falling edge)
          pwm_enabled = !pwm_enabled;
      }
      last_button_state = button_state;

      // Mapeia os valores de VRX e VRY para o intervalo de PWM, Cálculo de PWM com base na posição do joystick
      //uint16_t pwm_x = abs(vrx_value - 2048) * 2;  // Ajusta o PWM para o LED vermelho (VRX), abs = absolute, funcao de <stdlib.h>
      //uint16_t pwm_y = abs(vry_value - 2048) * 2;  // Ajusta o PWM para o LED azul (VRY)

      // Cálculo de PWM com base na posição do joystick
      //uint16_t pwm_x = abs(vrx_value - 2047);  // Diferença absoluta em relação ao centro (2048)
      //uint16_t pwm_y = abs(vry_value - 2047);  // Diferença absoluta em relação ao centro (2048)
      uint16_t pwm_x;
      uint16_t pwm_y;
      const uint16_t deadzone = 50; // Definir uma zona morta fixa ao redor de 2048
      // Se o valor estiver dentro da zona morta, o PWM será zero
      if (vrx_value > (2048 - deadzone) && vrx_value < (2048 + deadzone)) {
        pwm_x = 0;
      } else {
        pwm_x = (abs(vrx_value - 2048) * 4096) / 2048;  // Mapeia de 0 a 4096
      }

      // Repete o mesmo processo para o eixo Y
      if (vry_value > (2048 - deadzone) && vry_value < (2048 + deadzone)) {
        pwm_y = 0;
      } else {
        pwm_y = (abs(vry_value - 2048) * 4096) / 2048;
      }
      // Limita o valor máximo do PWM a 4095, Limitação de segurança (caso algum valor ultrapasse 4095)
      if (pwm_x > 4095) pwm_x = 4095;
      if (pwm_y > 4095) pwm_y = 4095;

      if (pwm_enabled) {
        // Controle dos LEDs com PWM baseado no valor do ADC0 (VRX) e ADC1 (VRY)
        pwm_set_gpio_level(LEDR_PIN, pwm_x); // Ajusta o duty cycle do LED proporcional ao valor lido de VRX
        pwm_set_gpio_level(LEDB_PIN, pwm_y); // Ajusta o duty cycle do LED proporcional ao valor lido de VRY
      } else {
        pwm_set_gpio_level(LEDR_PIN, 0);
        pwm_set_gpio_level(LEDB_PIN, 0);
      }      
      // Controle dos LEDs com PWM baseado no valor do ADC0 (VRX) e ADC1 (VRY)
      //pwm_set_gpio_level(LEDR_PIN, vrx_value);  // Ajusta o duty cycle do LED proporcional ao valor lido de VRX
      //pwm_set_gpio_level(LEDB_PIN, vry_value);  // Ajusta o duty cycle do LED proporcional ao valor lido de VRY
      pwm_set_gpio_level(LEDG_PIN, joy_pressed ? pwm_wrap : 0);

      // Calcula o duty cycle em porcentagem para impressão
      float duty_cycle_x = (vrx_value / 4095.0) * 100;  // Converte o valor lido do ADC em uma porcentagem do duty cycle, vermelho
      float duty_cycle_y = (vry_value / 4095.0) * 100;  // Converte o valor lido do ADC em uma porcentagem do duty cycle, azul

      // Imprime os valores lidos e o duty cycle proporcional na comunicação serial a cada 1 segundo
      uint32_t current_time = to_ms_since_boot(get_absolute_time());  // Obtém o tempo atual desde o boot do sistema
      if (current_time - last_print_time >= 1000) {  // Verifica se passou 1 segundo desde a última impressão
          printf("VRX: %u\n", vrx_value);  // Imprime o valor lido de VRX no console serial
          printf("Duty Cycle LED_R: %.2f%%\n", duty_cycle_x);  // Imprime o duty cycle calculado em porcentagem, vermelho
          printf("VRY: %u\n", vry_value);  // Imprime o valor lido de VRY no console serial
          printf("Duty Cycle LED_G: %.2f%%\n", duty_cycle_y);  // Imprime o duty cycle calculado em porcentagem, azul
          last_print_time = current_time;  // Atualiza o tempo da última impressão
      }
      //instrucoes display ssd1306
      // Mapeia os valores do ADC para a posição do display
      x = map_value(adc_value_y, 0, 4095, 0, 120); // 128 - 8 para manter dentro da tela
      //y = map_value(adc_value_y, 0, 4095, 0, 56);  // 64 - 8 para manter dentro da tela
      y = map_value(adc_value_x, 0, 4095, 56, 0);
      //cor = !cor;
      // Atualiza o conteúdo do display com animações
      ssd1306_fill(&ssd, !cor); // Limpa o display (128x64)
      ssd1306_rect(&ssd, 1, 1, 126, 63, cor, !cor); // Desenha o retângulo externo 
      //ssd1306_line(&ssd, 3, 25, 123, 25, cor); // Desenha uma linha
      //ssd1306_line(&ssd, 3, 37, 123, 37, cor); // Desenha uma linha   
      //ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6); // Desenha uma string
      //ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16); // Desenha uma string
      //ssd1306_draw_string(&ssd, "ADC   JOYSTICK", 10, 28); // Desenha uma string 
      //ssd1306_draw_string(&ssd, "X    Y    PB", 20, 41); // Desenha uma string    
      //ssd1306_line(&ssd, 44, 37, 44, 60, cor); // Desenha uma linha vertical         
      //ssd1306_draw_string(&ssd, str_x, 8, 52); // Desenha uma string     
      //ssd1306_line(&ssd, 84, 37, 84, 60, cor); // Desenha uma linha vertical      
      //ssd1306_draw_string(&ssd, str_y, 49, 52); // Desenha uma string   
      //ssd1306_rect(&ssd, 52, 90, 8, 8, cor, !gpio_get(JOYSTICK_PB)); // Desenha um retângulo, preenche o retangulo ao clicar no analogico
      if (gpio_get(JOYSTICK_PB) == 0) {
        // Desenha apenas o contorno do novo retângulo quando o joystick for pressionado
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); 
      }
      //ssd1306_rect(&ssd, 52, 102, 8, 8, cor, !gpio_get(Botao_A)); // Desenha um retângulo          
      // Desenha o quadrado no display (com os valores calculados para x e y)
      ssd1306_rect(&ssd, y, x, 8, 8, cor, cor); // retângulo no meio do display
      //ssd1306_rect(&ssd, 28, 60, 8, 8, cor, cor); // retangulo no meio do display
      ssd1306_send_data(&ssd); // Atualiza o display


    // Introduz um atraso de 100 milissegundos antes de repetir a leitura
    sleep_ms(100);  // Pausa o programa por 100ms para evitar leituras e impressões muito rápidas
    }
  }