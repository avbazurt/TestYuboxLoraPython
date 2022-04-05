#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <sched.h>
#include <pthread.h>
#include <getopt.h>

#include <pigpio.h>
#include <LoRaWan-Arduino.h>

#include <deque>

// Parámetros configurables de LoRaWAN
static uint8_t nodeDeviceEUI[8] = { 0 };
static uint8_t nodeAppEUI[8] = { 0 };
static uint8_t nodeAppKey[16] = { 0 };
static uint32_t nodeDevAddr = 0;
static uint8_t nodeNwsKey[16] = { 0 };
static uint8_t nodeAppsKey[16] = { 0 };
static uint8_t loraSubBand = 2;
static unsigned int lorawan_app_txduty_cycle_sec = 10;

bool parseArguments(int argc, char * const argv[]);
bool parseEUIString(const char * tag, const char * s, uint8_t * p, size_t n);
static bool parseHexString(const char * s, uint8_t * p, size_t n);
static void buildHexString(const uint8_t * p, char * s, size_t n);

bool setupLoRaWANTerminal(void);
void runLoRaWANTerminal(void);

static pthread_mutex_t printf_mutex = PTHREAD_MUTEX_INITIALIZER;
int locked_printf(const char * format, ...);

typedef struct LoRaWAN_txPacket_t {
    uint8_t * pktdata;
    size_t pktsize;
    uint32_t pktid;
    lmh_confirm confirm;

    // Constructor necesario para que funcione std::deque<>.emplace_back()
    LoRaWAN_txPacket_t(uint8_t *p, size_t n, uint32_t id, lmh_confirm c) : pktdata(p), pktsize(n), pktid(id), confirm(c) {}
} LoRaWAN_txPacket;

static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static std::deque<LoRaWAN_txPacket> txQueue;
static uint32_t pktCount = 0;

static TimerEvent_t txTimer;
static void lorawan_tx_handler(void);

int main(int argc, char * const argv[])
{
    setlinebuf(stdin);
    setlinebuf(stdout);

    // Inicializar pigpio para control de pines
    if (gpioInitialise() < 0) {
        fprintf(stderr, "Fallo al inicializar pigpio!\n");
        return 255;
    }

    // Iniciar timer de TX, pero todavía no iniciarlo
    TimerInit(&txTimer, lorawan_tx_handler);

    // Elevar prioridad de proceso con scheduler SCHED_RR
    struct sched_param sch_param = { .sched_priority = 1 };
    if (0 != sched_setscheduler(0, SCHED_RR, &sch_param)) {
        gpioTerminate();
        fprintf(stderr, "Fallo al iniciar scheduler SCHED_RR!\n");
        return 254;
    }

    // Valores por omisión para EUI de dispositivo
    BoardGetUniqueId(nodeDeviceEUI);
    nodeDevAddr = 
        ((uint32_t)nodeDeviceEUI[0] << 24) |
        ((uint32_t)nodeDeviceEUI[1] << 16) |
        ((uint32_t)nodeDeviceEUI[2] <<  8) |
        ((uint32_t)nodeDeviceEUI[3]);

    if (!parseArguments(argc, argv)) {
        gpioTerminate();
        return 1;
    }

    if (!setupLoRaWANTerminal()) {
        gpioTerminate();
        return 1;
    }

    runLoRaWANTerminal();

    TimerStop(&txTimer);
    gpioTerminate();
    return 0;
}

