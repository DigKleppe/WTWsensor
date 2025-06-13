/* ICMP echo example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_console.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "argtable3/argtable3.h"
#include "ping/ping_sock.h"
#include "esp_check.h"
#include "ping.h"

const static char *TAG = "ping";
static volatile bool pingRdy;
static pingResult_t pingResult;

static void cmd_ping_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    printf("%" PRIu32 " bytes from %s icmp_seq=%" PRIu16 " ttl=%" PRIu16 " time=%" PRIu32 " ms\n",
           recv_len, ipaddr_ntoa((ip_addr_t*)&target_addr), seqno, ttl, elapsed_time);
    pingResult = PING_SUCCESS;
    pingRdy = true;
    

}

static void cmd_ping_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    printf("From %s icmp_seq=%d timeout\n",ipaddr_ntoa((ip_addr_t*)&target_addr), seqno);
    pingResult = PING_FAILED;
 //   pingRdy = true;   
}

static void cmd_ping_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    ip_addr_t target_addr;
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    uint32_t loss;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));

    if (transmitted > 0) {
        loss = (uint32_t)((1 - ((float)received) / transmitted) * 100);
    } else {
        loss = 0;
    }
#ifdef CONFIG_LWIP_IPV4
    if (IP_IS_V4(&target_addr)) {
        printf("\n--- %s ping statistics ---\n", inet_ntoa(*ip_2_ip4(&target_addr)));
    }
#endif
#ifdef CONFIG_LWIP_IPV6
    if (IP_IS_V6(&target_addr)) {
        printf("\n--- %s ping statistics ---\n", inet6_ntoa(*ip_2_ip6(&target_addr)));
    }
#endif
    printf("%" PRIu32 " packets transmitted, %" PRIu32 " received, %" PRIu32 "%% packet loss, time %" PRIu32 "ms\n",
           transmitted, received, loss, total_time_ms);
    // delete the ping sessions, so that we clean up all resources and can create a new ping session
    // we don't have to call delete function in the callback, instead we can call delete function from other tasks
    esp_ping_delete_session(hdl);
    pingResult = PING_END;
  //  pingRdy = true;   
}

pingResult_t ping(  esp_ip4_addr_t target_addr)
{
    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    esp_err_t err;

    config.target_addr = IPADDR4_INIT(target_addr.addr);

    /* set callback functions */
    esp_ping_callbacks_t cbs = {
        .cb_args = NULL,
        .on_ping_success = cmd_ping_on_ping_success,
        .on_ping_timeout = cmd_ping_on_ping_timeout,
        .on_ping_end = cmd_ping_on_ping_end
    };
    esp_ping_handle_t pingH;
    err = esp_ping_new_session(&config, &cbs, &pingH);
    if( err != ESP_OK )
        ESP_LOGE( TAG, "esp_ping_new_session failed");
     else {
        err = esp_ping_start(pingH);
     }
     if (err) {
         ESP_LOGE( TAG, "esp_ping_start failed");
         return PING_ERROR;
     }
  
    do {
        vTaskDelay( 100 / portTICK_PERIOD_MS);
    } while (pingResult != PING_END);  

 //   esp_ping_delete_session(pingH);
    pingRdy = true;

    printf("Ping finished\n");
    return pingResult;
}



// static esp_console_repl_t *s_repl = NULL;

// /* handle 'quit' command */
// static int do_cmd_quit(int argc, char **argv)
// {
//     printf("ByeBye\r\n");
//     s_repl->del(s_repl);
//     return 0;
// }

// static esp_err_t register_quit(void)
// {
//     esp_console_cmd_t command = {
//         .command = "quit",
//         .help = "Quit REPL environment",
//         .func = &do_cmd_quit
//     };
//     return esp_console_cmd_register(&command);
// }

// void app_main(void)
// {
//     ESP_ERROR_CHECK(nvs_flash_init());
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());

//     esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
//     // install console REPL environment
// #if CONFIG_ESP_CONSOLE_UART
//     esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &s_repl));
// #elif CONFIG_ESP_CONSOLE_USB_CDC
//     esp_console_dev_usb_cdc_config_t cdc_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&cdc_config, &repl_config, &s_repl));
// #elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
//     esp_console_dev_usb_serial_jtag_config_t usbjtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usbjtag_config, &repl_config, &repl));
// #endif

//     /* Use either WiFi console commands or menuconfig options to connect to WiFi/Ethernet
//      *
//      * Please disable `Provide wifi connect commands` in `Example Connection Configuration`
//      * to connect immediately using configured interface and settings (WiFi/Ethernet).
//      */
// #if defined(CONFIG_EXAMPLE_PROVIDE_WIFI_CONSOLE_CMD)
//     /* register wifi connect commands */
//     example_register_wifi_connect_commands();
// #elif defined(CONFIG_EXAMPLE_CONNECT_WIFI) || defined(CONFIG_EXAMPLE_CONNECT_ETHERNET)
//     /* automatic connection per menuconfig */
//     ESP_ERROR_CHECK(example_connect());
// #endif
//     struct ifreq ifr;
//     ESP_ERROR_CHECK(esp_netif_get_netif_impl_name(EXAMPLE_INTERFACE, ifr.ifr_name));
//     printf("Connected on interface: %s (%d)", ifr.ifr_name, esp_netif_get_netif_impl_index(EXAMPLE_INTERFACE));

//     /* register command `ping` */
//     register_ping();
//     /* register command `quit` */
//     register_quit();

//     // start console REPL
//     ESP_ERROR_CHECK(esp_console_start_repl(s_repl));
// }