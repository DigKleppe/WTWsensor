/*
 * udpServer.cpp
 *
 *  Created on: May 2, 2022
 *      Author: dig
 */

// Server side implementation of UDP client-server model
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "udpServer.h"

#define TAG "UDPserver"

QueueHandle_t udpMssgBox;

void udpServerTask(void *pvParameters) {
	udpTaskParams_t udpTaskParams = *(udpTaskParams_t *)pvParameters;
	int sockfd;
	bool err = false;
	struct sockaddr_in servaddr, cliaddr;
	udpMssg_t udpMssg;
	char *buffer;

	udpMssgBox = xQueueCreate(10, sizeof(udpMssg_t));

	while (1) {
		if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			ESP_LOGE(TAG, "socket creation failed");
			err = true;
		} else {
			memset(&servaddr, 0, sizeof(servaddr));
			memset(&cliaddr, 0, sizeof(cliaddr));

			// Filling server information
			servaddr.sin_family = AF_INET; // IPv4
			servaddr.sin_addr.s_addr = INADDR_ANY;
			servaddr.sin_port = htons(udpTaskParams.port);

			// Bind the socket with the server address
			if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
				ESP_LOGE(TAG, "bind failed");
				err = true;
				closesocket(sockfd);
			} else {
				int len, n;
		//		ESP_LOGI(TAG, "server running");
				len = sizeof(cliaddr); // len is value/result

				buffer = (char *)malloc((size_t)udpTaskParams.maxLen + 1);
				if (!buffer) {
					ESP_LOGE(TAG, "buffer creation failed");
					n = 0;
					// todo
				} else {
					n = recvfrom(sockfd, buffer, udpTaskParams.maxLen, MSG_WAITALL, (struct sockaddr *)&cliaddr, (socklen_t *)&len);
					buffer[n] = '\0';
			//		ESP_LOGI(TAG, "Received: %s", buffer);
				}
				udpMssg.len = n;
				udpMssg.mssg = buffer;
				xQueueSendToBack(udpMssgBox, &udpMssg, 0);
				closesocket(sockfd);
			}
		}
		if (err) {
			vTaskDelay(100);
			err = false;
		}
	}
}
