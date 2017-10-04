//===============================
// Header
//===============================
#include <stdint.h>
#include <string.h>
#include <string>
#include <inttypes.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "freertos_plus/FreeRTOS_CLI.h"

#include "tcpip_adapter.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/priv/api_msg.h"
#include "lwip/err.h"

#include "telemetry/c_tlm_var.h"
#include "telemetry/c_tlm_stream.h"

#include "time.h"

//===============================
// MACRO Constants
//===========================/====
#define LED_BUILTIN                  16
#define MAX_CONNECTIONS              4
#define SPECIAL_STRING_LENGTH        64
#define EXTRACT_URL_TEMPLATE_SIZE    128
#define MAX_INPUT_LENGTH             50
#define MAX_OUTPUT_LENGTH            512
//===============================
// MACRO functions
//===============================

//===============================
// Constants Field
//===============================
const static char* TAG = __FILE;

const static char http_html_hdr[]   = "HTTP/1.1 200 OK\r\n"
                                      "Access-Control-Allow-Origin: *\r\n"
                                      "Content-type: text/html\r\n"
                                      "\r\n";
const static char http_sse_hdr[]    = "HTTP/1.1 200 OK\r\n"
                                      "Access-Control-Allow-Origin: *\r\n"
                                      "Content-Type: text/event-stream\r\n"
                                      "Cache-Control: no-cache\r\n"
                                      "Connection: keep-alive\r\n"
                                      "\r\n";
const static char http_index_html[] = "<!DOCTYPE html>"
                                      "<html>\n"
                                      "<head>\n"
                                      "<title>HELLO ESP32</title>\n"
                                      "</head>\n"
                                      "<body>\n"
                                      "<h1>Hello World, from ESP32!</h1>\n"
                                      "</body>\n"
                                      "</html>\n";
