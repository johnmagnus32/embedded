/*
 * UDP echo server on STM32F411RE + ENC28J60
 *
 * Listens on port 7 (echo), sends back whatever it receives.
 * Also sends a "hello" broadcast every 5 seconds.
 *
 * Test from a PC:
 *   echo "hello" | nc -u 192.168.1.100 7
 */

#include "config.h"
#include "devicetree.h"
#include "device.h"
#include "drivers/uart.h"
#include "net.h"
#include "sched.h"
#include "heap.h"

extern void systick_init(uint32_t cpu_hz, uint32_t tick_hz);
extern uint8_t _heap_start[];
extern uint32_t _heap_size;

DEVICE_DT_DECLARE(DT_CHOSEN_CONSOLE);
DEVICE_DT_DECLARE(spi1);
DEVICE_DT_DECLARE(eth0);

/* --- Network polling task --- */

static void net_rx_task(void)
{
    while (1) {
        net_poll();  /* check for incoming packets */
        sched_yield();
    }
}

/* --- UDP echo server task --- */

static void echo_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    int sock = udp_socket_open(7);  /* port 7 = echo */

    uart_puts(console, "[net] echo server listening on port 7\n");

    while (1) {
        char buf[128] = {0};
        ip_addr_t src_ip;
        uint16_t src_port;

        int len = udp_recv(sock, buf, sizeof(buf) - 1, &src_ip, &src_port);
        if (len > 0) {
            buf[len] = '\0';
            uart_puts(console, "[net] received: ");
            uart_puts(console, buf);
            uart_puts(console, "\n");

            /* Echo it back */
            udp_send(sock, src_ip, src_port, buf, len);
            uart_puts(console, "[net] echoed back\n");
        }

        sched_sleep_ms(10);
    }
}

/* --- Periodic sender task --- */

static void sender_task(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    int sock = udp_socket_open(5000);
    int count = 0;

    while (1) {
        sched_sleep_ms(5000);

        char msg[32] = "hello #";
        char *p = msg + 7;
        int v = count;
        if (v == 0) *p++ = '0';
        else {
            char tmp[8]; int n = 0;
            while (v > 0) { tmp[n++] = '0' + v % 10; v /= 10; }
            while (n > 0) *p++ = tmp[--n];
        }
        *p = '\0';

        /* Send to broadcast */
        udp_send(sock, IP_ADDR(192, 168, 1, 255), 5000, msg, (size_t)(p - msg));

        uart_puts(console, "[net] sent: ");
        uart_puts(console, msg);
        uart_puts(console, "\n");

        count++;
    }
}

static void idle_task(void)
{
    while (1) { __asm volatile("wfi"); }
}

int main(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN_CONSOLE);
    const struct device *spi = DEVICE_DT_GET(spi1);
    const struct device *eth = DEVICE_DT_GET(eth0);

    console->init(console);
    spi->init(spi);
    eth->init(eth);

    uart_puts(console, "\n=== UDP/IP Network Demo ===\n");
    uart_puts(console, "IP: 192.168.1.100\n");
    uart_puts(console, "Echo server on port 7\n\n");

    heap_init(_heap_start, (size_t)&_heap_size);

    /* Initialize network stack */
    net_init(eth,
             IP_ADDR(192, 168, 1, 100),   /* our IP */
             IP_ADDR(255, 255, 255, 0),    /* netmask */
             IP_ADDR(192, 168, 1, 1));     /* gateway */

    sched_create_task(net_rx_task, "net_rx", 1);
    sched_create_task(echo_task, "echo", 3);
    sched_create_task(sender_task, "sender", 4);
    sched_create_task(idle_task, "idle", 7);

    systick_init(DT_SYSCLK_HZ, 1000);
    sched_start();
}