bool parseArguments(int argc, char * const argv[])
{
    int opt_index = 0;
    static struct option long_options[] = {
        // Mostrar ayuda y salir
        {"help",      0, NULL, 'h'},

        {"deveui",    1, NULL, 'D'},    // EUI del dispositivo para OTAA, cadena hexadecimal de 8 bytes (16 dígitos)
        {"appeui",    1, NULL, 'A'},    // EUI del aplicación para OTAA, cadena hexadecimal de 8 bytes (16 dígitos) (opcional para ChirpStack)
        {"appkey",    1, NULL, 'K'},    // Clave secreta OTAA de dispositivo, cadena hexadecimal de 16 bytes (32 dígitos)
        {"subband",   1, NULL, 'b'},    // Sub-banda inicial para usar en conexión, se pueden negociar adicionales con ADR

        {"skew",      1, NULL, 'S'},    // Offset de milisegundos para compensar por reloj lento de RPI0W

        {"txduty",    1, NULL, 't'},    // Demora mínima entre paquetes transmitidos

        // TODO: región LoRaWAN, activar/desactivar OTAA, credenciales para no-OTAA
        {NULL, 0, NULL, 0}
    };
    static const char * short_options = "hb:A:K:D:t:S:";
    int c;
    bool args_ok = true;
    bool display_help = false;

    int16_t delta = 0;

    while (args_ok && -1 != (c = getopt_long(argc, argv, short_options, long_options, &opt_index))) {
        switch (c) {
        case 'h':
            display_help = true;
            break;
        case 'D':
            args_ok = parseEUIString("devEUI", optarg, nodeDeviceEUI, sizeof(nodeDeviceEUI));
            break;
        case 'A':
            args_ok = parseEUIString("appEUI", optarg, nodeAppEUI, sizeof(nodeAppEUI));
            break;
        case 'K':
            args_ok = parseEUIString("appKey", optarg, nodeAppKey, sizeof(nodeAppKey));
            break;
        case 'b':
            if (0 >= sscanf(optarg, "%hhu", &loraSubBand)) {
                fprintf(stderr, "Valor de sub-banda no-numérico: %s\n", optarg);
                args_ok = false;
            } else if (!(loraSubBand >= 1 && loraSubBand <= 8)) {
                fprintf(stderr, "Valor de sub-banda no está en rango 1..8: %s\n", optarg);
                args_ok = false;
            }
            break;
        case 'S':
            if (0 >= sscanf(optarg, "%hd", &delta)) {
                fprintf(stderr, "Valor de delta msec no-numérico: %s\n", optarg);
                args_ok = false;
            }
            break;
        case 't':
            if (0 >= sscanf(optarg, "%u", &lorawan_app_txduty_cycle_sec)) {
                fprintf(stderr, "Valor de txduty sec no-numérico: %s\n", optarg);
                args_ok = false;
            } else if (lorawan_app_txduty_cycle_sec <= 0) {
                fprintf(stderr, "Valor de txduty sec no debe ser cero o negativo: %s\n", optarg);
                args_ok = false;
            }
            break;
        }
    }

    if (delta != 0) LoRaMacMsecSkew += delta;

    if (display_help) {
        const char * help_text = "\
Modo de empleo: lorawan-rpi [OPCIÓN]...\n\
Herramienta de comunicación LoRaWAN para Raspberry Pi con el HAT LoRaWAN YUBOX\n\
Este programa debe ser usado como una tubería que recibe órdenes vía entrada\n\
estándar desde otro programa, y comunica eventos y resultados vía salida\n\
estándar hacia el programa.\n\
\n\
Opciones de invocación:\n\
  -h, --help        Muestra esta ayuda y sale\n\
  -D, --deveui      Cadena hexadecimal de 16 dígitos que identifica al disp. LoRa\n\
  -A, --appeui      Cadena hexadecimal de 16 dígitos que identifica aplicación\n\
                    LoRaWAN (no necesario para ChirpStack)\n\
  -K, --appkey      Cadena hexadecimal de 32 dígitos que sirve de clave OTAA\n\
  -b, --subband     Sub-banda inicial a usar para unión a red LoRaWAN\n\
  -S, --skew        Número de milisegundos adicionales que han transcurrido más\n\
                    allá de 1000 milisegundos cuando el sistema ha contado 1000\n\
                    milisegundos. Para combatir reloj lento en Raspberry Pi Zero W\n\
                    se recomienda un valor de mínimo 6.\n\
  -t, --txduty      Intervalo en segundos (por omisión 10) a esperar entre\n\
                    transmisiones. Un valor menor implica riesgo mayor de\n\
                    colisión con otros dispositivos LoRaWAN en el mismo canal.\n\
\n\
INTERACCIÓN\n\
Este programa debe invocarse con privilegios de root (usando sudo).\n\
\n\
Los comandos reconocidos son:\n\
\n\
EXIT                Finaliza el programa\n\
SEND xxxxxx         Encola la trama xxxxxx que se asume codificada como dígitos\n\
                    hexadecimales. La trama NO se envía de inmediato, sino que\n\
                    se espera a la siguiente ventana de transmisión según el\n\
                    valor de --txduty.\n\
    OK PACKET nn QUEUED\n\
                    Respuesta al comando SEND. A la trama se le asigna el número\n\
                    nn que será luego emitido otra vez en el evento\n\
                    EVENT PACKET nn SENT|FAIL\n\
CSND xxxxxx         Encola la trama xxxxxx en modo de envío confirmado. Si no se\n\
                    recibe respuesta de que la trama fue recibida, será reenviada\n\
                    varias veces.\n\
\n\
Al ejecutarse, el programa mostrará los siguientes eventos:\n\
\n\
EVENT JOIN START    Inicia la negociación para unirse a la red LoRaWAN\n\
EVENT JOIN OK       Negociación de unión a red exitosa, se establece comunciación\n\
EVENT JOIN FAIL     Negociación de unión a red ha fallado. Puede que se requiera\n\
                    un valor más alto de --skew\n\
EVENT RX RSSI nn SNR nn HEXDATA xxxxxx\n\
                    Se ha recibido una trama de datos, codificada como dígitos\n\
                    hexadecimales. Los valores de RSSI y SNR son informativos.\n\
EVENT PACKET nn SENT\n\
                    El paquete nn se ha mandado a enviar por la red.\n\
EVENT PACKET nn FAIL\n\
                    El paquete nn no pudo enviarse por la red y ha sido desechado.\n\
\n";
        puts(help_text);
        args_ok = false;
    }

    return args_ok;
}