const static char http_sse_html[]  = "<!DOCTYPE html>\n"
                                     "<html>\n"
                                     "<head>\n"
                                     "   <title>ESP32 SSE Example</title>\n"
                                     "   <meta name='viewport' content='width=device-width, initial-scale=1'>\n"
                                     "   <style type='text/css'>\n"
                                     "       html, body { height: 100%; background: #f9f9f9; }\n"
                                     "       body { font-family: 'Courier New', Courier, monospace; }\n"
                                     "       #container\n"
                                     "       {\n"
                                     "           position: absolute;\n"
                                     "           top: 50%;\n"
                                     "           left: 50%;\n"
                                     "           transform: translate(-50%, -50%);\n"
                                     "           width: 600px;\n"
                                     "           height: 600px;\n"
                                     "           background: white;\n"
                                     "           border-radius: 10px;\n"
                                     "           display: flex;\n"
                                     "           align-items: center;\n"
                                     "           justify-content: center;\n"
                                     "           flex-direction: column;\n"
                                     "           max-height: 100%;\n"
                                     "           max-width: 100%;\n"
                                     "       }\n"
                                     "       #container div\n"
                                     "       {\n"
                                     "           text-align: center;\n"
                                     "           width: 100%;\n"
                                     "       }\n"
                                     "       #container div button\n"
                                     "       {\n"
                                     "           text-align: center;\n"
                                     "           width: calc(100% / 3.2);\n"
                                     "           border-radius: 5px;\n"
                                     "           border: 1px solid #ccc;\n"
                                     "           background: white;\n"
                                     "           height: 50%;\n"
                                     "       }\n"
                                     "       #container div button:hover\n"
                                     "       {\n"
                                     "           background: #ccc;\n"
                                     "           cursor: pointer;\n"
                                     "       }\n"
                                     "       #container div input\n"
                                     "       {\n"
                                     "           border-radius: 5px;\n"
                                     "           height: 50%;\n"
                                     "           border: 1px solid #ccc;\n"
                                     "       }\n"
                                     "       #container div p { color: red; }\n"
                                     "       #server-data\n"
                                     "       {\n"
                                     "           height: 92.5%;\n"
                                     "           width:90%;\n"
                                     "           border-radius: 5px;\n"
                                     "           border: 1px solid #ccc;\n"
                                     "       }\n"
                                     "   </style>\n"
                                     "</head>\n"
                                     "<body>\n"
                                     "   <div id='container'>\n"
                                     "       <div style='height: 7.5%;margin-top: 2.5%;'>\n"
                                     "           <button onclick='sendData()'>AJAX Send</button>\n"
                                     "           <button onclick='recieveData(this)'>Start Server Side Events</button>\n"
                                     "           <button onclick='toggleInterval(this)'>Toggle Interval</button>\n"
                                     "           <input name='client-data' id='client-data' type='text' style='width:90%' />\n"
                                     "       </div>\n"
                                     "       <div style='height: 3.5%;'> <p id='response'> response </p> </div>\n"
                                     "       <div style='height: 90%; margin-top: 2.5%;'>\n"
                                     "           <textarea id='server-data' ></textarea>\n"
                                     "       </div>\n"
                                     "   </div>\n"
                                     "</body>\n"
                                     "<script type='text/javascript'>\n"
                                     "    var source_sta, source_ap;\n"
                                     "    const URL = 'http://192.168.4.1';\n"
                                     "    // const URL = 'http://192.168.0.104';\n"
                                     "    var response = document.querySelector('#response');\n"
                                     "    var busy_counter = 0;\n"
                                     "    function recieveData(obj)\n"
                                     "    {\n"
                                     "       if(source_ap)\n"
                                     "       {\n"
                                     "           obj.style.background = 'red';\n"
                                     "           response.innerHTML = 'Resetting Event Source';\n"
                                     "           source_ap.close();\n"
                                     "           source_ap = null;\n"
                                     "       }\n"
                                     "       else\n"
                                     "       {\n"
                                     "           obj.style.background = 'limegreen';\n"
                                     "           response.innerHTML = 'Connecting...';\n"
                                     "           source_ap = new EventSource(URL);\n"
                                     "           source_ap.onopen = function()\n"
                                     "           {\n"
                                     "               response.innerHTML = 'Connected';\n"
                                     "           };\n"
                                     "           source_ap.onmessage = function(event)\n"
                                     "           {\n"
                                     "               console.log(event);\n"
                                     "               var text_area = document.querySelector('#server-data');\n"
                                     "               text_area.innerHTML = `id = ${event.lastEventId} :: data = ${event.data}\n` +  text_area.innerHTML;\n"
                                     "           };\n"
                                     "           source_ap.onerror = function(event)\n"
                                     "           {\n"
                                     "               response.innerHTML = `failed to connect to ${URL}`;\n"
                                     "               source_ap.close();\n"
                                     "               source_ap = null;\n"
                                     "           };\n"
                                     "       }\n"
                                     "    }\n"
                                     "    function sendData()\n"
                                     "    {\n"
                                     "       var oReq = new XMLHttpRequest();\n"
                                     "       var value = document.querySelector('#client-data').value.replace(/ /g, '_');\n"
                                     "       oReq.open('GET', `${URL}?data=${value}`);\n"
                                     "       oReq.send();\n"
                                     "    }\n"
                                     "    var counter = 0;\n"
                                     "    var interval;\n"
                                     "    function toggleInterval(obj)\n"
                                     "    {\n"
                                     "       // console.log(interval);\n"
                                     "       console.log(obj);\n"
                                     "       if(interval)\n"
                                     "       {\n"
                                     "           clearInterval(interval);\n"
                                     "           interval = null;\n"
                                     "           obj.style.background = 'red';\n"
                                     "       }\n"
                                     "       else\n"
                                     "       {\n"
                                     "           obj.style.background = 'limegreen';\n"
                                     "           interval = setInterval(() =>\n"
                                     "           {\n"
                                     "               var oReq = new XMLHttpRequest();\n"
                                     "               var value = `testing_${counter++}`;\n"
                                     "               console.log(value);\n"
                                     "               oReq.open('GET', `${URL}?data=${value}`);\n"
                                     "               oReq.send();\n"
                                     "           }, 40);\n"
                                     "       }\n"
                                     "    }\n"
                                     "</script>\n"
                                     "</html>\n";

