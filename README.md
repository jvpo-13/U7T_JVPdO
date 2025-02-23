# Monitor de Intensidade Sonora (SIMIS)

## Descrição do Projeto

Este projeto é um **Sistema de Monitoramento de Intensidade Sonora (SIMIS)** desenvolvido para o **Raspberry Pi Pico**. O objetivo principal é medir e exibir o nível de som ambiente em decibéis (dB) e alertar o usuário sobre exposições perigosas a ruídos elevados. O sistema conta com:

- **Microfone** para captação do som ambiente.
- **Display OLED** para exibição de informações.
- **LEDs RGB** para indicar diferentes níveis de som.
- **Buzzer** para alertas sonoros.
- **Botões e joystick** para interação do usuário.
- **Sistema de alarme** baseado na duração e intensidade da exposição sonora.

## Periféricos Utilizados

| Periférico                            | Função                                     |
| ------------------------------------- | ------------------------------------------ |
| **Microfone** (ADC)                   | Captura o som ambiente                     |
| **Display OLED (I2C)**                | Exibe informações do sistema               |
| **LED RGB**                           | Indica intensidade sonora                  |
| **Buzzer**                            | Emite alertas sonoros                      |
| **Botões A e B**                      | Interagem com o sistema                    |
| **Joystick**                          | Navega entre as telas                      |
| **ADC** (Conversor Analógico-Digital) | Lê valores do microfone                    |
| **DMA** (Acesso Direto à Memória)     | Processa dados do microfone com eficiência |

## Estrutura do Código

O código está organizado em diferentes seções para lidar com a inicialização dos periféricos, captura e processamento de dados, exibição no display e ativação de alarmes.

### 1. **Definição de Pinos e Variáveis Globais**

Os pinos do Raspberry Pi Pico são definidos no início do código, incluindo os pinos do microfone, OLED, LEDs, botões e joystick. Também são criadas variáveis para armazenar leituras do microfone e controlar os tempos de exposição sonora.

| Componente       | Pino no Raspberry Pi Pico |
|------------------|--------------------------|
| **Microfone**    | GPIO 28                   |
| **OLED SDA**     | GPIO 14                   |
| **OLED SCL**     | GPIO 15                   |
| **Botão A**      | GPIO 5                    |
| **Botão B**      | GPIO 6                    |
| **LED Vermelho** | GPIO 13                   |
| **LED Verde**    | GPIO 11                   |
| **LED Azul**     | GPIO 12                   |
| **Buzzer A**     | GPIO 21                   |
| **Buzzer B**     | GPIO 10                   |
| **Joystick X**   | GPIO 27                   |
| **Joystick Y**   | GPIO 26                   |
| **Joystick SEL** | GPIO 22                   |

### 2. **Inicialização dos Periféricos**

As funções `config_pins()`, `init_i2c()`, `init_display()` e `dma_config()` configuram os pinos, inicializam o I2C para o OLED e preparam o DMA para processamento eficiente dos dados do ADC.

### 3. **Captura e Processamento dos Dados do Microfone**

A leitura do microfone é feita pela função `mic_power()`, que calcula a potência do sinal. A intensidade sonora em dB é calculada pela função `get_intensity()`.

### 4. **Exibição de Dados no Display OLED**

O display mostra diferentes informações:

- Intensidade sonora atual
- Tempo de exposição acumulado
- Gráfico das últimas leituras
- Histórico de alarmes
  A função `show_text()` é usada para exibir mensagens formatadas na tela.

### 5. **Indicação Visual e Sonora**

A função `find_led()` controla os LEDs RGB para indicar o nível de som:

- **Verde** (<70 dB): Som seguro
- **Amarelo** (70-85 dB): Alerta moderado
- **Vermelho** (>85 dB): Som perigoso

A função `play_tone()` emite sons com o buzzer para alertas.

### 6. **Detecção de Alarmes**

A função `triggerAlarm()` é ativada quando os níveis de som são perigosos. Ela exibe mensagens no OLED, acende LEDs vermelhos e toca sons de alerta.

### 7. **Loop Principal**

A função `loop_display()` é executada continuamente para atualizar as leituras do microfone, calcular tempos de exposição e verificar condições para alarmes.

## Funcionamento

1. O sistema é iniciado e exibe a tela inicial.
2. O microfone captura o som e converte em um valor de dB.
3. O display OLED mostra as leituras e informações.
4. Se os níveis de som forem perigosos por muito tempo, um alarme é ativado.
5. O usuário pode navegar entre telas com o joystick.
6. LEDs RGB indicam os níveis de som.

## Melhorias Futuras

- Armazenamento dos dados em um **cartão SD**.
- Integração com **Wi-Fi/Bluetooth** para monitoramento remoto.
- Implementação de um **modo de economia de energia**.

## Conclusão

Este projeto demonstra como o Raspberry Pi Pico pode ser utilizado para medição e monitoramento sonoro, utilizando diversos periféricos. O sistema pode ser expandido para aplicações de segurança ocupacional, proteção auditiva e monitoramento ambiental.

---

**Desenvolvido por:** João Victor Pomiglio de Oliveira