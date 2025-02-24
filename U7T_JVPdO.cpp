#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

#include "ssd1306_font.h"
#include "images.h"
#include "display.h"
#include "musics.h"

// Pino e canal do microfone e joystick no ADC.
const uint8_t ADC_VERT = 0;
const uint8_t ADC_HORZ = 1;
const uint8_t ADC_MIC = 2;

const uint MIC_PIN = 28;     // Pino do Microfone
const uint I2C_SDA_PIN = 14; // Pino do SDA do Display
const uint I2C_SCL_PIN = 15; // Pino do SCL do Display
const uint BTNA = 5;         // Pino do Botão A
const uint BTNB = 6;         // Pino do Botão B
const uint LED_R = 13;       // Pino do LED_R
const uint LED_G = 11;       // Pino do LED_R
const uint LED_B = 12;       // Pino do LED_R
const uint BUZZA = 21;       // Pino do Buzzer A
const uint BUZZB = 10;       // Pino do Buzzer B
const uint Y_PIN = 26;       // Pino da Posição Y do Joystick
const uint X_PIN = 27;       // Pino da Posição X do Joystick
const uint SEL_PIN = 22;     // Pino do Botão do Joystick

// Interrupções dos Botões
volatile bool btn_a_pressed = false;
volatile bool btn_b_pressed = false;
volatile bool sel_pressed = false;
volatile uint64_t last_interrupt_time = 0;

uint8_t saved_page = 3; // Página salva pelo botão B
uint64_t last_time; // Último tempo de leitura do ADC

uint8_t buf[SSD1306_BUF_LEN]; // Buffer para renderização do display

// Variáveis globais para acúmulo de tempo (em segundos)
float elapsed85 = 0.0f;
float elapsed88 = 0.0f;
float elapsed91 = 0.0f;
float elapsed94 = 0.0f;
float elapsed97 = 0.0f;

// Limite de volume máximo para alarme imediato (em dB)
#define MAX_VOLUME_THRESHOLD 100.0f

float adc_baseline = 2047.5f; // Valor inicial médio do ADC (12 bits)

float safe_values[5] = {0}; // Array de tempos seguros para cada faixa (85,88,91,94,97 dB)

// Contadores de alarmes
int alarmCountSafe = 0;      // Alarmes disparados por exposição excessiva
int alarmCountMaxVolume = 0; // Alarmes disparados por volume máximo

// Último motivo de alarme (para exibição)
char lastAlarmReason[30] = {0};

// Variável de controle para evitar alarmes repetidos enquanto a condição persistir
bool alarmActive = false;

#define NUM_READINGS 10
float mic_readings[NUM_READINGS] = {0}; // Buffer para as 10 últimas leituras
int reading_index = 0;                  // Índice do próximo elemento a ser escrito
float max_peak = 0.0f;

// Estrutura para armazenar as notas musicais
typedef struct
{
  int hours;
  int minutes;
  int seconds;
} TimeComponents;

// Inicializa a área de renderização para todo o frame
struct render_area frame_area = {
  start_col : 0,
  end_col : SSD1306_WIDTH - 1,
  start_page : 0,
  end_page : SSD1306_NUM_PAGES - 1
};

// Estrutura para armazenar as notas musicais
typedef struct
{
  float max_hours;
  const char *warning;
} ExposureLimit;

// Função de tratamento de interrupção
void gpio_callback(uint gpio, uint32_t events)
{
  // Debounce de 50ms
  if (time_us_64() - last_interrupt_time > 50000)
  {
    last_interrupt_time = time_us_64();

    if (gpio == BTNA)
      btn_a_pressed = true;
    if (gpio == BTNB)
      btn_b_pressed = true;
    if (gpio == SEL_PIN)
      sel_pressed = true;
  }
}