const static char success[]         = "success";
const static char failure[]         = "failure";
const static char notfound_404[]    = "404 not found!";
//===============================
// Structures and Enumerations
//===============================
//===============================
// Global Variables
//===============================
//// WiFi
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
struct netconn * clients[MAX_CONNECTIONS] = { NULL };
char special_string[SPECIAL_STRING_LENGTH] = "special";
SemaphoreHandle_t xSemaphore;
//===============================
// Function Fields
//===============================
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            printf("got ip\n");
            printf("ip: " IPSTR "\n", IP2STR(&event->event_info.got_ip.ip_info.ip));
            printf("netmask: " IPSTR "\n", IP2STR(&event->event_info.got_ip.ip_info.netmask));
            printf("gw: " IPSTR "\n", IP2STR(&event->event_info.got_ip.ip_info.gw));
            printf("\n");
            fflush(stdout);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            /* This is a workaround as ESP32 WiFi libs don't currently
                 auto-reassociate.
                But for this project's purposes, do not reconnect!
            */
            // esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:

            break;
        default:
            break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );

    wifi_config_t sta_config;
    memcpy(sta_config.sta.ssid, "Chen Family", 32);
    memcpy(sta_config.sta.password, "June15#a", 32);
    sta_config.sta.bssid_set = false;

    wifi_config_t ap_config;
    memcpy(ap_config.ap.ssid, "SSE-ESP32-EXAMPLE", 32);
    ap_config.ap.ssid_len = 0;
    memcpy(ap_config.ap.password, "testing1234", 32);
    ap_config.ap.channel = 3;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.beacon_interval = 500;
    ap_config.ap.max_connection = 16;

    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_AP, &ap_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}

bool extract_url_variable(const char * variable,
                          const char * url,
                          uint32_t url_length,
                          char * dest,
                          uint32_t dest_length)
{
    bool success = false;
    char template_string[EXTRACT_URL_TEMPLATE_SIZE] = { 0 };
    char * variable_found = strstr(url, variable);
    if (variable_found != NULL)
    {
        snprintf(template_string, EXTRACT_URL_TEMPLATE_SIZE, "%s=%%%d[^&\\ \n\r]", variable, dest_length);
        sscanf(variable_found, (const char *)template_string, dest);
        success = true;
    }
    return success;
}

bool add_sse_client(struct netconn * conn)
{
    bool success = false;
    for (int i = 0; i < MAX_CONNECTIONS; ++i)
    {
        if (clients[i] == NULL)
        {
            //// 75 seconds
            // conn->so_options |= SOF_KEEPALIVE;
            // conn->keep_intvl = 75000;
            clients[i] = conn;
            printf("Added SSE to Channel (%d)\n", i);
            success = true;
            break;
        }
    }
    if (!success)
    {
        printf("Refused! SSE Channels full!\n");
    }
    return success;
}

static void http_server_netconn_serve(void *pvParameters)
{
    struct netconn *conn = (struct netconn *)pvParameters;
    struct netbuf *inbuf;
    char *buf;
    uint16_t buflen;
    bool close_flag = true;
    char url[256] = { 0 };
    char * is_event_stream = NULL;
    err_t err;

    /* Read the data from the port, blocking if nothing yet there.
     We assume the request (the part we care about) is in one netbuf */
    err = netconn_recv(conn, &inbuf);

    if (err == ERR_OK)
    {
        netbuf_data(inbuf, (void**)&buf, &buflen);
        //// NULL Terminate end of buffer
        buf[buflen] = '\0';
        //// Attempt to find "Accept: text/event-stream" string within request
        //// Adding the first and last "\r\n" means that this is not the first or last line
        is_event_stream = strstr(buf, "\r\nAccept: text/event-stream\r\n");
        //// Output captured http request information
        // printf("buf = %.*s\n", buflen, buf);
        // printf("ACCEPT = 0x%X\n", is_event_stream);

        if(is_event_stream != NULL)
        {
            if (add_sse_client(conn))
            {
                netconn_write(conn, http_sse_hdr, sizeof(http_sse_hdr) - 1, NETCONN_NOCOPY);
                close_flag = false;
            }
        }
        else
        {
            sscanf(buf, "GET %255s HTTP/1.1\n", url);
            // printf("url = %s\n", url);
            //// Beyond this point, I am only sending html data back!
            netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
            //// More cases
            if (strstr(url, "/?") != NULL)
            {
                bool success_flag = extract_url_variable(
                    "data",
                    url,
                    strlen(url),
                    special_string,
                    SPECIAL_STRING_LENGTH
                );
                bool success_flag = extract_url_variable(
                    "speed",
                    url,
                    strlen(url),
                    special_string,
                    SPECIAL_STRING_LENGTH
                );
                // printf("%d)%s\n", success_flag, special_string);
                if (success_flag)
                {
                    netconn_write(conn, success, sizeof(success) - 1, NETCONN_NOCOPY);
                }
                else
                {
                    netconn_write(conn, failure, sizeof(failure) - 1, NETCONN_NOCOPY);
                }
            }
            else if (strstr(url, "/sse") != NULL)
            {
                netconn_write(conn, http_sse_html, sizeof(http_sse_html) - 1, NETCONN_NOCOPY);
            }
            else if (strstr(url, "/") != NULL || strstr(url, "/index") != NULL)
            {
                netconn_write(conn, http_index_html, sizeof(http_index_html) - 1, NETCONN_NOCOPY);
            }
            else
            {
                netconn_write(conn, notfound_404, sizeof(notfound_404) - 1, NETCONN_NOCOPY);
            }
        }
    }
    else
    {
        printf("err = %d \n", err);
    }

    netbuf_delete(inbuf);

    if (close_flag)
    {
        netconn_close(conn);
        if(err == ERR_OK)
        {
            netconn_free(conn);
        }
        else
        {
            printf("DID NOT FREE NETCONN BECAUSE err != ERR_OK => %d\n", err);
        }
    }

    /* Delete this thread, it is done now. Sleep precious child. */
    vTaskDelete(NULL);
}

