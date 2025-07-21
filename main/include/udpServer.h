
void udpServerTask (void *pvParameters);


typedef struct {
    int port;
    size_t maxLen;
   
} udpTaskParams_t;

typedef struct {
    int port;
    char *mssg;
    size_t len;
} udpMssg_t;


extern QueueHandle_t udpMssgBox;