// Função para exibir texto no display
void show_text(const char *text[], int num_lines, uint8_t *buf, struct render_area *frame_area, bool invert, uint16_t time)
{
  int y = 0;
  for (int i = 0; i < num_lines; i++)
  {
    WriteString(buf, 5, y, (char *)text[i]); // Escreve a string no buffer
    y += 8;                                  // Avança 8 pixels para a próxima linha
  }
  render(buf, frame_area); // Renderiza o buffer no display
  // sleep_ms(time);
  return;
}

// Toca uma nota com a frequência e duração especificadas
void play_tone(uint pin, uint frequency, uint duration_ms)
{
  uint slice_num = pwm_gpio_to_slice_num(pin);
  uint32_t clock_freq = clock_get_hz(clk_sys);
  uint32_t top = clock_freq / frequency - 1;

  // Equalização baseada na frequência
  float eq_factor;
  if (frequency < 3000)
  {
    eq_factor = 0.1; // +6dB em graves
  }
  else if (frequency < 4000)
  {
    eq_factor = 0.1; // 0dB
  }
  else
  {
    eq_factor = 0.1; // -12dB em agudos
  }

  uint32_t level = (uint32_t)(top * eq_factor * 00.1);

  pwm_set_wrap(slice_num, top);
  pwm_set_gpio_level(pin, level); // 50% de duty cycle

  sleep_ms(duration_ms);

  pwm_set_gpio_level(pin, 0); // Desliga o som após a duração
  sleep_ms(50);               // Pausa entre notas
}

// Função principal para tocar a música
void play_music(uint pin, const Note notes[], int num_notes)
{
  for (int i = 0; i < num_notes; i++)
  {
    if (notes[i].frequency == 0)
    {
      sleep_ms(notes[i].duration);
    }
    else
    {
      play_tone(pin, notes[i].frequency, notes[i].duration);
    }
  }
}

void triggerAlarm(const char *reason)
{
  // Atualiza o último motivo
  strncpy(lastAlarmReason, reason, sizeof(lastAlarmReason));
  lastAlarmReason[sizeof(lastAlarmReason) - 1] = '\0';

  char line1[30], line2[30], line3[30];
  snprintf(line1, sizeof(line1), "%s", lastAlarmReason);

  const char *text[] = {
      "Alarme  Ativado",
      "               ",
      "   Motivador:  ",
      line1,
      "               ",
      " Pressione o A ",
      " para retornar ",
      " e  prosseguir "};

  const int num_lines = sizeof(text) / sizeof(text[0]);
  show_text(text, num_lines, buf, &frame_area, true, 1000);

  gpio_put(LED_R, 1);
  gpio_put(LED_G, 0);
  gpio_put(LED_B, 0);

  play_music(BUZZB, alarm_melody, sizeof(alarm_melody) / sizeof(alarm_melody[0]));
  gpio_put(LED_R, 0);
  sleep_ms(500);
}

// Função para ler o valor do ADC
uint32_t read_adc(uint8_t adc_channel) {
  adc_select_input(adc_channel);
  return adc_read();
}

// Calcula a potência média das leituras do ADC. (Valor RMS)
float mic_power()
{
  float avg = 0.f;
  uint32_t sound = 0;
  uint8_t medidas = 50;

  for (uint i = 0; i < medidas; ++i) {
    sound = read_adc(ADC_MIC);
    avg += abs(sound - adc_baseline); // Subtrai a baseline calibrada
  }

  return avg / medidas;
}

// Calcula a intensidade sonora em dB a partir da tensão lida no ADC
float get_intensity(float v)
{
  if (v == 0)
    return 0.0;
  else
    return 20 * log10((3.3 * v) / 0.05);
}

// Função para encontrar a cor do LED baseado na intensidade sonora
void find_led(float db)
{
  if (db <= 70.0f)
  {
    gpio_put(LED_R, 0);
    gpio_put(LED_G, 1);
    gpio_put(LED_B, 0);
  }
  else if (db <= 85.0f)
  {
    gpio_put(LED_R, 1);
    gpio_put(LED_G, 1);
    gpio_put(LED_B, 0);
  }
  else
  {
    gpio_put(LED_R, 1);
    gpio_put(LED_G, 0);
    gpio_put(LED_B, 0);
  }
  return;
}

