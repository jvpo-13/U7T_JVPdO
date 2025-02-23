# SIMIS - Sistema de Monitoramento de Intensidade Sonora

![Diagrama de blocos simplificado](https://via.placeholder.com/800x400.png?text=Diagrama+do+Hardware)  
*Projeto para Raspberry Pi Pico que monitora níveis de pressão sonora (dB) através de um microfone, com interface gráfica OLED, controle por joystick e alarmes sonoros/visuais.*

---

## 📋 Visão Geral
Monitora níveis de dB em tempo real, calcula tempos seguros de exposição (baseado em normas NIOSH) e dispara alarmes para condições perigosas. Desenvolvido em C para Raspberry Pi Pico.

---

## 🛠 Hardware Necessário
| Componente           | Função                              |
|----------------------|-------------------------------------|
| Raspberry Pi Pico    | Microcontrolador principal          |
| Microfone (ADC)      | Captura de áudio                    |
| OLED 128x64 (I2C)    | Exibição de dados                   |
| Joystick analógico   | Navegação no menu                   |
| LEDs RGB             | Feedback visual de intensidade      |
| Buzzer passivo       | Alarmes sonoros                     |

---

## 🎯 Funcionalidades Principais
- **Medição contínua de dB** (60-110 dB)
- **Cálculo de tempo seguro** de exposição (NIOSH)
- **Alarmes** para:
  - Volume instantâneo >100 dB
  - Tempo acumulado em faixas críticas
- **Gráfico temporal** das últimas leituras
- **Histórico detalhado** por faixa de intensidade
- **Menu interativo** com 5 telas navegáveis

---

## 🧩 Estrutura do Código

### 1. Configuração Inicial (`config_pins()`)
```c
void config_pins() {
  // Inicializa GPIOs e ADCs
  gpio_init(LED_R);
  adc_gpio_init(MIC_PIN);
  
  // Configura interrupções para botões
  gpio_set_irq_enabled_with_callback(BTNA, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
  
  // PWM para buzzer
  pwm_init_buzzer(BUZZA);
}

2. Processamento de Áudio
c
Copy
float get_intensity(float v) {
  return 20 * log10((3.3 * v)/0.033); // Conversão para dB
}

void find_led(float db) {
  if(db <= 70) GPIO_OUT(LED_G, 1);       // Verde: <70 dB
  else if(db <= 85) GPIO_OUT(LED_R, 1);  // Vermelho: 70-85 dB
  else GPIO_OUT(LED_B, 1);               // Azul: >85 dB
}
Converte tensão do ADC para escala logarítmica (dB)

Sistema de cores para feedback imediato

3. Lógica de Alarmes
c
Copy
void triggerAlarm(const char *reason) {
  play_music(BUZZB, alarm_melody, ...); // Toca melodia
  gpio_put(LED_R, 1);                   // LED vermelho
  show_text("ALARME ATIVADO", ...);      // Exibe motivo
}
Condições de disparo:

Volume instantâneo >100 dB

Exposição acumulada em faixas:

85 dB: 8 horas

88 dB: 4 horas (-3dB = ½ tempo)

91 dB: 2 horas

4. Interface Gráfica (loop_display())
c
Copy
void loop_display() {
  switch(page) {
    case 1: // Tela principal
      show_text(intensity, max_hours...);
    case 2: // Gráfico temporal
      DrawLine(buf, x1, y1, x2, y2);
  }
}
Tela	Conteúdo
1	Status atual (dB/tempo seguro)
2	Gráfico das últimas leituras
3	Instruções de uso
4	Tempos acumulados por faixa
5	Histórico de alarmes
5. Gestão de Tempo de Exposição
c
Copy
float calculate_safe_exposure(float db) {
  if(db <= 70) return INFINITY;
  return 8 * pow(0.5, (db-85)/3); // NIOSH
}
Exemplo de redução:

85 dB → 8 horas

88 dB → 4 horas

91 dB → 2 horas

94 dB → 1 hora

97 dB → 30 min

🔧 Customização
Parâmetro	Variável	Valores Típicos
Limite volume máximo	MAX_VOLUME_THRESHOLD	90-110 dB
Taxa de atualização	NUM_READINGS	10-50 amostras
Tempos de exposição	calculate_safe_exposure	Ajustar base/step
Melodias de alarme	alarm_melody[]	Editar notas/musics.h
📦 Dependências
pico-sdk

Biblioteca SSD1306 (ssd1306_font.h, images.h)

Arquivos customizados (display.h, musics.h)

⚙ Como Usar
Conectar hardware conforme pinagem definida

Compilar com pico-sdk

Navegação:

Joystick horizontal: muda telas

Botão B: fixa tela atual

Botão A: limpa alarmes

mermaid
Copy
graph TD
  A[Leitura ADC] --> B{Processamento}
  B --> C[Display]
  B --> D[Alarmes]
  C --> E[Gráfico]
  D --> F[LEDs/Buzzer]
📄 Licença
MIT License - jvpo | 2023
Livro para uso e modificação, com atribuição ao autor original.