inline char int_to_ascii_hex(uint32_t n)
{
    char c = '0';
    if(n < 10)
    {
        c = '0'+n;
    }
    else
    {
        c = 'A'+(n-10);
    }
    return c;
}

static void handle_sse(void *pvParameters)
{
    char sse_buffer[128] = { 0 };
    uint32_t sse_id = 0;

    uint32_t heap = esp_get_free_heap_size();
    uint32_t tasks = 0;
    uint32_t random = 15;

    tlm_component * bucket = tlm_component_add("http_server");
    // Add variables to bucket
    TLM_REG_VAR(bucket, heap, tlm_uint);
    TLM_REG_VAR(bucket, tasks, tlm_uint);
    TLM_REG_VAR(bucket, random, tlm_uint);

    while (true)
    {
        // printf("ram=%d\n",esp_get_free_heap_size());
        sprintf(sse_buffer, "id: %08X\ndata: (_) :: %s\n\n\r\n", sse_id, special_string);

        tasks = uxTaskGetNumberOfTasks();
        heap  = esp_get_free_heap_size();
        random++;

        for (int i = 0; i < MAX_CONNECTIONS; ++i)
        {
            if (clients[i] != NULL)
            {
                sse_buffer[20] = int_to_ascii_hex(i);
                err_t error = netconn_write(
                                clients[i],
                                sse_buffer,
                                strlen(sse_buffer) - 1,
                                NETCONN_COPY
                            );
                // printf(sse_buffer);
                if (error != ERR_OK)
                {
                    printf("connection #%d shows error %d\n", i, error);
                    netconn_close(clients[i]);
                    netconn_delete(clients[i]);
                    clients[i] = NULL;
                }
            }
        }

        sse_id++;
        vTaskDelay(200);
    }
}

static void http_server(void *pvParameters)
{
    struct netconn *conn, *newconn;
    err_t err;
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, NULL, 80);
    netconn_listen(conn);

    do
    {
        err = netconn_accept(conn, &newconn);
        if (err == ERR_OK)
        {
            // printf("new conn+task :: %d :: %d :: %d\n", tasks, esp_get_free_heap_size());
            xTaskCreate(http_server_netconn_serve, "http_server_netconn_serve", 2048,  newconn, 2, NULL);
        }
    }
    while (err == ERR_OK);
    /* Delete Listening Server */
    netconn_close(conn);
    netconn_delete(conn);
}

/* This function implements the behaviour of a command, so must have the correct
prototype. */
static BaseType_t prvTaskStatsCommand(char *pcWriteBuffer,
                                      size_t xWriteBufferLen,
                                      const char *pcCommandString )
{
    /* For simplicity, this function assumes the output buffer is large enough
    to hold all the text generated by executing the vTaskList() API function,
    so the xWriteBufferLen parameter is not used. */
    ( void ) xWriteBufferLen;

    /* pcWriteBuffer is used directly as the vTaskList() parameter, so the table
    generated by executing vTaskList() is written directly into the output
    buffer. */
    // vTaskList( pcWriteBuffer + strlen( pcHeader ) );

    sprintf( (char *)pcWriteBuffer, "Hello World!\r\n" );

    /* The entire table was written directly to the output buffer.  Execution
    of this command is complete, so return pdFALSE. */
    return pdFALSE;
}

static void stream_tlm(const char *s, void *arg)
{
    printf(s);
}