// Inicializa o PWM no pino do buzzer
void pwm_init_buzzer(uint pin)
{
  gpio_set_function(pin, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(pin);
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, 4.0f); // Ajusta divisor de clock
  pwm_init(slice_num, &config, true);
  pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente
}

// Função para configurar os pinos do LED_R e dos botões
void config_pins()
{
  stdio_init_all(); // Inicializa a comunicação serial (para depuração)

  gpio_init(LED_R); // Inicializa o pino do LED_R
  gpio_init(LED_G); // Inicializa o pino do LED_G
  gpio_init(LED_B); // Inicializa o pino do LED_B
  gpio_set_irq_enabled_with_callback(BTNA, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
  gpio_set_irq_enabled_with_callback(BTNB, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
  gpio_set_irq_enabled_with_callback(SEL_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

  adc_gpio_init(Y_PIN);
  adc_gpio_init(X_PIN);
  adc_gpio_init(MIC_PIN);
  adc_init(); // Inicializa o módulo ADC
  adc_set_clkdiv(96.0f); // Configura o divisor de clock do ADC

  pwm_init_buzzer(BUZZA);
  pwm_init_buzzer(BUZZB);
  // gpio_init(SEL_PIN);

  gpio_set_dir(LED_R, GPIO_OUT); // Configura o pino do LED_R como saída
  gpio_set_dir(LED_G, GPIO_OUT); // Configura o pino do LED_R como saída
  gpio_set_dir(LED_B, GPIO_OUT); // Configura o pino do LED_R como saída
  gpio_set_dir(BTNA, GPIO_IN);   // Configura o pino do botão A como entrada
  gpio_set_dir(BTNB, GPIO_IN);   // Configura o pino do botão B como entrada
  gpio_set_dir(SEL_PIN, GPIO_IN);

  gpio_put(LED_R, 0); // Garante que o LED_R comece desligado
  gpio_put(LED_G, 0); // Garante que o LED_G comece desligado
  gpio_put(LED_B, 0); // Garante que o LED_B comece desligado
  gpio_pull_up(BTNA); // Habilita o resistor de pull-up no botão A
  gpio_pull_up(BTNB); // Habilita o resistor de pull-up no botão B
  gpio_put(BUZZA, 0);
  gpio_put(BUZZB, 0);
  gpio_pull_up(SEL_PIN);
}

// Função para inicializar o I2C
void init_i2c()
{
  // I2C is "open drain", pull ups to keep signal high when no data is being sent
  i2c_init(i2c1, SSD1306_I2C_CLK * 1000);
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);

  // run through the complete initialization process
  SSD1306_init();

  // Mensagem de confirmação da inicialização do I2C
  printf("I2C Iniciado!\n");
}


uint8_t joystick() // Função para controlar o menu com o joystick
{ 
  int horz = (200 * read_adc(ADC_HORZ) / 4094);
  printf("aquij1\n");
  int vert = (200 * read_adc(ADC_VERT) / 4094);
  printf("aquij2\n");
  horz -= 100;
  vert -= 100;
  uint8_t page = 3;

  if (abs(horz) < 10 && abs(vert) < 10)
    page = 3;
  else if (vert < -10 && vert < horz)
  {
    page = 1;
  }
  else if (vert > 10 && vert > abs(horz))
    page = 2;
  else if (horz < -10 && horz < vert)
    page = 4;
  else if (horz > 10 && horz > vert)
    page = 5;
  bool savePressed = gpio_get(BTNB); //////////////////////////
  if (btn_b_pressed == true)
  {
    saved_page = page;
    btn_b_pressed = false;
  }

  printf("x:%4d, y:%4d, page:%u\n", horz, vert, page);

  return page;
}

// Função para limpar o display
void clear_display(uint8_t *buf, struct render_area *frame_area)
{
  memset(buf, 0, SSD1306_BUF_LEN);
  render(buf, frame_area);
}

// Função para exibir framboesas e logo no display
void display_rasp(uint8_t *buf, struct render_area *frame_area)
{
  // Define o deslocamento vertical em páginas (cada página = 8px)
  uint8_t vertical_offset_pages = 2; // desloca 2 páginas para baixo (16 pixels)

  // Configura a área para renderizar as raspberries com o offset vertical
  struct render_area area = {
      .start_page = vertical_offset_pages,
      .end_page = (uint8_t)(vertical_offset_pages + (IMG_HEIGHT / SSD1306_PAGE_HEIGHT) - 1)};

  area.start_col = 0;
  area.end_col = IMG_WIDTH - 1;
  calc_render_area_buflen(&area);
  uint8_t offset = 6 + IMG_WIDTH; // 6px de padding horizontal

  for (int i = 0; i < 4; i++)
  {
    render(raspberry26x32, &area); // Renderiza a imagem da framboesa
    area.start_col += offset;
    area.end_col += offset;
  }

  SSD1306_scroll(true); // Ativa a rolagem
  sleep_ms(2000);
  SSD1306_scroll(false); // Desativa a rolagem

  // Configura a área para renderizar o logo, também com offset vertical
  struct render_area area_logo = {
      .start_page = 0,
      .end_page = (IMG_HEIGHT_LOGO / SSD1306_PAGE_HEIGHT) - 1};

  area_logo.start_col = 0;
  area_logo.end_col = IMG_WIDTH_LOGO - 1;
  calc_render_area_buflen(&area_logo);
  render(logo_embarcatech, &area_logo); // Renderiza o logo

  sleep_ms(2000);
}

// Função para testar a frequência do buzzer
void frequency_sweep_test(uint pin)
{
  printf("Iniciando teste de frequencia:\n");

  for (uint freq = 2500; freq <= 7500; freq += 50)
  {
    printf("Testando %4u Hz...\n", freq);
    play_tone(pin, freq, 200);
  }

  printf("Teste concluido!\n");
}

// Função para inicializar o display
void init_display()
{
  calc_render_area_buflen(&frame_area);

  // Zera o display
  clear_display(buf, &frame_area);

  // Sequência de introdução: pisca a tela 3 vezes
  for (int i = 0; i < 3; i++)
  {
    SSD1306_send_cmd(SSD1306_SET_ALL_ON); // Liga todos os pixels
    sleep_ms(10);
    SSD1306_send_cmd(SSD1306_SET_ENTIRE_ON); // Volta ao modo normal
    sleep_ms(10);
  }

  display_rasp(buf, &frame_area); // Exibe as framboesas

  play_music(BUZZA, intro_melody, sizeof(intro_melody) / sizeof(intro_melody[0]));
  gpio_put(LED_G, 0);

  const char *text[] = {
      "   S I M I S   ",
      "               ",
      "  Sistema de   ",
      " Monitoramento ",
      "de  Intensidade",
      "    Sonora     ",
      "               ",
      "feito por: jvpo"};

  int num_lines = sizeof(text) / sizeof(text[0]);           // Calcula o número de linhas
  show_text(text, num_lines, buf, &frame_area, true, 3000); // Exibe o texto
  sleep_ms(2000);
  clear_display(buf, &frame_area); // Limpa o display
}

float calculate_safe_exposure(float db)
{
  if (db <= 70.0f)
  {
    return INFINITY; // Tempo ilimitado
  }
  else if (db <= 85.0f)
  {
    return 8.0f; // 8 horas (valor base)
  }
  else
  {
    float steps = (db - 85.0f) / 3.0f;
    return 8.0f * powf(0.5f, steps);
  }
}

ExposureLimit get_exposure_details(float db)
{
  ExposureLimit result;

  if (db <= 70.0f)
  {
    result.max_hours = INFINITY;
    result.warning = "Ambiente seguro";
  }
  else if (db <= 85.0f)
  {
    result.max_hours = 8.0f;
    result.warning = " Uso  moderado ";
  }
  else
  {
    float steps = (db - 85.0f) / 3.0f;
    result.max_hours = 8.0f * powf(0.5f, steps);
    result.warning = "Perigo auditivo";
  }

  return result;
}

TimeComponents getTimeComponents(double elapsed)
{
  int total = (int)elapsed;
  TimeComponents tc;
  tc.hours = total / 3600;
  tc.minutes = (total % 3600) / 60;
  tc.seconds = total % 60;
  return tc;
}

void calculate_safe_values()
{
  // Calcula o tempo seguro (em segundos) para cada faixa
  safe_values[0] = calculate_safe_exposure(85.0f) * 3600.0f;
  safe_values[1] = calculate_safe_exposure(88.0f) * 3600.0f;
  safe_values[2] = calculate_safe_exposure(91.0f) * 3600.0f;
  safe_values[3] = calculate_safe_exposure(94.0f) * 3600.0f;
  safe_values[4] = calculate_safe_exposure(97.0f) * 3600.0f;
  return;
}

void loop_display()
{
  static uint64_t last_update = 0;
  // Atualiza a 10 FPS (33ms por frame)
  if (time_us_64() - last_update < 99000)
    return;
  last_update = time_us_64();
  printf("aqui1\n");
  uint8_t page = joystick();
  printf("aqui2\n");
  if (page == 3)
  {
    page = saved_page;
  }
  printf("aqui3\n");
  float avg = mic_power();
  float intensity = get_intensity(avg);
  if (intensity > max_peak)
    max_peak = intensity;
  find_led(intensity);
  ExposureLimit limit = get_exposure_details(intensity);

  mic_readings[reading_index] = intensity;
  reading_index = (reading_index + 1) % NUM_READINGS;

  // Considerando que cada iteração tem ~10 ms de intervalo
  // Calcula o tempo decorrido em segundos desde a última iteração
  uint64_t current_time = time_us_64();
  float dt = (current_time - last_time) / 1000000.0f; // dt em segundos
  last_time = current_time;

  if (intensity >= 85.0f)
    elapsed85 += dt;
  if (intensity >= 88.0f)
    elapsed88 += dt;
  if (intensity >= 91.0f)
    elapsed91 += dt;
  if (intensity >= 94.0f)
    elapsed94 += dt;
  if (intensity >= 97.0f)
    elapsed97 += dt;

  if (!alarmActive)
  {

    // Prioriza o alarme de volume máximo
    if (intensity >= MAX_VOLUME_THRESHOLD)
    {
      alarmCountMaxVolume++;
      triggerAlarm("VolMax excedido");
      alarmActive = true;
    }
    // Verifica os tempos acumulados para cada faixa (pode-se escolher a condição mais severa)
    else if (elapsed97 >= safe_values[4])
    {
      alarmCountSafe++;
      triggerAlarm("Temp Expos 97dB");
      alarmActive = true;
    }
    else if (elapsed94 >= safe_values[3])
    {
      alarmCountSafe++;
      triggerAlarm("Temp Expos 94dB");
      alarmActive = true;
    }
    else if (elapsed91 >= safe_values[2])
    {
      alarmCountSafe++;
      triggerAlarm("Temp Expos 91dB");
      alarmActive = true;
    }
    else if (elapsed88 >= safe_values[1])
    {
      alarmCountSafe++;
      triggerAlarm("Temp Expos 88dB");
      alarmActive = true;
    }
    else if (elapsed85 >= safe_values[0])
    {
      alarmCountSafe++;
      triggerAlarm("Temp Expos 85dB");
      alarmActive = true;
    }
  }

  while (alarmActive)
  {
    triggerAlarm(lastAlarmReason);
    if (btn_a_pressed)
    {
      alarmActive = false;
      btn_a_pressed = false;
      elapsed85 = 0.0f;
      elapsed88 = 0.0f;
      elapsed91 = 0.0f;
      elapsed94 = 0.0f;
      elapsed97 = 0.0f;
    }
    sleep_ms(10);
  }

  // clear_display(buf, &frame_area);
  switch (page)
  {
  case 1:
  {
    // Variáveis locais ao case 1
    char volume_str[30], tempo_str[30];

    // Formatação numérica
    snprintf(volume_str, sizeof(volume_str), "     %.2f dB   ", intensity);
    snprintf(tempo_str, sizeof(tempo_str),   "     %.2f h    ", limit.max_hours);

    // Array de texto com escopo local
    const char *text[] = {
        "MONITOR  SONORO",
        "               ",
        " Status  Atual ",
        limit.warning,
        "  Intensidade  ",
        volume_str,
        " Tempo  seguro ",
        isinf(limit.max_hours) ? "   ILIMITADO   " : tempo_str};

    const int num_lines = sizeof(text) / sizeof(text[0]);
    memset(buf, 0, SSD1306_BUF_LEN);
    show_text(text, num_lines, buf, &frame_area, false, 1000);
    break;
  }

  case 2:
  {
    memset(buf, 0, SSD1306_BUF_LEN);
    // Parâmetros do gráfico
    const int graph_x0 = 0;
    const int graph_y0 = 20; // margem superior para textos
    const int graph_width = SSD1306_WIDTH;
    const int graph_height = 40; // área reservada para o gráfico
    const int num_points = NUM_READINGS;
    const int step_x = graph_width / (num_points - 1);

    // Supondo uma escala fixa de dB (por exemplo, de 60 dB a 100 dB)
    const float dB_min = 0.0f;
    const float dB_max = 100.0f;

    // Calcula o pico das últimas leituras
    float peak = 0.0f;
    float mean = 0.0f;
    for (int i = 0; i < NUM_READINGS; i++)
    {
      if (mic_readings[i] > peak)
        peak = mic_readings[i];
      mean += mic_readings[i];
    }
    mean /= NUM_READINGS;

    // Desenha o gráfico conectando os pontos
    // Como o buffer é circular, vamos traçar na ordem cronológica:
    for (int i = 0; i < num_points - 1; i++)
    {
      // índice real: partindo do reading_index (ponto mais antigo) até o último
      int idx1 = (reading_index + i) % NUM_READINGS;
      int idx2 = (reading_index + i + 1) % NUM_READINGS;

      // Converte cada leitura (em dB) para coordenada Y na área do gráfico:
      // Quanto maior o dB, mais alto (y menor)
      int x1 = graph_x0 + i * step_x;
      int x2 = graph_x0 + (i + 1) * step_x;
      int y1 = graph_y0 + graph_height - (int)(((mic_readings[idx1] - dB_min) / (dB_max - dB_min)) * graph_height);
      int y2 = graph_y0 + graph_height - (int)(((mic_readings[idx2] - dB_min) / (dB_max - dB_min)) * graph_height);

      // Desenha a linha entre os pontos (assumindo que DrawLine está disponível)
      DrawLine(buf, x1, y1, x2, y2, true);
    }

    // Exibe o valor de pico na parte inferior ou superior do display
    char peak_str[20];
    snprintf(peak_str, sizeof(peak_str), " Pico: %.1f dB", peak);
    WriteString(buf, 0, 2, peak_str);

    char mean_str[20];
    snprintf(mean_str, sizeof(mean_str), " Media: %.1f dB", mean);
    WriteString(buf, 0, 8, mean_str);

    render(buf, &frame_area);
    break;
  }
  case 3:
  {
    // Array de texto fixo
    const char *text[] = {
        "MONITOR  SONORO",
        "               ",
        "Use o joystick ",
        "para se mover  ",
        "entre as telas ",
        "e o pressione B",
        "para travar na ",
        "tela atual     "};

    const int num_lines = sizeof(text) / sizeof(text[0]);
    memset(buf, 0, SSD1306_BUF_LEN);
    show_text(text, num_lines, buf, &frame_area, false, 1000);
    break;
  }

  case 4:
  {
    TimeComponents t85 = getTimeComponents(elapsed85);
    TimeComponents t88 = getTimeComponents(elapsed88);
    TimeComponents t91 = getTimeComponents(elapsed91);
    TimeComponents t94 = getTimeComponents(elapsed94);
    TimeComponents t97 = getTimeComponents(elapsed97);

    // Prepara as strings formatadas
    char line1[30], line2[30], line3[30], line4[30], line5[30];
    snprintf(line1, sizeof(line1), "85 dB %01d: %02d: %02d", t85.hours, t85.minutes, t85.seconds);
    snprintf(line2, sizeof(line2), "88 dB %01d: %02d: %02d", t88.hours, t88.minutes, t88.seconds);
    snprintf(line3, sizeof(line3), "91 dB %01d: %02d: %02d", t91.hours, t91.minutes, t91.seconds);
    snprintf(line4, sizeof(line4), "94 dB %01d: %02d: %02d", t94.hours, t94.minutes, t94.seconds);
    snprintf(line5, sizeof(line5), "97 dB %01d: %02d: %02d", t97.hours, t97.minutes, t97.seconds);

    // Array de strings para ser exibido pela função show_text
    const char *text[] = {
        " Tempo exposto ",
        "               ",
        " vol  h min seg",
        line1,
        line2,
        line3,
        line4,
        line5};
    int num_lines = sizeof(text) / sizeof(text[0]);
    memset(buf, 0, SSD1306_BUF_LEN);
    show_text(text, num_lines, buf, &frame_area, false, 2000);
    break;
  }

  case 5:
  {
    // Página de resumo dos alarmes disparados
    char line1[30], line2[30], line3[30];
    snprintf(line1, sizeof(line1), "Tempo Expo: %02d", alarmCountSafe);
    snprintf(line2, sizeof(line2), "Vol Maximo: %02d", alarmCountMaxVolume);
    snprintf(line3, sizeof(line3), "%s", lastAlarmReason);
    const char *text[] = {
        " INFO  ALARMES ",
        "               ",
        " QTD. Ativados ",
        line1,
        line2,
        "               ",
        "Ultimo  Motivo:",
        line3};
    int num_lines = sizeof(text) / sizeof(text[0]);
    memset(buf, 0, SSD1306_BUF_LEN);
    show_text(text, num_lines, buf, &frame_area, false, 3000);
    break;
  }

  default:
  {
    const char *text[] = {
        "MONITOR  SONORO",
        "               ",
        "Use o joystick ",
        "para se mover  ",
        "entre as telas ",
        "e o pressione  ",
        "para travar na ",
        "tela atual     "};

    const int num_lines = sizeof(text) / sizeof(text[0]);
    memset(buf, 0, SSD1306_BUF_LEN);
    show_text(text, num_lines, buf, &frame_area, false, 1000);
  }
  }
}

void calibrate_microphone() {
  const uint32_t num_samples = 500;
  uint32_t total = 0;
  
  // Amostra o ADC em condições silenciosas
  for (int i = 0; i < num_samples; i++) {
      total += read_adc(ADC_MIC);
      sleep_us(100); // Pequeno intervalo entre amostras
  }
  adc_baseline = (float)total / num_samples;
  
  // Feedback visual
  gpio_put(LED_B, 1);
  sleep_ms(200);
  gpio_put(LED_B, 0);
}

void test()
{
  if (gpio_get(SEL_PIN) == 0)
  {
    elapsed97 += 300;
  }
  return;
}

int main()
{
  last_time = time_us_64();
  stdio_init_all();
  config_pins();
  init_i2c();
  init_display();
  adc_init();
  calibrate_microphone();
  calculate_safe_values();

  while (true)
  {
    loop_display();
    test();
    sleep_ms(100);
  }

  return 0;
}