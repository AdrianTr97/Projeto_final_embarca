# Projeto_final_embarca
Projeto Final Embarcatech - Sistema de Monitoramento de Qualidade do Ar

Implementação do projeto:
Link github: https://github.com/AdrianTr97/Projeto_final_embarca
Link vídeo Google Drive: https://drive.google.com/file/d/1BAd1FNWjeDYL3kmp87GeUPSa_CAVjNF6/view?usp=sharing
Link pasta do projeto em .rar:
https://drive.google.com/file/d/1xv09nSJGAWDxxTwzE548VIAhDhRP77WP/view?usp=sharing


-Descrição:

Este projeto visa monitorar a poluição do ar e os níveis de monóxido de carbono (CO) utilizando o Raspberry Pi Pico W e o kit de desenvolvimento BitDogLab. Sensores MQ135 e MQ7 são simulados através dos potenciômetros do joystick para representar a qualidade do ar e os níveis de CO. O sistema exibe informações no display OLED 128x64 e controla ventiladores simulados por LEDs PWM, ativando alertas visuais e sonoros em condições críticas.

-Funcionalidades:

Monitoramento de poluição do ar e CO

Exibição de dados no display OLED 128x64

Controle de ventiladores simulados via LEDs PWM

Alertas sonoros com buzzer em caso de níveis críticos

Botão para desativar LEDs e evitar interferência na leitura

-Hardware Utilizado:

Raspberry Pi Pico W

Kit BitDogLab

Display OLED SSD1306 (I2C)

Matriz de LEDs WS2812 5x5

Botões A e B

Joystick (potenciômetros usados para simular sensores)

LEDs RGB (usados para indicações visuais)

Buzzer para alertas

-Software:

Linguagem: C/C++ (Pico SDK)

Biblioteca SSD1306 para controle do display

Utiliza UART para depuração e diagnóstico

Controle de PWM para simulação da velocidade dos ventiladores

-Estrutura do Código:

Inicialização: Configura GPIOs, display e periféricos.

Leitura de Sensores: Obtém valores dos potenciômetros.

Processamento: Converte valores lidos para níveis de poluição e CO.

Saída: Atualiza display e controla LEDs PWM.

Alerta: Aciona buzzer e exibe mensagens em condição crítica.

-Como Usar:

Ligue o Raspberry Pi Pico W e conecte o kit BitDogLab.

Ajuste os potenciômetros do joystick para simular os sensores.

Observe os dados no display e o comportamento dos LEDs PWM.

Em caso de alerta crítico, pressione o botão para desativar os LEDs.

-Melhorias Futuras:

Integração com conexão Wi-Fi para envio de dados.

Implementação de um histórico de medições.

Uso de sensores reais para medição precisa.