static BaseType_t prvTelemetry(char *pcWriteBuffer,
                               size_t xWriteBufferLen,
                               const char *pcCommandString )
{
    char *pcParameter;
    BaseType_t lParameterStringLength, xReturn;

    /* Note that the use of the static parameter means this function is not reentrant. */
    static BaseType_t lParameterNumber = 0;
    static const uint32_t PARAM_STRING_LENGTH = 128;
    static char params[3][PARAM_STRING_LENGTH];

    if( lParameterNumber == 0 )
    {
        /* lParameterNumber is 0, so this is the first time the function has been
        called since the command was entered.  Return the string "The parameters
        were:" before returning any parameter strings. */
        // sprintf( pcWriteBuffer, "The parameters were:\r\n" );

        /* Next time the function is called the first parameter will be echoed
        back. */
        lParameterNumber = 1L;

        /* There is more data to be returned as no parameters have been echoed
        back yet, so set xReturn to pdPASS so the function will be called again. */
        xReturn = pdPASS;
    }
    else
    {
        /* lParameter is not 0, so holds the number of the parameter that should
        be returned.  Obtain the complete parameter string. */
        pcParameter = (char *)FreeRTOS_CLIGetParameter(
                                    /* The command string itself. */
                                    pcCommandString,
                                    /* Return the next parameter. */
                                    lParameterNumber,
                                    /* Store the parameter string length. */
                                    &lParameterStringLength
                                );

        if( pcParameter != NULL && lParameterNumber < 4)
        {
            /* There was another parameter to return.  Copy it into pcWriteBuffer.
            in the format "[number]: [Parameter String". */
            // memset( pcWriteBuffer, 0x00, xWriteBufferLen );
            // sprintf( pcWriteBuffer, "%d: %.*s\r\n", lParameterNumber, lParameterStringLength, pcParameter );

            sprintf(params[lParameterNumber-1], "%.*s", lParameterStringLength, pcParameter);

            /* There might be more parameters to return after this one, so again
            set xReturn to pdTRUE. */
            xReturn = pdTRUE;
            lParameterNumber++;
        }
        else
        {
            /* No more parameters were found. Or parameter count reached above 3 */
            /* Make sure the write buffer does not contain a valid string to
            prevent junk being printed out. */
            pcWriteBuffer[ 0 ] = 0x00;

            /* There is no more data to return, so this time set xReturn to
            pdFALSE. */
            xReturn = pdFALSE;

            /* Execute based on parameter length. */
            switch(lParameterNumber)
            {
                case 4:
                   if (tlm_variable_set_value(params[0], params[1], params[2]))
                    {
                        printf("%s:%s set to %s\n", params[0], params[1], params[2]);
                    }
                    else
                    {
                        printf("Failed to set %s:%s to %s\n", params[0], params[1], params[2]);
                    }
                    break;
                case 2:
                    if(strncmp(params[0], "ascii", PARAM_STRING_LENGTH) == 0)
                    {
                        tlm_stream_all(stream_tlm, NULL, true);
                        break;
                    }
                default:
                    sprintf(pcWriteBuffer, "Invalid telemetry command!\r\n");
            }

            /* Start over the next time this command is executed. */
            lParameterNumber = 0;

        }
    }

    return xReturn;
}

static const char * const pcWelcomeMessage =
    "\r\n"
    "FreeRTOS command server"
    "\r\n"
    "========================="
    "\r\n"
    "Type 'help' to view a list of registered commands"
    "\r\n";
const char CLI_PROMPT[] = "\nCLI> ";
const int cli_prompt_timeout = 120000;

