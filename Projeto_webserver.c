/*
 * Sistema de Controle Residencial Inteligente para Raspberry Pi Pico
 *
 * Este programa implementa um sistema de automação residencial controlado via web com:
 * - Conexão WiFi para controle remoto
 * - Matriz de LEDs para exibição de status
 * - Display OLED para notificações
 * - Sensor ultrassônico para detecção de proximidade
 * - Sensor de luz para iluminação automática
 * - Monitoramento de temperatura
 * - Interface web para controle de dispositivos domésticos
 */

#include <stdio.h>               // Funções padrão de entrada/saída
#include <string.h>              // Funções para manipulação de strings
#include <stdlib.h>              // Alocação de memória e outras utilidades

#include "pico/stdlib.h"         // Funções padrão do Raspberry Pi Pico
#include "hardware/adc.h"        // Funções do ADC (Conversor Analógico-Digital)
#include "pico/cyw43_arch.h"     // Driver WiFi CYW43

#include "lwip/pbuf.h"           // Manipulação de buffers de pacotes IP
#include "lwip/tcp.h"            // Implementação do protocolo TCP
#include "lwip/netif.h"          // Funções de interface de rede

#include "hardware/i2c.h"        // Interface I2C
#include "inc/ssd1306.h"         // Driver para display OLED
#include "inc/font.h"            // Definições de fontes para o display
#include "hardware/pio.h"        // Funções de I/O programável
#include "hardware/clocks.h"     // Funções de controle de clock
#include "animacoes_led.pio.h"   // Programa PIO para animações de LED

// Credenciais da rede WiFi - Cuidado ao compartilhar publicamente!
#define WIFI_SSID "**************"
#define WIFI_PASSWORD "********"

/* ========== DEFINIÇÕES DE HARDWARE ========== */

// Configuração da matriz de LEDs
#define NUM_PIXELS 25           // Número de LEDs na matriz
#define matriz_leds 7           // Pino de saída para a matriz

// Configuração I2C para o display OLED
#define I2C_PORT i2c1           // Porta I2C utilizada
#define I2C_SDA 14              // Pino SDA
#define I2C_SCL 15              // Pino SCL
#define ENDERECO 0x3C           // Endereço I2C do display
#define WIDTH 128               // Largura do display em pixels
#define HEIGHT 64               // Altura do display em pixels

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN  // GPIO do chip CYW43
#define LED_BLUE_PIN 12                // GPIO12 - LED azul
#define LED_GREEN_PIN 11               // GPIO11 - LED verde
#define LED_RED_PIN 13                 // GPIO13 - LED vermelho

// Pinos para o sensor ultrassônico
#define TRIG_PIN 8              // Pino de trigger do sensor
#define ECHO_PIN 9              // Pino de echo do sensor

// Pino para o sensor de luz (LDR)
#define ldr_pin 16

// Variáveis globais para controle dos dispositivos
PIO pio;                       // Controlador PIO
uint sm;                       // State Machine do PIO
uint contagem = 5;             // Contador para exibição na matriz
ssd1306_t ssd;                 // Estrutura do display OLED

int tv = 0;

// Estados dos dispositivos (ligado/desligado)
bool estado_led_sala = false;
bool estado_led_cozinha = false;
bool estado_led_quarto = false;
bool estado_led_banheiro = false;
bool estado_led_quintal = false;
bool estado_display = false;

/* ========== PROTÓTIPOS DE FUNÇÕES ========== */
void gpio_led_bitdog(void);    // Inicializa os GPIOs dos LEDs
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err); // Callback para conexões TCP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err); // Callback para recebimento de dados
float temp_read(void);         // Lê a temperatura interna
void user_request(char **request); // Processa as requisições do usuário
void ligar_luz();              // Controla a matriz de LEDs
void ligar_display();          // Controla o display OLED
void send_trigger_pulse();     // Envia pulso para o sensor ultrassônico
float measure_distance_cm();   // Mede a distância com o sensor ultrassônico
void luz_frente_controlada();  // Controla os LEDs frontais baseado em sensores

/* ========== IMPLEMENTAÇÃO DAS FUNÇÕES ========== */

