#pragma once
/* Protocolo de programacion por PC (USART6, 115200 8N1), compatible con el
 * software oficial: ver docs/storage_comms.md seccion 2.                    */
void cps_init(void);
int  cps_poll(void);          /* procesa bytes; devuelve 1 mientras haya sesion PC activa */
int  cps_session_ended(void); /* 1 una vez tras terminar la sesion (recargar datos) */