bool parseEUIString(const char * tag, const char * s, uint8_t * p, size_t n)
{
    if (strlen(s) != 2 * n) {
        fprintf(stderr, "Secuencia para %s debe ser de exactamente %u dígitos hexadecimales\n", tag, 2 * n);
        return false;
    }

    if (!parseHexString(s, p, n)) {
        fprintf(stderr, "Secuencia para %s no es una secuencia hexadecimal válida\n", tag);
        return false;
    }
    return true;
}



#define JOINREQ_NBTRIALS 3			   /**< Number of trials for the join request. */

static hw_config hwConfig;

/*

EBYTE   PINOUT   RPI BOARD PIGPIO
-----   -------- --------- ------
MOSI    SPI_MOSI 19        10
MISO    SPI_MISO 21         9
SCK     SPI_CLK  23        11
NSS     SPI_CS   24         8 CE0
BUSY    LORA2    35        19
DIO1    LORA1    36        16
RXEN    LORA5    37        26
NRST    LORA3    38        20
TXEN    LORA4    40        21

*/
const int PIN_LORA_NSS  =  8;  // LORA SPI CS
const int PIN_LORA_SCLK =  11;  // LORA SPI CLK
const int PIN_LORA_MISO =   9;  // LORA SPI MISO
const int PIN_LORA_MOSI =  10;  // LORA SPI MOSI
const int PIN_LORA_RESET=  20;  // LORA RESET
const int PIN_LORA_BUSY =  19;  // LORA SPI BUSY
const int PIN_LORA_DIO_1=  16;  // LORA DIO_1
const int RADIO_TXEN    =  21;  // LORA ANTENNA TX ENABLE
const int RADIO_RXEN    =  26;  // LORA ANTENNA RX ENABLE

// Foward declaration
static void lorawan_has_joined_handler(void);
static void lorawan_rx_handler(lmh_app_data_t *app_data);
static void lorawan_confirm_class_handler(DeviceClass_t Class);
static void lorawan_join_failed_handler(void);

/**@brief Structure containing LoRaWan parameters, needed for lmh_init()
*/
static lmh_param_t lora_param_init = {
    LORAWAN_ADR_ON,
    LORAWAN_DEFAULT_DATARATE,
    LORAWAN_PUBLIC_NETWORK,
    JOINREQ_NBTRIALS,
    LORAWAN_DEFAULT_TX_POWER,
    LORAWAN_DUTYCYCLE_OFF
};

