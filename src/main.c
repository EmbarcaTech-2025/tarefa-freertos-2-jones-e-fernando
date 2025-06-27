/*! ============================================================================
 *
 *  Copyright(c) 2025 JV Gomes, EmbarcaTech - All rights reserved. 
 *  
 *  RFID Tag reader
 *  @brief    O programa a seguir faz a leitura e escrita de Tags RFID por meio
 *            do FreeRTOS
 *
 *  @file	    Template.c
 *  @author   Joao Vitor G. de Oliveira
 *  @date	    2 Jun 2025
 *  @version  1.0
 * 
============================================================================ */
/* ============================   LIBRARIES  =============================== */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#include "FreeRTOS.h"
#include "task.h"

#include "ssd1306.h"
/* =============================   MACROS   ================================ */

#define LED_R_PIN 13 // LED VERMELHO RGB BTDL
#define LED_G_PIN 11 // LED VERDE RGB BTDL
#define LED_B_PIN 12 // LED AZUL RGB BTDL

#define BUZZER_A_PIN 21       // BUZZER ESQUERDO BTD
#define BUZZER_FREQUENCY 8000 // Frequência do Buzzer em Hz

#define BUTTON_A_PIN 5 // Botão A BTDL
#define BUTTON_B_PIN 6 // Botão B BTDL

// --- Configurações do Display OLED ---
#define I2C_SDA_PIN 14   // Pino SDA para comunicação I2C com o OLED
#define I2C_SCL_PIN 15   // Pino SCL para comunicação I2C com o OLED
#define I2C_ADDRESS 0x3C // Endereço I2C do display OLED SSD1306
#define I2C_FREQUENCY 400000 // Frequência da comunicação I2C em Hz (400kHz)
#define I2C_PORT i2c1        // Instância do barramento I2C a ser utilizada (i2c1)
#define OLED_WIDTH 128       // Largura do display OLED em pixels
#define OLED_HEIGHT 64       // Altura do display OLED em pixels

// Instância da estrutura de controle do display OLED
ssd1306_t display;

/* =========================   GLOBAL VARIABLES   ========================== */

// --- Handles das Tarefas do FreeRTOS ---
TaskHandle_t xLedTaskHandle = NULL;    // Handle para a tarefa do LED
TaskHandle_t xBuzzerTaskHandle = NULL; // Handle para a tarefa do Buzzer
TaskHandle_t xOledTaskHandle = NULL;   // Handle para a tarefa do OLED

/* ========================   FUNCTION PROTOTYPE   ========================= */

void buzzer_pwm_init(void);
void SSD1306_Init(void);

/* ====================   TASKS FREERTOS PROTOTYPE   ======================= */

void led_task(void *pvParameters);
void buzzer_task(void *pvParameters);
void button_task(void *pvParameters);
void oled_task(void *pvParameters);

/* ===========================   MAIN FUNCTION   =========================== */
int main(void)
{
  stdio_init_all(); // Inicializa todas as E/S padrão (necessário para printf via USB/UART)
  sleep_ms(2000);   // Aguarda um tempo para a estabilização do sistema ou console serial
  printf("Sistema iniciando...\n");

  SSD1306_Init();     // Configura I2C e inicializa o display OLED
  buzzer_pwm_init();  // Configura o PWM para o buzzer

  printf("Hardware inicializado.\n");

  // Criação das tarefas do FreeRTOS
  // xTaskCreate(função_tarefa, nome_tarefa, tamanho_pilha, parametros_tarefa, prioridade, &handle_tarefa)
  BaseType_t ledStatus = xTaskCreate(led_task, "LED_Task", 256, NULL, 1, &xLedTaskHandle);
  BaseType_t buzzerStatus = xTaskCreate(buzzer_task, "Buzzer_Task", 256, NULL, 1, &xBuzzerTaskHandle);
  BaseType_t buttonStatus = xTaskCreate(button_task, "Button_Task", 256, NULL, 2, NULL); // Prioridade maior para botões
  BaseType_t oledStatus = xTaskCreate(oled_task, "OLED_Task", 256, NULL, 1, &xOledTaskHandle);

  // Verifica se todas as tarefas foram criadas com sucesso
  if (ledStatus != pdPASS || buzzerStatus != pdPASS || buttonStatus != pdPASS || oledStatus != pdPASS) 
  {
    printf("Erro ao criar uma ou mais tarefas!\n");
    // Exibe mensagem de erro no OLED se a inicialização falhar
    ssd1306_clear(&display);
    ssd1306_draw_string(&display, 0, 0, 1, "Erro Task!");
    ssd1306_show(&display);
    while(1); // Trava o sistema em caso de erro na criação de tarefas
  } 
  else 
  {
    printf("Tarefas criadas com sucesso.\n");
  }
  
  printf("Iniciando scheduler do FreeRTOS...\n");
  vTaskStartScheduler(); // Inicia o escalonador do FreeRTOS

  while (true)
  {
    
  }

}/* end main */

