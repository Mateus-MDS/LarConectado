LarConectado: 
Automação Residencial com Raspberry Pi Pico W

Descrição
LarConectado é um sistema de automação residencial baseado na Raspberry Pi Pico W que permite o controle de LEDs, exibição de temperatura, e monitoramento de sensores por meio de uma interface web, sem uso de bibliotecas externas, utilizando a pilha TCP/IP lwIP diretamente.

O sistema também inclui um display OLED SSD1306, sensores de ambiente como ultrassônico e LDR, uma matriz de LEDs 5x5 via PIO, e monitora a temperatura interna do chip RP2040.

Objetivos
Controlar LEDs remotamente por página web.

Exibir a temperatura interna da Pico via navegador.

Monitorar iluminação ambiente (LDR) e presença (ultrassônico).

Representar informações visuais com matriz de LEDs 5x5.

Exibir status e dados no OLED SSD1306.

Prover interface web responsiva sem uso de bibliotecas externas.

Rodar servidor TCP/IP com lwIP direto na Pico W.

Componentes Utilizados
Componente	Pinos	Função

Raspberry Pi Pico W	-	Microcontrolador com Wi-Fi
LED (x2 ou mais) - GPIOs livres (ex: GP2, GP3) - Atuadores de saída controlados via web
Display OLED SSD1306 - I²C - GP14 (SDA), GP15 (SCL) - Exibição local de dados
Sensor Ultrassônico HC-SR04	GP16 (TRIG), GP17 (ECHO) - Detecção de objeto proximo
Sensor LDR (com divisor) - GP26 (ADC0) - Detecção de luz ambiente
Matriz de LEDs 5×5	PIO - GP7	Visualização gráfica de estados
Interface Web Wi-Fi - Controle e visualização remota

Funcionamento
Inicialização
Configuração de GPIOs, ADC, PIO e periféricos.

Inicialização do display OLED.

Conexão à rede Wi-Fi.

Inicialização da pilha lwIP para serviço TCP/IP.

Upload e parsing do HTML embutido para servidor web.

Interface Web
Página HTML simples acessível via IP da Pico.

Exibe:

Temperatura interna (atualizada dinamicamente).

Botões para acender/apagar LEDs.

Comunicação por sockets TCP/HTTP diretos via lwIP.

Leitura e Monitoramento
Sensor Ultrassônico: distância medida periodicamente.

Sensor LDR: leitura analógica para avaliar luminosidade.

Temperatura Interna: leitura do sensor térmico do RP2040.

Estrutura do Código
Wi-Fi & lwIP Setup

Inicializa interface de rede.

Configura IP fixo ou DHCP.

Inicia servidor HTTP básico.

Drivers

OLED SSD1306 via I²C.

Matriz de LEDs via PIO.

Leitura ADC e sensores digitais.

Web Server

Serve HTML estático com rotas HTTP simples.

Mapeia comandos de botões para GPIOs.

Main Loop

Atualiza sensores.

Atualiza OLED e LED matrix.

Processa requisições HTTP.

Como Executar o Projeto
Monte os componentes conforme a tabela de pinos.

Configure suas credenciais de Wi-Fi no código.

Compile o firmware com suporte à pilha lwIP (sem RTOS).

Grave o firmware na Pico W.

Acesse o IP atribuído à Pico na sua rede local pelo navegador.

Use a interface para controlar LEDs e visualizar dados ao vivo.

Requisitos
Raspberry Pi Pico W

SDK do Raspberry Pi Pico configurado

lwIP (sem RTOS, modo standalone)

Display OLED compatível com SSD1306

Sensor LDR, Ultrassônico HC-SR04, LEDs

Repositório
GitHub: https://github.com/Mateus-MDS/LarConectado.git

Autor
Nome: Mateus Moreira da Silva


