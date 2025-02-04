#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
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

//#define I2C_SDA_PIN 14
//#define I2C_SCL_PIN 15

const uint I2C_SDA_PIN = 14;     // Pino do botão A
const uint I2C_SCL_PIN = 15;     // Pino do botão B
const uint BTNA = 5;     // Pino do botão A
const uint BTNB = 6;     // Pino do botão B
const uint LED_R = 13;       // Pino do LED_R
const uint LED_G = 11;       // Pino do LED_R
const uint LED_B = 12;       // Pino do LED_R
const uint BUZZA = 21;       //
const uint BUZZB = 10;       //
const uint Y_PIN = 26;
const uint X_PIN = 27;
const uint SEL_PIN = 22;
const uint OLED_SDA_PIN = 14;  // Pino de dados (SDA)
const uint OLED_SCL_PIN = 15;  // Pino de clock (SCL)

uint8_t horz;
uint8_t vert;
bool selPressed;

// Inicializa o PWM no pino do buzzer
void pwm_init_buzzer(uint pin) {
  gpio_set_function(pin, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(pin);
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, 4.0f); // Ajusta divisor de clock
  pwm_init(slice_num, &config, true);
  pwm_set_gpio_level(pin, 0); // Desliga o PWM inicialmente
}

// Toca uma nota com a frequência e duração especificadas
    void play_tone(uint pin, uint frequency, uint duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1;

    // Equalização baseada na frequência
    float eq_factor;
    if(frequency < 3000) {
        eq_factor = 0.1;  // +6dB em graves
    } else if(frequency < 4000) {
        eq_factor = 0.1;  // 0dB
    } else {
        eq_factor = 0.1;  // -12dB em agudos
    }
        
    uint32_t level = (uint32_t)(top * eq_factor * 00.1);

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, level); // 50% de duty cycle

    sleep_ms(duration_ms);

    pwm_set_gpio_level(pin, 0); // Desliga o som após a duração
    sleep_ms(50); // Pausa entre notas
    }

// Função principal para tocar a música
void play_music(uint pin) {
    for (int i = 0; i < sizeof(music_notes) / sizeof(music_notes[0]); i++) {
        if (music_notes[i] == 0) {
            sleep_ms(note_duration[i]);
        } else {
            play_tone(pin, music_notes[i], note_duration[i]);
        }
    }
}

// Função para configurar os pinos do LED_R e dos botões
void config_pins() {
  stdio_init_all(); // Inicializa a comunicação serial (para depuração)

  gpio_init(LED_R);   // Inicializa o pino do LED_R
  gpio_init(LED_G);   // Inicializa o pino do LED_G
  gpio_init(LED_B);   // Inicializa o pino do LED_B
  gpio_init(BTNA);  // Inicializa o pino do botão A
  gpio_init(BTNB);  // Inicializa o pino do botão B
  adc_gpio_init(Y_PIN);
  adc_gpio_init(X_PIN);
  pwm_init_buzzer(BUZZA);
  pwm_init_buzzer(BUZZB);
  gpio_init(SEL_PIN);

  gpio_set_dir(LED_R, GPIO_OUT); // Configura o pino do LED_R como saída
  gpio_set_dir(LED_G, GPIO_OUT); // Configura o pino do LED_R como saída
  gpio_set_dir(LED_B, GPIO_OUT); // Configura o pino do LED_R como saída
  gpio_set_dir(BTNA, GPIO_IN); // Configura o pino do botão A como entrada
  gpio_set_dir(BTNB, GPIO_IN); // Configura o pino do botão B como entrada
  gpio_set_dir(SEL_PIN, GPIO_IN);

  gpio_put(LED_R, 0);      // Garante que o LED_R comece desligado
  gpio_put(LED_G, 1);      // Garante que o LED_G comece ligado
  gpio_put(LED_B, 0);      // Garante que o LED_B comece desligado
  gpio_pull_up(BTNA);  // Habilita o resistor de pull-up no botão A
  gpio_pull_up(BTNB);  // Habilita o resistor de pull-up no botão B
  gpio_put(BUZZA, 0);  
  gpio_put(BUZZB, 0);  
  gpio_pull_up(SEL_PIN);
}

void beep(char *buzz, uint16_t time){
  if ('A'){
    play_music(BUZZA);
  }
  else if('B'){ 
    play_music(BUZZB);
  }
  //sleep_ms(time);
  return;
}