/**@brief Structure containing LoRaWan callback functions, needed for lmh_init()
*/
static lmh_callback_t lora_callbacks = {
    BoardGetBatteryLevel,
    BoardGetUniqueId,
    BoardGetRandomSeed,
    lorawan_rx_handler,
    lorawan_has_joined_handler,
    lorawan_confirm_class_handler,
    lorawan_join_failed_handler
};

bool setupLoRaWANTerminal(void)
{
    uint32_t r;

    // Define the HW configuration between MCU and SX126x
    hwConfig.CHIP_TYPE = SX1262_CHIP;         // Example uses an eByte E22 module with an SX1262
    hwConfig.PIN_LORA_RESET = PIN_LORA_RESET; // LORA RESET
    hwConfig.PIN_LORA_NSS = PIN_LORA_NSS;     // LORA SPI CS
    hwConfig.PIN_LORA_SCLK = PIN_LORA_SCLK;   // LORA SPI CLK
    hwConfig.PIN_LORA_MISO = PIN_LORA_MISO;   // LORA SPI MISO
    hwConfig.PIN_LORA_DIO_1 = PIN_LORA_DIO_1; // LORA DIO_1
    hwConfig.PIN_LORA_BUSY = PIN_LORA_BUSY;   // LORA SPI BUSY
    hwConfig.PIN_LORA_MOSI = PIN_LORA_MOSI;   // LORA SPI MOSI
    hwConfig.RADIO_TXEN = RADIO_TXEN;         // LORA ANTENNA TX ENABLE
    hwConfig.RADIO_RXEN = RADIO_RXEN;         // LORA ANTENNA RX ENABLE
    hwConfig.USE_DIO2_ANT_SWITCH = false;     // Example uses an CircuitRocks Alora RFM1262 which uses DIO2 pins as antenna control
    hwConfig.USE_DIO3_TCXO = true;            // Example uses an CircuitRocks Alora RFM1262 which uses DIO3 to control oscillator voltage
    hwConfig.USE_DIO3_ANT_SWITCH = false;     // Only Insight ISP4520 module uses DIO3 as antenna control

    r = lora_hardware_init(hwConfig);
    if (r != 0) {
        fprintf(stderr, "lora_hardware_init() falla: %d\n", r);
        locked_printf("ERR HARDWARE FAIL %d\n", r);
        return false;
    }

    // Setup the EUIs and Keys
    lmh_setDevEui(nodeDeviceEUI);
    lmh_setAppEui(nodeAppEUI);
    lmh_setAppKey(nodeAppKey);
    lmh_setNwkSKey(nodeNwsKey);
    lmh_setAppSKey(nodeAppsKey);
    lmh_setDevAddr(nodeDevAddr);

    // Initialize LoRaWan
    r = lmh_init(&lora_callbacks, lora_param_init, true, CLASS_A, LORAMAC_REGION_US915);
    if (r != 0) {
        fprintf(stderr, "lmh_init() falla: %d\n", r);
        locked_printf("ERR LORAMAC FAIL %d\n", r);
        lora_hardware_uninit();
        return false;
    }

    // Seleccionar sub-banda donde escucha el gateway LoRaWAN
    if (!lmh_setSubBandChannels(loraSubBand)) {
        fprintf(stderr, "lmh_setSubBandChannels(%d) falla - banda posiblemente incorrecta\n", loraSubBand);
        locked_printf("ERR SUBBAND %u FAIL\n", loraSubBand);
        lora_hardware_uninit();
        return false;
    }

    // Iniciar negociación para unirse a red LoRaWAN...
    lmh_join();
    locked_printf("EVENT JOIN START\n");

    return true;
}

#define MAX_INPUT_LINE 2048