// Função principal
int main() {
    // Inicializa todas as bibliotecas padrão
    stdio_init_all();

    // Inicializa os GPIOs dos LEDs
    gpio_led_bitdog();

    // Configuração do PIO para a matriz de LEDs
    pio = pio0;
    uint offset = pio_add_program(pio, &animacoes_led_program);
    sm = pio_claim_unused_sm(pio, true);
    animacoes_led_program_init(pio, sm, offset, matriz_leds);

    // Configuração do I2C para o display OLED
    i2c_init(I2C_PORT, 400 * 1000);  // Inicializa I2C a 400kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Inicialização do display OLED
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);  // Limpa o display
    ssd1306_send_data(&ssd);

    // Inicializa o chip WiFi
    while (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    // Configura o LED do WiFi como desligado inicialmente
    cyw43_arch_gpio_put(LED_PIN, 0);

    // Configura o modo Station para conectar a uma rede WiFi
    cyw43_arch_enable_sta_mode();

    // Tenta conectar ao WiFi
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    // Exibe o IP atribuído ao dispositivo
    if (netif_default) {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura o servidor TCP na porta 80 (HTTP)
    struct tcp_pcb *server = tcp_new();
    if (!server) {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    // Associa o servidor à porta 80
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    // Coloca o servidor em modo de escuta
    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    // Inicializa o ADC para leitura de temperatura
    adc_init();
    adc_set_temp_sensor_enabled(true);

    // Loop principal do programa
    while (true) {
        // Controla os LEDs frontais baseado nos sensores
        luz_frente_controlada();
        
        // Atualiza o estado da matriz de LEDs
        ligar_luz();
        
        // Atualiza o display OLED
        ligar_display();
        
        // Processa eventos de rede
        cyw43_arch_poll();
        
        // Pequena pausa para reduzir uso da CPU
        sleep_ms(100);
    }

    // Desliga o WiFi antes de encerrar
    cyw43_arch_deinit();
    return 0;
}

/* ========== FUNÇÕES DE HARDWARE ========== */

// Inicializa os pinos GPIO para os LEDs e sensores
void gpio_led_bitdog(void) {
    // Configura os LEDs RGB como saída
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);
    
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);
    
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);

    // Configura o sensor ultrassônico
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    // Configura o sensor de luz (LDR)
    gpio_init(ldr_pin);
    gpio_set_dir(ldr_pin, GPIO_IN);
}

/* ========== FUNÇÕES DE CONTROLE ========== */

// Controla a matriz de LEDs baseado nos estados dos cômodos
void ligar_luz() {
    uint32_t luz_sala, luz_cozinha, luz_quarto, luz_banheiro, luz_quintal;

    // Define as cores para cada cômodo baseado no estado
    luz_sala = estado_led_sala ? 0xFFFFFF00 : 0x00000000;
    luz_cozinha = estado_led_cozinha ? 0xFFFFFF00 : 0x00000000;
    luz_quarto = estado_led_quarto ? 0xFFFFFF00 : 0x00000000;
    luz_banheiro = estado_led_banheiro ? 0xFFFFFF00 : 0x00000000;
    luz_quintal = estado_led_quintal ? 0xFFFFFF00 : 0x00000000;

    // Atualiza cada LED da matriz 5x5
    for (int i = 0; i < NUM_PIXELS; i++) {
        uint32_t valor_led = 0;
        int linha = i / 5;  // Divide a matriz em linhas
        
        // Atribui a cor baseado na linha (cômodo)
        if (linha == 4) {
            valor_led = luz_sala;
        } else if (linha == 3) {
            valor_led = luz_cozinha;
        } else if (linha == 2) {
            valor_led = luz_quarto;
        } else if (linha == 1) {
            valor_led = luz_banheiro;
        } else if (linha == 0) {
            valor_led = luz_quintal;
        } else {
            valor_led = 0x000000;  // LED apagado
        }
        
        // Envia o valor para o LED
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}

// Controla o display OLED
void ligar_display() {
    bool cor = true;  // Cor branca para o texto

    // Limpa e desenha a moldura do display
    ssd1306_fill(&ssd, !cor);
    ssd1306_rect(&ssd, 0, 0, 127, 63, cor, !cor);    // Moldura externa
    ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);    // Moldura interna

    // Exibe o estado da TV
    if (estado_display) {
        // Limpa e desenha a moldura do display
        ssd1306_fill(&ssd, !cor);
        ssd1306_rect(&ssd, 0, 0, 127, 63, cor, !cor);    // Moldura externa
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);    // Moldura interna
        ssd1306_draw_string(&ssd, "TELEVISAO ", 35, 30);
        ssd1306_draw_string(&ssd, "LIGADA", 38, 40);

        // Atualiza o display
        ssd1306_send_data(&ssd);

        tv = 1;
    } else {
        if (tv == 1){
            // Limpa e desenha a moldura do display
        ssd1306_fill(&ssd, !cor);
        ssd1306_rect(&ssd, 0, 0, 127, 63, cor, !cor);    // Moldura externa
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);    // Moldura interna
        ssd1306_draw_string(&ssd, "TELEVISAO ", 30, 30);
        ssd1306_draw_string(&ssd, "DESLIGADA", 28, 40);

        // Atualiza o display
        ssd1306_send_data(&ssd);

        sleep_ms(2000);

        ssd1306_fill(&ssd, !cor);

        // Atualiza o display
        ssd1306_send_data(&ssd);

        tv = 0;
        }
    }
}

/* ========== FUNÇÕES DOS SENSORES ========== */

// Envia um pulso para o sensor ultrassônico
void send_trigger_pulse() {
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);
}