/* =======================  DEVELOPMENT OF FUNCTIONS ======================= */

/*! ---------------------------------------------------------------------------
 *  @brief Inicializa o PWM para controle do buzzer.
 *  Configura o pino do buzzer para funcionar como saída PWM, define os
 *  parâmetros de frequência através do divisor de clock e inicializa o slice
 *  PWM correspondente com o buzzer desligado.
 *
 *  @param[in] (void) : Não recebe parâmetros.
 *
 *  @return (void) : Não possui valor de retorno.
 * 
 ----------------------------------------------------------------------------*/
void buzzer_pwm_init(void)
{
    gpio_set_function(BUZZER_A_PIN, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(BUZZER_A_PIN);

    pwm_config config = pwm_get_default_config();
    // Calcula o divisor de clock para atingir a frequência desejada do buzzer.
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (BUZZER_FREQUENCY * 4096));
    pwm_init(slice_num, &config, true); // Inicializa o slice PWM e o habilita

    pwm_set_gpio_level(BUZZER_A_PIN, 0); // Inicia o PWM com o buzzer desligado
}

/*! ---------------------------------------------------------------------------
 *  @brief Inicializa o display OLED SSD1306 via interface I2C.
 *  A função configura os pinos SDA e SCL para comunicação I2C, habilita os 
 *  resistores de pull-up internos e inicializa o barramento I2C com a frequência 
 *  definida. Em seguida, realiza a inicialização do display SSD1306 com os 
 *  parâmetros de resolução e endereço I2C. Caso a inicialização falhe, o sistema 
 *  entra em loop travado. Após a inicialização bem-sucedida, exibe uma mensagem 
 *  temporária de "Inicializando..." no display por 2 segundos.
 *
 *  @param[in] (void) : Não recebe parâmetros.
 *
 *  @return (void) : Não possui valor de retorno.
 * 
 ----------------------------------------------------------------------------*/
void SSD1306_Init(void)
{
  i2c_init(I2C_PORT, I2C_FREQUENCY); // Inicializa o barramento I2C
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C); // Configura o pino SDA para I2C
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C); // Configura o pino SCL para I2C
  gpio_pull_up(I2C_SDA_PIN); // Habilita pull-up interno para SDA
  gpio_pull_up(I2C_SCL_PIN); // Habilita pull-up interno para SCL

  display.external_vcc = false; // Informa à biblioteca que o VCC é gerado internamente pelo display

  // Inicializa o display SSD1306
  if (!ssd1306_init(&display, OLED_WIDTH, OLED_HEIGHT, I2C_ADDRESS, I2C_PORT)) 
  {
    printf("Falha ao inicializar SSD1306!\n");
    while(1); // Trava se a inicialização falhar
  }
  printf("OLED ok!\n");
  ssd1306_clear(&display);
  ssd1306_draw_string(&display, 0, 0, 1, "Display Init...");
  ssd1306_show(&display);
  sleep_ms(2000); // Aguarda 2 segundos para exibir a mensagem de inicialização
}

/* ===========================  DEVELOPMENT TASKS ========================== */

/*! ---------------------------------------------------------------------------
 *  @brief Tarefa responsável por alternar as cores do LED RGB.
 *  A função inicializa os pinos do LED RGB como saída digital e, dentro de um
 *  loop infinito, alterna ciclicamente entre as cores (vermelho, verde e azul),
 *  mantendo cada cor acesa por um intervalo de 500ms.
 *
 *  @param[in] pvParameters : Parâmetro genérico da tarefa, não utilizado nesta função.
 *
 *  @return (void) : Não possui valor de retorno.
 * 
 ----------------------------------------------------------------------------*/
