#ifndef MICROROS_TRANSPORTS_H
#define MICROROS_TRANSPORTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declaration for uxrCustomTransport */
struct uxrCustomTransport;

/* UDP Transport functions - for Ethernet */
bool cubemx_transport_open(struct uxrCustomTransport * transport);
bool cubemx_transport_close(struct uxrCustomTransport * transport);
size_t cubemx_transport_write(struct uxrCustomTransport* transport, const uint8_t * buf, size_t len, uint8_t * err);
size_t cubemx_transport_read(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);

/* DMA Transport functions - for UART with DMA */
bool cubemx_transport_open_dma(struct uxrCustomTransport * transport);
bool cubemx_transport_close_dma(struct uxrCustomTransport * transport);
size_t cubemx_transport_write_dma(struct uxrCustomTransport* transport, uint8_t * buf, size_t len, uint8_t * err);
size_t cubemx_transport_read_dma(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);

/* IT Transport functions - for UART with Interrupts */
bool cubemx_transport_open_it(struct uxrCustomTransport * transport);
bool cubemx_transport_close_it(struct uxrCustomTransport * transport);
size_t cubemx_transport_write_it(struct uxrCustomTransport* transport, uint8_t * buf, size_t len, uint8_t * err);
size_t cubemx_transport_read_it(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);

/* USB CDC Transport functions */
bool cubemx_transport_open_usb(struct uxrCustomTransport * transport);
bool cubemx_transport_close_usb(struct uxrCustomTransport * transport);
size_t cubemx_transport_write_usb(struct uxrCustomTransport* transport, uint8_t * buf, size_t len, uint8_t * err);
size_t cubemx_transport_read_usb(struct uxrCustomTransport* transport, uint8_t* buf, size_t len, int timeout, uint8_t* err);

/* Memory allocator functions - defined in microros_allocators.c */
void * microros_allocate(size_t size, void * state);
void microros_deallocate(void * pointer, void * state);
void * microros_reallocate(void * pointer, size_t size, void * state);
void * microros_zero_allocate(size_t number_of_elements, size_t size_of_element, void * state);

/* Memory diagnostics */
int microros_get_absolute_used_memory(void);
int microros_get_used_memory(void);

#endif /* MICROROS_TRANSPORTS_H */