// Mede a distância usando o sensor ultrassônico
float measure_distance_cm() {
    send_trigger_pulse();

    // Espera o pino ECHO ficar em HIGH
    while (gpio_get(ECHO_PIN) == 0);

    // Marca o tempo de início
    absolute_time_t start = get_absolute_time();

    // Espera o pino ECHO voltar para LOW
    while (gpio_get(ECHO_PIN) == 1);

    // Calcula a duração do pulso em microssegundos
    absolute_time_t end = get_absolute_time();
    int64_t pulse_duration = absolute_time_diff_us(start, end);

    // Converte para centímetros (fórmula padrão para sensor HC-SR04)
    float distance_cm = pulse_duration / 58.0;

    return distance_cm;
}

// Controla os LEDs frontais baseado nos sensores
void luz_frente_controlada() {
    float dist = measure_distance_cm();
    
    // Aciona os LEDs se houver objeto próximo e estiver escuro
    if ((dist < 15) && (!gpio_get(ldr_pin))) {
        gpio_put(LED_BLUE_PIN, 1);
        gpio_put(LED_GREEN_PIN, 1);
        gpio_put(LED_RED_PIN, 1);
    } else {
        gpio_put(LED_BLUE_PIN, 0);
        gpio_put(LED_GREEN_PIN, 0);
        gpio_put(LED_RED_PIN, 0);
    }
}

/* ========== FUNÇÕES DE REDE ========== */

// Callback para aceitar novas conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Processa as requisições do usuário
void user_request(char **request) {
    // Verifica qual comando foi recebido e altera o estado correspondente
    if (strstr(*request, "GET /mudar_estado_luz_sala") != NULL) {
        estado_led_sala = !estado_led_sala;
    }
    else if (strstr(*request, "GET /mudar_estado_luz_cozinha") != NULL) {
        estado_led_cozinha = !estado_led_cozinha;
    }
    else if (strstr(*request, "GET /mudar_estado_luz_quarto") != NULL) {
        estado_led_quarto = !estado_led_quarto;
    }
    else if (strstr(*request, "GET /mudar_estado_luz_banheiro") != NULL) {
        estado_led_banheiro = !estado_led_banheiro;
    }
    else if (strstr(*request, "GET /mudar_estado_luz_quintal") != NULL) {
        estado_led_quintal = !estado_led_quintal;
    }
    else if (strstr(*request, "GET /mudar_estado_display") != NULL) {
        estado_display = !estado_display;
    }
    else if (strstr(*request, "GET /on") != NULL) {
        cyw43_arch_gpio_put(LED_PIN, 1);
    }
    else if (strstr(*request, "GET /off") != NULL) {
        cyw43_arch_gpio_put(LED_PIN, 0);
    }
}

// Lê a temperatura interna do RP2040
float temp_read(void) {
    adc_select_input(4);  // Seleciona o canal do sensor de temperatura
    uint16_t raw_value = adc_read();
    
    // Fórmula de conversão para temperatura (documentação do RP2040)
    const float conversion_factor = 3.3f / (1 << 12);
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;
    
    return temperature;
}

// Callback para recebimento de dados TCP (requisições HTTP)
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Copia a requisição para um buffer
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Processa a requisição do usuário
    user_request(&request);
    
    // Lê a temperatura atual
    float temperature = temp_read();

    // HTML da página web
    char html[2048];
    snprintf(html, sizeof(html),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n"
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title>Controle Residencial</title>\n"
             "<style>\n"
             "body { background-color:rgb(188, 251, 181); font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
             "h1 { font-size: 64px; margin-bottom: 30px; }\n"
             "button { background-color: LightBlue; font-size: 36px; margin: 10px; padding: 20px 40px; border-radius: 10px; }\n"
             ".temperature { font-size: 48px; margin-top: 30px; color: #333; }\n"
             "</style>\n"
             "</head>\n"
             "<body>\n"
             "<h1>Controle Residencial</h1>\n"
             "<form action=\"./mudar_estado_luz_sala\"><button>Luz da Sala</button></form>\n"
             "<form action=\"./mudar_estado_luz_cozinha\"><button>Luz da Cozinha</button></form>\n"
             "<form action=\"./mudar_estado_luz_quarto\"><button>Luz do Quarto</button></form>\n"
             "<form action=\"./mudar_estado_luz_banheiro\"><button>Luz do Banheiro</button></form>\n"
             "<form action=\"./mudar_estado_luz_quintal\"><button>Luz do Quintal</button></form>\n"
             "<form action=\"./mudar_estado_display\"><button>Televisão</button></form>\n"
              "<p class=\"temperature\">Temperatura Interna: %.2f &deg;C</p>\n"
             "</body>\n"
             "</html>\n",
             temperature);

    // Envia a resposta HTTP
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    // Libera a memória alocada
    free(request);
    pbuf_free(p);

    return ERR_OK;
}