void led_task(void *pvParameters)
{
  const uint LED_PINS[] = {LED_R_PIN, LED_G_PIN, LED_B_PIN};
  const uint NUM_COLORS = sizeof(LED_PINS) / sizeof(LED_PINS[0]);
  uint current_color_index = 0;

  // Inicializa os pinos do LED como saída e os desliga
  for (uint i = 0; i < NUM_COLORS; ++i)
  {
    gpio_init(LED_PINS[i]);
    gpio_set_dir(LED_PINS[i], GPIO_OUT);
    gpio_put(LED_PINS[i], 0); // Garante que o LED comece desligado
  }

  while (true)
  {
    gpio_put(LED_PINS[current_color_index], 0); // Desliga a cor atual

    current_color_index = (current_color_index + 1) % NUM_COLORS; // Avança para a próxima cor

    gpio_put(LED_PINS[current_color_index], 1); // Liga a nova cor

    vTaskDelay(pdMS_TO_TICKS(500)); // Aguarda 500ms
  }
}

/*! ---------------------------------------------------------------------------
 *  @brief Tarefa responsável por gerar um som intermitente no buzzer.
 *  A função aciona o buzzer utilizando PWM com ciclo de trabalho de 50% por 
 *  100ms e, em seguida, o desliga por 900ms, criando um padrão sonoro com 
 *  pulsos a cada 1 segundo. A tarefa é executada de forma contínua dentro de 
 *  um loop infinito.
 *
 *  @param[in] pvParameters : Parâmetro genérico da tarefa, não utilizado nesta função.
 *
 *  @return (void) : Não possui valor de retorno.
 * 
 ----------------------------------------------------------------------------*/
void buzzer_task(void *pvParameters)
{
  while (true)
  {
    // Liga o buzzer com 50% do duty (12 bits/2 -> 2048)
    pwm_set_gpio_level(BUZZER_A_PIN, 2048);
    vTaskDelay(pdMS_TO_TICKS(100)); // Mantém ligado por 200ms
    pwm_set_gpio_level(BUZZER_A_PIN, 0);  // Desliga o buzzer

    vTaskDelay(pdMS_TO_TICKS(900)); // Aguarda 700ms, completando ciclos de 1 seg
  }
}

/*! ---------------------------------------------------------------------------
 *  @brief Tarefa responsável por monitorar os botões e controlar outras tarefas.
 *  A função inicializa dois botões como entrada com pull-up interno e faz a 
 *  leitura do estado deles de forma contínua. Ao detectar o acionamento do 
 *  Botão A, alterna entre suspender e retomar a tarefa do LED. Da mesma forma, 
 *  ao detectar o acionamento do Botão B, alterna entre suspender e retomar 
 *  a tarefa do buzzer. A detecção é feita por meio de borda de subida, 
 *  garantindo que a ação ocorra apenas uma vez por clique. A leitura é realizada 
 *  a cada 100ms, o que também contribui para o efeito de debouncing.
 *
 *  @param[in] pvParameters : Parâmetro genérico da tarefa, não utilizado nesta função.
 *
 *  @return (void) : Não possui valor de retorno.
 * 
 ----------------------------------------------------------------------------*/
void button_task(void *pvParameters)
{
  // Inicializa o pino do Botão A como entrada com pull-up interno
  gpio_init(BUTTON_A_PIN);
  gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_A_PIN);

  // Inicializa o pino do Botão B como entrada com pull-up interno
  gpio_init(BUTTON_B_PIN);
  gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_B_PIN);

  bool button_a_pressed_previously = false; // Estado anterior do Botão A
  bool button_b_pressed_previously = false; // Estado anterior do Botão B
  bool led_task_suspended = false;          // Flag para indicar se a tarefa do LED está suspensa
  bool buzzer_task_suspended = false;       // Flag para indicar se a tarefa do Buzzer está suspensa

  while (true)
  {
    // Leitura do Botão A (ativo em nível baixo devido ao pull-up)
    bool button_a_currently_pressed = !gpio_get(BUTTON_A_PIN);

    // Verifica se houve uma borda de subida (botão foi pressionado agora mas não antes)
    if (button_a_currently_pressed && !button_a_pressed_previously)
    {
      if (led_task_suspended)
      {
        vTaskResume(xLedTaskHandle); // Resume a tarefa do LED
        led_task_suspended = false;
        printf("Tarefa LED Retomada\n");
      }
      else
      {
        vTaskSuspend(xLedTaskHandle); // Suspende a tarefa do LED
        led_task_suspended = true;
        printf("Tarefa LED Suspensa\n");
      }
    }
    button_a_pressed_previously = button_a_currently_pressed; // Atualiza o estado anterior do Botão A

    // Leitura do Botão B (ativo em nível baixo devido ao pull-up)
    bool button_b_currently_pressed = !gpio_get(BUTTON_B_PIN);

    // Verifica se houve uma borda de subida (botão foi pressionado agora mas não antes)
    if (button_b_currently_pressed && !button_b_pressed_previously)
    {
      if (buzzer_task_suspended)
      {
          vTaskResume(xBuzzerTaskHandle); // Resume a tarefa do Buzzer
          buzzer_task_suspended = false;
          printf("Tarefa Buzzer Retomada\n");
      }
      else
      {
          vTaskSuspend(xBuzzerTaskHandle); // Suspende a tarefa do Buzzer
          buzzer_task_suspended = true;
          printf("Tarefa Buzzer Suspensa\n");
      }
    }
    button_b_pressed_previously = button_b_currently_pressed; // Atualiza o estado anterior do Botão B

    vTaskDelay(pdMS_TO_TICKS(100)); // Verifica os botões a cada 100ms para debouncing e responsividade
  }
}

