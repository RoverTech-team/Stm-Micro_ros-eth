
#include <unistd.h>
#include "cmsis_os.h"

/* Memory tracking variables with mutex protection */
static int absoluteUsedMemory = 0;
static int usedMemory = 0;
static osMutexId_t mem_mutex = NULL;

/* Forward declarations */
void *pvPortMallocMicroROS( size_t xWantedSize );
void vPortFreeMicroROS( void *pv );
void *pvPortReallocMicroROS( void *pv, size_t xWantedSize );
size_t getBlockSize( void *pv );
void *pvPortCallocMicroROS( size_t num, size_t xWantedSize );

/* Initialize mutex on first use */
static void ensure_mutex_init(void) {
    if (mem_mutex == NULL) {
        const osMutexAttr_t mem_mutex_attr = {
            .name = "mem_mutex",
            .attr_bits = osMutexRecursive | osMutexPrioInherit,
        };
        mem_mutex = osMutexNew(&mem_mutex_attr);
    }
}

void * microros_allocate(size_t size, void * state){
  (void) state;
  
  ensure_mutex_init();
  
  void *ptr = pvPortMallocMicroROS(size);
  
  if (ptr != NULL && mem_mutex != NULL) {
    osMutexAcquire(mem_mutex, osWaitForever);
    absoluteUsedMemory += size;
    usedMemory += size;
    osMutexRelease(mem_mutex);
  }
  
  return ptr;
}

void microros_deallocate(void * pointer, void * state){
  (void) state;
  
  if (pointer != NULL) {
    size_t block_size = getBlockSize(pointer);
    
    if (mem_mutex != NULL) {
      osMutexAcquire(mem_mutex, osWaitForever);
      usedMemory -= block_size;
      osMutexRelease(mem_mutex);
    }
    
    vPortFreeMicroROS(pointer);
  }
}

void * microros_reallocate(void * pointer, size_t size, void * state){
  (void) state;
  
  ensure_mutex_init();
  
  size_t old_size = 0;
  if (pointer != NULL) {
    old_size = getBlockSize(pointer);
  }
  
  void *ptr;
  if (pointer == NULL) {
    ptr = pvPortMallocMicroROS(size);
  } else {
    ptr = pvPortReallocMicroROS(pointer, size);
  }
  
  if (ptr != NULL && mem_mutex != NULL) {
    osMutexAcquire(mem_mutex, osWaitForever);
    absoluteUsedMemory += size;
    usedMemory += size;
    usedMemory -= old_size;
    osMutexRelease(mem_mutex);
  }
  
  return ptr;
}

void * microros_zero_allocate(size_t number_of_elements, size_t size_of_element, void * state){
  (void) state;
  
  ensure_mutex_init();
  
  size_t total_size = number_of_elements * size_of_element;
  void *ptr = pvPortCallocMicroROS(number_of_elements, size_of_element);
  
  if (ptr != NULL && mem_mutex != NULL) {
    osMutexAcquire(mem_mutex, osWaitForever);
    absoluteUsedMemory += total_size;
    usedMemory += total_size;
    osMutexRelease(mem_mutex);
  }
  
  return ptr;
}

/* Utility functions for memory diagnostics */
int microros_get_absolute_used_memory(void) {
    return absoluteUsedMemory;
}

int microros_get_used_memory(void) {
    return usedMemory;
}