static void enqueueTxPacket(const char * hexstring, lmh_confirm confirm);
void runLoRaWANTerminal(void)
{
    bool exit_request = false;

    char * input_line = (char *)malloc(MAX_INPUT_LINE + 1);

    if (NULL != input_line) {
        while (!feof(stdin) && !exit_request) {
            // Leer una línea de STDIN
            if (NULL == fgets(input_line, MAX_INPUT_LINE + 1, stdin)) {
                // La biblioteca pigpio instala un manejador de señal que
                // colisiona con SIGWINCH, ocasionando que un redimensionado
                // de ventana interrumpa la llamada read() interior a
                // fgets(). En lugar de usar break; para interrumpir el bucle
                // se usa continue; para volver a evaluar la condición de
                // EOF en stdin, la cual sí debe ocasionar que finalice
                // este programa.
                continue;
            }

            // Identificar qué se requiere hacer
            if (strstr(input_line, "EXIT") == input_line) {
                exit_request = true;
            } else if (strstr(input_line, "SEND ") == input_line) {
                char * hexstring = input_line + 5;
                size_t pos = strspn(hexstring, " \t");
                hexstring += pos;
                pos = strspn(hexstring, "0123456789abcdefABCDEF");
                hexstring[pos] = '\0';

                // Ahora hexstring debería ser una cadena de 0 o más dígitos hex
                enqueueTxPacket(hexstring, LMH_UNCONFIRMED_MSG);
            } else if (strstr(input_line, "CSND ") == input_line) {
                char * hexstring = input_line + 5;
                size_t pos = strspn(hexstring, " \t");
                hexstring += pos;
                pos = strspn(hexstring, "0123456789abcdefABCDEF");
                hexstring[pos] = '\0';

                // Ahora hexstring debería ser una cadena de 0 o más dígitos hex
                enqueueTxPacket(hexstring, LMH_CONFIRMED_MSG);
            } else if (*input_line != '\0' && *input_line != '\n') {
                locked_printf("ERR UNRECOGNIZED COMMAND\n");
            }
        }

        free(input_line);
    }
}

static void enqueueTxPacket(const char * hexstring, lmh_confirm confirm)
{
    size_t n = strlen(hexstring);
    if (n <= 0 || (n % 2)) {
        locked_printf("ERR INVALID HEX FRAME\n");
    } else {
        n >>= 1;

        uint8_t * pktdata = (uint8_t *)malloc(n);
        if (NULL != pktdata) {
            if (!parseHexString(hexstring, pktdata, n)) {
                locked_printf("ERR INVALID HEX FRAME\n");
            } else {
                pthread_mutex_lock(&queue_mutex);
                txQueue.emplace_back(pktdata, n, ++pktCount, confirm);
                pthread_mutex_unlock(&queue_mutex);

                locked_printf("OK PACKET %u QUEUED\n", pktCount);
            }

        } else {
            locked_printf("ERR MALLOC FAIL\n");
        }
    }
}

/**@brief LoRa function for handling OTAA join failed
*/
static void lorawan_join_failed_handler(void)
{
    locked_printf("EVENT JOIN FAIL\n");

    lmh_join();
    locked_printf("EVENT JOIN START\n");
}

/**@brief LoRa function for handling HasJoined event.
*/
static void lorawan_has_joined_handler(void)
{
    locked_printf("EVENT JOIN OK\n");

    lmh_class_request(CLASS_A);

    // Timer regular para transmitir paquetes
    txTimer.oneShot = false;
    TimerSetValue(&txTimer, lorawan_app_txduty_cycle_sec * 1000);
    TimerStart(&txTimer);
}

static void lorawan_confirm_class_handler(DeviceClass_t cl)
{
    locked_printf("EVENT SWITCH CLASS %c\n", "ABC"[cl]);

    // Informs the server that switch has occurred ASAP
    lmh_app_data_t m_lora_app_data = { NULL, 0, LORAWAN_APP_PORT, 0, 0 };
    lmh_send(&m_lora_app_data, LMH_UNCONFIRMED_MSG);
}