/*! ---------------------------------------------------------------------------
 *  @brief Tarefa responsável por exibir no display OLED o status das tarefas LED e Buzzer.
 *  A função monitora continuamente o estado das tarefas LED e Buzzer por meio 
 *  dos seus handles e exibe no display OLED se cada uma está "Em execução", 
 *  "Suspensa" ou com "Handle Nulo" (quando não foi inicializada corretamente). 
 *  A atualização do display ocorre a cada 250ms. A função também aguarda até 
 *  que os handles das tarefas estejam válidos antes de iniciar a exibição, 
 *  garantindo que não ocorram leituras inválidas.
 *
 *  @param[in] pvParameters : Parâmetro genérico da tarefa, não utilizado nesta função.
 *
 *  @return (void) : Não possui valor de retorno.
 * 
 ----------------------------------------------------------------------------*/
void oled_task(void *pvParameters) 
{
  char led_status_str[25];    // String para armazenar o status da tarefa LED
  char buzzer_status_str[25]; // String para armazenar o status da tarefa Buzzer
  eTaskState led_state;       // Enum para armazenar o estado da tarefa LED
  eTaskState buzzer_state;    // Enum para armazenar o estado da tarefa Buzzer

  // Aguarda até que os handles das tarefas LED e Buzzer sejam válidos.
  // Necessário caso esta tarefa inicie antes da criação completa das outras.
  while(xLedTaskHandle == NULL || xBuzzerTaskHandle == NULL) 
  {
    vTaskDelay(pdMS_TO_TICKS(10)); // Pequeno delay para não sobrecarregar a CPU
  }

  printf("Tarefa OLED Iniciada\n");

  while (true) 
  {
    // Obtém o estado da tarefa LED, verificando se o handle é válido
    if (xLedTaskHandle != NULL)
    {
      led_state = eTaskGetState(xLedTaskHandle);
    } 
    else 
    {
      led_state = eInvalid; // Define como estado inválido se o handle for nulo
    }

    // Obtém o estado da tarefa Buzzer, verificando se o handle é válido
    if (xBuzzerTaskHandle != NULL) 
    {
      buzzer_state = eTaskGetState(xBuzzerTaskHandle);
    } 
    else 
    {
      buzzer_state = eInvalid; // Define como estado inválido se o handle for nulo
    }
    
    ssd1306_clear(&display); // Limpa o buffer do display antes de desenhar

    // Formata e exibe o status da tarefa LED
    if (led_state != eInvalid) 
    {
      sprintf(led_status_str, "Task LED: %s",(led_state == eSuspended) ? "Suspended" : "Run");
    } 
    else 
    {
      sprintf(led_status_str, "LED: Handle Nulo");
    }
    ssd1306_draw_string(&display, 0, 0, 1, led_status_str); // Desenha na linha 0

    // Formata e exibe o status da tarefa Buzzer
    if (buzzer_state != eInvalid) 
    {
      sprintf(buzzer_status_str, "Task Buzz: %s",(buzzer_state == eSuspended) ? "Suspended" : "Run");
    } 
    else 
    {
        sprintf(buzzer_status_str, "Buzzer: Handle Nulo");
    }
    ssd1306_draw_string(&display, 0, 10, 1, buzzer_status_str); // Desenha na linha 10 (abaixo da primeira)

    ssd1306_show(&display); // Atualiza o display físico com o conteúdo do buffer

    vTaskDelay(pdMS_TO_TICKS(250)); // Atualiza o display a cada 250ms
  }
}
/* end program */