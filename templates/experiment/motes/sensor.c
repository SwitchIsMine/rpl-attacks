/*
  *
  * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  * SUCH DAMAGE.
  *
*/
#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"

#include <string.h>
#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF(" %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((u8_t *)addr)[0], ((u8_t *)addr)[1], ((u8_t *)addr)[2], ((u8_t *)addr)[3], ((u8_t *)addr)[4], ((u8_t *)addr)[5], ((u8_t *)addr)[6], ((u8_t *)addr)[7], ((u8_t *)addr)[8], ((u8_t *)addr)[9], ((u8_t *)addr)[10], ((u8_t *)addr)[11], ((u8_t *)addr)[12], ((u8_t *)addr)[13], ((u8_t *)addr)[14], ((u8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

#define SEND_INTERVAL        15 * CLOCK_SECOND
#define MAX_PAYLOAD_LEN        40

 static struct uip_udp_conn *client_conn;
 static uip_ipaddr_t srvipaddr;

/*---------------------------------------------------------------------------*/
 PROCESS(udp_client_process, "UDP client process");
 AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
 static void
 tcpip_handler(void)
 {
   char *str;

    if(uip_newdata()) {
     str = uip_appdata;
     str[uip_datalen()] = '\0';
     printf("Response from the server: '%s'\n", str);
   }
 }
/*---------------------------------------------------------------------------*/
 static void
 timeout_handler(void)
 {
   static int seq_id;
   char buf[MAX_PAYLOAD_LEN];
 
   printf("Client sending to: ");
   PRINT6ADDR(&srvipaddr);
   sprintf(buf, "Hello %d from the client %s", ++seq_id, &client_conn->ripaddr);
   printf(" (msg: %s)\n", buf);
   uip_udp_packet_sendto(client_conn, buf, strlen(buf),
                        &srvipaddr, UIP_HTONS(3000));
 }
 /*---------------------------------------------------------------------------*/
static void
 print_local_addresses(void)
 {
   int i;
   uint8_t state;

   PRINTF("Client IPv6 addresses: ");
   for(i = 0; i < UIP_CONF_NETIF_MAX_ADDRESSES; i++) {
     state = uip_ds6_if.addr_list[i].state;
     if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
       PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
       PRINTF("\n");
     }
   }
 }
/*---------------------------------------------------------------------------*/
 static void
 set_connection_address(uip_ipaddr_t *ipaddr)
 {
   // specify the root ip address!
   uip_ip6addr(ipaddr,0xaaaa,0,0,0,0xc30c,0,0,0);
 }
/*---------------------------------------------------------------------------*/
 PROCESS_THREAD(udp_client_process, ev, data)
 {
   static struct etimer periodic;
   uip_ipaddr_t ipaddr;

   PROCESS_BEGIN();
   PRINTF("UDP client process started\n");

   // wait 5 second, in order to have the IP addresses well configured
   etimer_set(&periodic, CLOCK_CONF_SECOND*5);

   // wait until the timer has expired
   PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

   print_local_addresses();
   set_connection_address(&srvipaddr);
   /* new connection with remote host */
   client_conn = udp_new(NULL, UIP_HTONS(3000), NULL);
   if(client_conn == NULL) {
      PRINTF("No UDP connection available, exiting the process!\n");
      PROCESS_EXIT();
   }

   udp_bind(client_conn, UIP_HTONS(3001)); 
   PRINTF("Created a connection with the server ");
   PRINT6ADDR(&client_conn->ripaddr);
   PRINTF("local/remote port %u/%u\n",
   UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));

   etimer_set(&periodic, SEND_INTERVAL);
   while(1) {
     PROCESS_YIELD();
     if(etimer_expired(&periodic)) {
       timeout_handler();
       etimer_restart(&periodic);
     } else if(ev == tcpip_event) {
       tcpip_handler();
     }
   }

   PROCESS_END();
 }
 /*---------------------------------------------------------------------------*/
