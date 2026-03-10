/**
 * T4b: Debug Patch for lwip.c
 * 
 * ADD THESE LINES to the beginning of MX_LWIP_Init() in:
 *   CM7/LWIP/App/lwip.c
 * 
 * This provides detailed debug output inside LwIP initialization
 */

/* ========================================================= */
/* ADD THIS AT THE TOP of MX_LWIP_Init() function:          */
/* ========================================================= */

  /* Enable GPIOB for debug blinks */
  RCC->AHB4ENR |= (1 << 1);
  for(volatile int dbg_i = 0; dbg_i < 1000; dbg_i++);
  GPIOB->MODER &= ~(3 << 0);
  GPIOB->MODER |= (1 << 0);

  /* 1 blink = entering MX_LWIP_Init */
  GPIOB->BSRR = (1 << 0);
  for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
  GPIOB->BSRR = (1 << 16);
  for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);

/* ========================================================= */
/* ADD THIS AFTER tcpip_init(NULL, NULL):                    */
/* ========================================================= */

  /* 2 blinks = tcpip_init done */
  for(int dbg_j = 0; dbg_j < 2; dbg_j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
  }

/* ========================================================= */
/* ADD THIS AFTER netif_add():                               */
/* ========================================================= */

  /* 3 blinks = netif_add done */
  for(int dbg_j = 0; dbg_j < 3; dbg_j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
  }

/* ========================================================= */
/* ADD THIS AFTER netif_set_up():                            */
/* ========================================================= */

  /* 4 blinks = netif_set_up done */
  for(int dbg_j = 0; dbg_j < 4; dbg_j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
  }

/* ========================================================= */
/* ADD THIS AFTER osThreadNew(ethernet_link_thread...):      */
/* ========================================================= */

  /* 5 blinks = ethernet_link_thread created */
  for(int dbg_j = 0; dbg_j < 5; dbg_j++) {
    GPIOB->BSRR = (1 << 0);
    for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
    GPIOB->BSRR = (1 << 16);
    for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
  }

/* ========================================================= */
/* END OF PATCH                                              */
/* ========================================================= */


/**
 * COMPLETE EXAMPLE OF MODIFIED MX_LWIP_Init():
 * 
 * void MX_LWIP_Init(void)
 * {
 *   // --- DEBUG PATCH START ---
 *   RCC->AHB4ENR |= (1 << 1);
 *   for(volatile int dbg_i = 0; dbg_i < 1000; dbg_i++);
 *   GPIOB->MODER &= ~(3 << 0);
 *   GPIOB->MODER |= (1 << 0);
 *   
 *   GPIOB->BSRR = (1 << 0); for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
 *   GPIOB->BSRR = (1 << 16); for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
 *   // --- DEBUG PATCH END ---
 *   
 *   IP_ADDRESS[0] = 192; ...
 *   
 *   tcpip_init(NULL, NULL);
 *   
 *   // --- DEBUG PATCH ---
 *   for(int dbg_j = 0; dbg_j < 2; dbg_j++) {
 *     GPIOB->BSRR = (1 << 0); for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
 *     GPIOB->BSRR = (1 << 16); for(volatile int dbg_i = 0; dbg_i < 400000; dbg_i++);
 *   }
 *   
 *   IP4_ADDR(&ipaddr, ...);
 *   
 *   netif_add(&gnetif, ...);
 *   
 *   // --- DEBUG PATCH ---
 *   for(int dbg_j = 0; dbg_j < 3; dbg_j++) { ... }
 *   
 *   netif_set_default(&gnetif);
 *   netif_set_up(&gnetif);
 *   
 *   // --- DEBUG PATCH ---
 *   for(int dbg_j = 0; dbg_j < 4; dbg_j++) { ... }
 *   
 *   netif_set_link_callback(...);
 *   
 *   osThreadNew(ethernet_link_thread, ...);
 *   
 *   // --- DEBUG PATCH ---
 *   for(int dbg_j = 0; dbg_j < 5; dbg_j++) { ... }
 * }
 */