// Função para inicializar o I2C
void init_i2c(){
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

uint8_t read_adc(uint8_t adc){
  // A leitura vai de 0 a 4095
  adc_select_input(adc);
  uint16_t leitura = adc_read();
  uint8_t result = 100*leitura/4094;
  return result;
}

void joystick(){
  horz = read_adc(1);
  vert = read_adc(0);
  selPressed = gpio_get(SEL_PIN);

  // selPressed é true se o joystick estiver pressionado
  printf("x: %u, y: %u, sel: %u\n", horz, vert, selPressed);
  return;
}

void get_button(){
  int vert = gpio_get(Y_PIN);
  int horz = gpio_get(X_PIN);
  bool selPressed = gpio_get(SEL_PIN);
  // horz vai de 0 (direita) a 1023 (esquerda)
  // vert vai de 0 (parte inferior) a 1023 (parte superior)
  // selPressed é true se o joystick estiver pressionado
}

void invert_display(bool invert){
  if(invert)
    SSD1306_send_cmd(SSD1306_SET_INV_DISP); // Inverte o display
  else 
    SSD1306_send_cmd(SSD1306_SET_NORM_DISP); // Volta ao normal
}

void clear_display(uint8_t *buf, struct render_area *frame_area){
  memset(buf, 0, SSD1306_BUF_LEN);
  render(buf, frame_area);
}

// Função para exibir texto no display
void show_text(char *text[], int num_lines, uint8_t *buf, struct render_area *frame_area, bool invert, uint16_t time) {
  int y = 0;
  for (int i = 0; i < num_lines; i++) {
    WriteString(buf, 5, y, text[i]); // Escreve a string no buffer
    y += 8; // Avança 8 pixels para a próxima linha
  }
  render(buf, frame_area); // Renderiza o buffer no display
  invert_display(invert);
  sleep_ms(time);
  return;
}

// Função para exibir framboesas no display
void display_rasp(uint8_t *buf, struct render_area *frame_area, uint16_t time) {
  // Renderiza 3 framboesas
  
  struct render_area area = {
    start_page: 0,
    end_page: (IMG_HEIGHT / SSD1306_PAGE_HEIGHT) - 1
  };

  area.start_col = 0;
  area.end_col = IMG_WIDTH - 1;
  calc_render_area_buflen(&area);
  uint8_t offset = 6 + IMG_WIDTH; // 6px padding

  for (int i = 0; i < 4; i++) {
    render(raspberry26x32, &area); // Renderiza a imagem da framboesa
    area.start_col += offset;
    area.end_col += offset;
  }

  SSD1306_scroll(true); // Ativa a rolagem
  sleep_ms(time);
  SSD1306_scroll(false); // Desativa a rolagem
  /////////////////////////////////////////////////////////
  struct render_area area_logo = {
    start_page: 0,
    end_page: (IMG_HEIGHT_LOGO / SSD1306_PAGE_HEIGHT) - 1
  };

  area_logo.start_col = 0;
  area_logo.end_col = IMG_WIDTH_LOGO - 1;
  calc_render_area_buflen(&area_logo);
  render(logo_embarcatech, &area_logo); // Renderiza a imagem da framboesa
  
  sleep_ms(3000);
}

// Inicializa a área de renderização para todo o frame
struct render_area frame_area = {
  start_col: 0,
  end_col: SSD1306_WIDTH - 1,
  start_page: 0,
  end_page: SSD1306_NUM_PAGES - 1
};
uint8_t buf[SSD1306_BUF_LEN];

void frequency_sweep_test(uint pin) {
    printf("Iniciando teste de frequencia:\n");
    
    for(uint freq = 2500; freq <= 7500; freq += 50) {
        printf("Testando %4u Hz...\n", freq);
        play_tone(pin, freq, 200);
    }
    
    printf("Teste concluido!\n");
}

// Função para inicializar o display
void init_display() {
  calc_render_area_buflen(&frame_area);
  
  // Zera o display
  clear_display(buf, &frame_area);

  //frequency_sweep_test(BUZZA);
  
  // Sequência de introdução: pisca a tela 3 vezes
  for (int i = 0; i < 3; i++) {
    SSD1306_send_cmd(SSD1306_SET_ALL_ON); // Liga todos os pixels
    sleep_ms(100);
    SSD1306_send_cmd(SSD1306_SET_ENTIRE_ON); // Volta ao modo normal
    sleep_ms(100);
  }

  beep("A", 1000);
  beep("B", 1000);

  display_rasp(buf, &frame_area, 2000); // Exibe as framboesas

  char *text[] = {
    "A long time ago",
    "  on an OLED ",
    "   display",
    " far far away",
    "Lived a small",
    "red raspberry",
    "by the name of",
    "    PICO"
  };

  int num_lines = sizeof(text) / sizeof(text[0]); // Calcula o número de linhas
  show_text(text, num_lines, buf, &frame_area, true, 1000); // Exibe o texto
  clear_display(buf, &frame_area); // Limpa o display
}

void loop_display(){
  char horzStr[10], vertStr[10], selPressedStr[10];

  // Convertendo os valores para strings
  sprintf(horzStr, "%d", horz);
  sprintf(vertStr, "%d", vert);
  sprintf(selPressedStr, "%d", selPressed);  

  char *text[] = {
    "Joystick Info",
    " ",
    "Horizontal",
    horzStr,
    "Vertical",
    vertStr,
    "Select State",
    selPressedStr    
  };

  int num_lines = sizeof(text) / sizeof(text[0]); // Calcula o número de linhas
  show_text(text, num_lines, buf, &frame_area, false, 1000); // Exibe o texto
  clear_display(buf, &frame_area); // Limpa o display
  /*
  bool pix = true;
  for (int i = 0; i < 2; i++) {
    for (int x = 0; x < SSD1306_WIDTH; x++) {
      DrawLine(buf, x, 0, SSD1306_WIDTH - 1 - x, SSD1306_HEIGHT - 1, pix);
      render(buf, &frame_area);
    }

    for (int y = SSD1306_HEIGHT - 1; y >= 0; y--) {
      DrawLine(buf, 0, y, SSD1306_WIDTH - 1, SSD1306_HEIGHT - 1 - y, pix);
      render(buf, &frame_area);
    }
    pix = false;
  }*/
}

int main() {
  stdio_init_all();
  config_pins();
  adc_init();
  init_i2c();
  init_display();

  while (true) {
    joystick();
    loop_display();
    sleep_ms(10);
  }

  return 0;
}