#include "esp_netif_ip_addr.h"

typedef enum  { PING_SUCCESS, /*!< Ping procedure succeeded */
                       PING_FAILED, /*!< Ping procedure timed out */
                       PING_ERROR, /*!< Ping procedure encountered an error */
                       PING_END /*!< Ping session ended */ } pingResult_t;


pingResult_t ping(  esp_ip4_addr_t target_addr);