/**@brief Function for handling LoRaWan received data from Gateway

   @param[in] app_data  Pointer to rx data
*/
static void lorawan_rx_handler(lmh_app_data_t *app_data)
{
    char * hexdata;

    fprintf(stderr, "LoRa Packet received on port %d, size:%d, rssi:%d, snr:%d\n",
        app_data->port, app_data->buffsize, app_data->rssi, app_data->snr);

    switch (app_data->port) {
    case 3:
        // Port 3 switches the class
        if (app_data->buffsize == 1) {
            switch (app_data->buffer[0]) {
            case 0:
                lmh_class_request(CLASS_A);
                break;

            case 1:
                lmh_class_request(CLASS_B);
                break;

            case 2:
                lmh_class_request(CLASS_C);
                break;

            default:
                break;
            }
        }
        break;

    case LORAWAN_APP_PORT:
        // YOUR_JOB: Take action on received data
        hexdata = (char *)malloc(2 * app_data->buffsize + 1);
        if (hexdata != NULL) {
            buildHexString(app_data->buffer, hexdata, app_data->buffsize);
            locked_printf("EVENT RX RSSI %d SNR %d HEXDATA %s\n",
                app_data->rssi, app_data->snr, hexdata);
            free(hexdata);
        }
        break;

    default:
        break;
    }
}

static void lorawan_tx_handler(void)
{
    lmh_app_data_t lora_app_data = { NULL, 0, LORAWAN_APP_PORT, 0, 0 };
    uint32_t pktid = 0;
    lmh_confirm confirm = LMH_UNCONFIRMED_MSG;

    pthread_mutex_lock(&queue_mutex);
    if (txQueue.size() > 0) {
        lora_app_data.buffer = txQueue.front().pktdata;
        lora_app_data.buffsize = txQueue.front().pktsize;
        pktid = txQueue.front().pktid;
        confirm = txQueue.front().confirm;

        txQueue.pop_front();
    }
    pthread_mutex_unlock(&queue_mutex);

    lmh_error_status lmh_res = lmh_send(&lora_app_data, confirm);
    if (LMH_SUCCESS == lmh_res) {
        if (pktid != 0) locked_printf("EVENT PACKET %u SENT\n", pktid);
    } else {
        fprintf(stderr, "lmh_send() falla: %d\n", lmh_res);
        locked_printf("EVENT PACKET %u FAIL\n", pktid);
    }

    if (lora_app_data.buffer != NULL) {
        free(lora_app_data.buffer);
        lora_app_data.buffer = NULL;
        lora_app_data.buffsize = 0;
    }

    // Si ha fallado la transmisión con datos, se intenta transmitir un paquete
    // vacío por si la falla es debida a paquete máximo no negociado.
    if (LMH_ERROR == lmh_res && pktid != 0) {
        lmh_res = lmh_send(&lora_app_data, LMH_UNCONFIRMED_MSG);
        if (LMH_SUCCESS != lmh_res) {
            fprintf(stderr, "lmh_send() (vacío) falla: %d\n", lmh_res);
        }
    }
}

int locked_printf(const char * format, ...)
{
    pthread_mutex_lock(&printf_mutex);

    va_list ap;
    va_start(ap, format);
    int r = vprintf(format, ap);
    va_end(ap);

    pthread_mutex_unlock(&printf_mutex);

    return r;
}

// Parsear una cadena hexadecimal y construir la trama binaria correspondiente.
// El búfer apuntado por s debe tener 2*n dígitos hexadecimales, y el búfer
// apuntado por p debe tener espacio disponible de n bytes.
static bool parseHexString(const char * s, uint8_t * p, size_t n)
{
    while (n > 0) {
        char buf[3];
        buf[0] = s[0];
        buf[1] = s[1];
        buf[2] = '\0';
        if (sscanf(buf, "%hhx", p) < 1) {
            return false;
        }

        n--; p++; s += 2;
    }
    return true;
}

// Generar una cadena hexadecimal a partir de la trama binaria indicada en p.
// El búfer apuntado por s debe tener 2*n+1 bytes disponibles para incluir un
// caracter nulo de terminación.
static void buildHexString(const uint8_t * p, char * s, size_t n)
{
    memset(s, '\0', 2*n + 1);
    while (n > 0) {
        sprintf(s, "%02hhX", *p);

        n--; p++; s += 2;
    }
}