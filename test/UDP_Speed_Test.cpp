/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


/*
    Copyright (c) Dec 2023  , Daniel Perron
    Add dummy UDP push to see the maximum bandwidth of the Pico
*/




#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include <time.h>

//  UDP SEND TO HOST IP/PORT

#define  SEND_TO_IP  "192.168.1.101"
#define  SEND_TO_PORT 12345
#define  WIFI_SSID "Frohne-2.4GHz"
#define  WIFI_PASSWORD NULL


#define  SAMPLE_CHUNK_SIZE 512


struct udp_pcb  * send_udp_pcb;




typedef struct{
   uint32_t blockId;
   uint16_t blockSize;
   uint16_t numberOfSample;
   uint16_t  AD_Value[SAMPLE_CHUNK_SIZE];
} SampleBlockStruct;

#define BLOCK_MAX 5

SampleBlockStruct block[BLOCK_MAX];
int block_head=0;
int block_tail=0;



void SendUDP(char * IP , int port, void * data, int data_size)
{
      ip_addr_t   destAddr;
      ip4addr_aton(IP,&destAddr);
      struct pbuf * p = pbuf_alloc(PBUF_TRANSPORT,data_size+1,PBUF_RAM);
      char *pt = (char *) p->payload;
      memcpy(pt,data,data_size);
      pt[data_size]='\0';
      cyw43_arch_lwip_begin();
      udp_sendto(send_udp_pcb,p,&destAddr,port);
      cyw43_arch_lwip_end();
      pbuf_free(p);
}



int main() {
    int loop;
    int blockId=0;

    stdio_init_all();
    // initialize wifi

    if (cyw43_arch_init()) {
        printf("Init failed!\n");
        return 1;
    }
    cyw43_pm_value(CYW43_NO_POWERSAVE_MODE,200,1,1,10);
    cyw43_arch_enable_sta_mode();

    printf("WiFi ... ");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed!\n");
        return 1;
    } else {
        printf("Connected.n");
        printf("IP: %s\n",ipaddr_ntoa(((const ip_addr_t *)&cyw43_state.netif[0].ip_addr)));
    }

   printf("Size of block is %d\n",sizeof(SampleBlockStruct));
  // getchar();
   printf("start\n");

   // set default block size
   for(loop=0;loop<BLOCK_MAX;loop++)
       block[loop].blockSize = sizeof(SampleBlockStruct);


    // create UDP
    send_udp_pcb = udp_new();




    while (1) {
        block[block_head].blockId=blockId++;
        block_head = (++block_head) % BLOCK_MAX;
        if(block_head != block_tail)
           {
            SendUDP(SEND_TO_IP,SEND_TO_PORT,&block[block_tail],sizeof(SampleBlockStruct));
            block_tail = (++block_tail) % BLOCK_MAX;
//            printf("%u\n",block_tail);
           }
 //       sleep_us(100);
    }
    return 0;
}