void vCommandConsoleTask( void *pvParameters )
{
    // Peripheral_Descriptor_t xConsole;
    char cRxedChar               = 0xFF;
    int cInputIndex              = 0;
    BaseType_t xMoreDataToFollow = pdFALSE;
    int cli_prompt_counter       = 0;
    /* The input and output buffers are declared static to keep them off the stack. */
    char pcOutputString[ MAX_OUTPUT_LENGTH ], pcInputString[ MAX_INPUT_LENGTH ] = { 0 };
    struct timeval start;
    struct timeval end;

    /* This code assumes the peripheral being used as the console has already
    been opened and configured, and is passed into the task as the task
    parameter.  Cast the task parameter to the correct type. */

    static const CLI_Command_Definition_t xStatsCommand =
    {
        "stats",
        "stats: Outputs some status information.\r\n",
        prvTaskStatsCommand,
        0
    };

    static const CLI_Command_Definition_t xTelemetryCommand =
    {
        "telemetry",
        "telemetry: Inspect and alter variables in real time.\r\n",
        prvTelemetry,
        -1
    };

    FreeRTOS_CLIRegisterCommand( &xStatsCommand );
    FreeRTOS_CLIRegisterCommand( &xTelemetryCommand );

    /* Send a welcome message to the user knows they are connected. */
    // FreeRTOS_write( xConsole, pcWelcomeMessage, strlen( pcWelcomeMessage ) );
    printf(pcWelcomeMessage);
    printf("---------------------------------------------------\r\n");
    printf("AVAILABLE COMMANDS:\r\n");
    do
    {
        xMoreDataToFollow = FreeRTOS_CLIProcessCommand("help", pcOutputString, MAX_OUTPUT_LENGTH);
        printf("    %s", pcOutputString);
    } while( xMoreDataToFollow != pdFALSE );
    printf("---------------------------------------------------\r\n");
    printf(CLI_PROMPT);

    for( ;; )
    {
        /* This implementation reads a single character at a time.  Wait in the
        Blocked state until a character is received. */
        // FreeRTOS_read( xConsole, &cRxedChar, sizeof( cRxedChar ) );
        cRxedChar = getchar();

        if( cRxedChar == '\n' )
        {
            /* A newline character was received, so the input command string is
            complete and can be processed.  Transmit a line separator, just to
            make the output easier to read. */
            // FreeRTOS_write( xConsole, "\r\n", strlen( "\r\n" );
            printf("\r\n");

            gettimeofday(&start, NULL);
            /* The command interpreter is called repeatedly until it returns
            pdFALSE.  See the "Implementing a command" documentation for an
            exaplanation of why this is. */
            do
            {
                /* Send the command string to the command interpreter.  Any
                output generated by the command interpreter will be placed in the
                pcOutputString buffer. */
                xMoreDataToFollow = FreeRTOS_CLIProcessCommand(
                                  pcInputString,    /* The command string.*/
                                  pcOutputString,   /* The output buffer. */
                                  MAX_OUTPUT_LENGTH /* The size of the output buffer. */
                              );

                /* Write the output generated by the command interpreter to the
                console. */
                printf("%s", pcOutputString);
                cli_prompt_counter = 0;

            } while( xMoreDataToFollow != pdFALSE );
            gettimeofday(&end, NULL);

            uint64_t delta = ((end.tv_sec - start.tv_sec)*1000000)+(end.tv_usec - start.tv_usec);

            printf("\x03\x03\x04\x04   Finished in %" PRIu64  " us", delta);
            printf(CLI_PROMPT);
            /* All the strings generated by the input command have been sent.
            Processing of the command is complete.  Clear the input string ready
            to receive the next command. */
            cInputIndex = 0;
            memset( pcInputString, 0x00, MAX_INPUT_LENGTH );
        }
        else
        {
            /* The if() clause performs the processing after a newline character
            is received.  This else clause performs the processing if any other
            character is received. */

            if( cRxedChar == '\r' )
            {
                /* Ignore carriage returns. */
            }
            else if( cRxedChar == '\b' )
            {
                /* Backspace was pressed.  Erase the last character in the input
                buffer - if there are any. */
                if( cInputIndex > 0 )
                {
                    cInputIndex--;
                    pcInputString[ cInputIndex ] = '\0';
                }
            }
            else
            {
                /* A character was entered.  It was not a new line, backspace
                or carriage return, so it is accepted as part of the input and
                placed into the input buffer.  When a \n is entered the complete
                string will be passed to the command interpreter. */
                if( cInputIndex < MAX_INPUT_LENGTH && cRxedChar != 0xFF )
                {
                    pcInputString[ cInputIndex ] = cRxedChar;
                    cInputIndex++;
                    printf("%c", cRxedChar);
                }
            }
        }
        cli_prompt_counter += 10;
        if(cli_prompt_counter >= cli_prompt_timeout)
        {
            printf(CLI_PROMPT);
            cli_prompt_counter = 0;
        }
        vTaskDelay(2);
    }
}

extern "C" int app_main(void)
{
    //// Initialize memory
    nvs_flash_init();
    xSemaphore = xSemaphoreCreateMutex();
    initialise_wifi();
    gpio_pad_select_gpio(LED_BUILTIN);
    gpio_set_direction((gpio_num_t)LED_BUILTIN, GPIO_MODE_OUTPUT);

    xTaskCreate(&handle_sse, "handle_sse", 4096, NULL, 1, NULL);
    xTaskCreate(&http_server, "http_server", 4096, NULL, 1, NULL);
    // xTaskCreate(&usb_reader, "usb_reader", 4096, NULL, 3, NULL);
    xTaskCreate(&vCommandConsoleTask, "vCommandConsoleTask", 4096, NULL, 3, NULL);

    return 0;
}