/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Laura Giraldo Duque
 * @brief          : Main program body
 ******************************************************************************
 */

#include "stm32f411xe.h"
#include <stdint.h>
#include <stm32f4xx.h>


 //Tarea 01 TALLER V
//DESCRIPCION DEL SISTEMA 
 /*En esta tarea implementamos un sistema basado en STM32F411xE, el cual se encarga de generar las señales de multiplexado para la etapa de potencia del display 
 y gestionar las interrupciones provenientes de los timer y las fotocompuertas, en el hardware se utiliza un siete segmentos 5641-BSR de ànodo comun para protegeros LEDs internos,
 las líneas de los segmentos, PA4, PC1, PB1, PB14, PB15, PB0, PB2, tienen resistencias limitadoras, para suministrar la corriente que necesitan estos leds para
 funcionar se conecta 4 transistores PNP para la etapa de potencia y multiplexado, conectados de la siguiente forma; emisores van conectados a VCC, los colectores
 al ánodo común de cada dígito, y las bases se conectan a los pines PB7, PA8, PC3 y PB13 a través de resistencias de base, al ser transistores PNP, se activan 
 cerrando el circuito cuando el microcontrolador envía un 0, ademas, tenemos dos fotocompuertas las cuales estan configuradas para mandar interrupciones al programa etsan uno conectado al pin PC9 
 para sumar y otro en el PB12 para restar, por ultimo de un LED  (Blinky) en el pin PH1. En el software dentro del bucle del main tenemos una variable llamada multiplex la cual el Timer 4 la activa 
 (la pone en 1) cada 2 ms. Al detectarlo, el main la baja a 0 y llama a la función Refresco_Display() que funciona con una máquina de estados, primero apaga los transistores enviando un 1, despues 
 dependiendo del estado actual de los digitos el sistema cuenta (unidades, decenas, centenas y miles, envia esta cifra a otra funcion llamada registro_digitos() que se encarga de encender los segmentos
 los cuales estan de la A hasta G y formar los digtos (todo esto con un switch case) despues enciende el transistor PNP del dígito correspondiente enviando un 0 lógico a su base.Finalmente, actualiza la 
 variable de estado para que, en los próximos 2 ms, el sistema ilumine el siguiente dígito en la secuencia.Por otro lado, el conteo y el indicador visual funcionan cuando las fotocompuertas detectan una interrupcion de luz
 generan una interrupción externa  el pin PC9 dispara una interrupción en flanco de subida para incrementar la variable 'contador', mientras que el PB12 lo hace en flanco de bajada para decrementarla, dentro de los handlers 
 de estas interrupciones es obligatorio bajar la bandera de interrupción pendiente (escribiendo un 1 en el registro EXTI->PR) para indicarle al procesador que el evento ya fue atendido y evitar que se quede en ese estado
  De igual manera, el Timer 11 genera una interrupción independiente cada 500 ms para hacer parpadear el LED en PH1, donde también se debe limpiar su respectiva bandera de actualización (bit UIF en el registro SR) tras cada ejecución

 */

//MAPEO DE PINES

/* 
5641-BSR DIGITOS:

-DIGITO 1 CORRESPONDE AL PIN 12 EL CUAL ENTRA AL GPIO PB7

-DIGITO 2 CORRESPONDE AL PIN 9 EL CUAL ENTRA AL GPIO PA8

-DIGITO 3 CORRESPONDE AL PIN 8 EL CUAL ENTRA AL GPIO PC3

-DIGITO 4 CORRESPONDE AL PIN 6 EL CUAL ENTRA AL GPIO PB13


5641-BSR LED:

    PIN 1 EL CUAL ENTRA AL GPIO PB15 (enciende segmento E)
    PIN 2 EL CUAL ENTRA AL GPIO PB14 (enciende segmento D)
    PIN 3 NO SE USA (Son los puntos)
    PIN 4 EL CUAL ENTRA AL GPIO PB1 (ennciende segmento C)
    PIN 5 EL CUAL ENTRA AL GPIO PB2 (enciende segmento G)
    PIN 7 EL CUAL ENTRA AL GPIO PC1 (enciende segmento B)
    PIN 10 EL CUAL ENTRA AL GPIO PB0 (enciende segmento F)
    PIN 11 EL CUAL ENTRA AL GPIO PA4 (enciende segmento A)


FOTOCOMPUERTA DE FLANCO DE SUBIDA;

    PC9 


FOTOCOMPUERTA DE FLANCO DE BAJADA:

  B12


Blinky PH1 timer 11


*/ 
// DEFINICIÒN DE VARIABLES
volatile uint16_t contador = 0; // 16 bits para que desborde automáticamente a 65535
volatile uint16_t multiplex = 0;
 
// MAQUINA DE ESTADOS 
typedef enum {
    DIGITO_1, // miles
    DIGITO_2, // centenas
    DIGITO_3, // decenas
    DIGITO_4  // unidades
} EstadoDigitos;

EstadoDigitos DigActual= DIGITO_1;

// CABECERA DE FUNCIONES
void init_GPIO(void);
void init_timmer(void);
void init_timmermultiplex4(void);
void init_EXTI(void);
void Registro_Digitos(uint8_t numero);
void Refresco_Display(void);

// Main
int main(void){
    init_GPIO();              // Configura los registros del GPIO para que los puertos se comporten como entradas y como salidas
    init_timmer();            // Configura el TIM11 y arranca el conteo para encender el LED 2 de la board tàctica  para el blinky
    init_timmermultiplex4();  // Configura el TIM4 para el multiplexado del display
    init_EXTI();              // Configura las interrupciones externas (Fotocompuertas), una en flanco de subida y otra en flanco de bajada

    while(1){

        if (multiplex){
            multiplex = 0;
            Refresco_Display();

        }
       
    }
}

// Definicion de funciones

void init_GPIO(void){
    RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOAEN; // Se limpia el registro del puerto A
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; // Se activa la señal de reloj del puerto A

    RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOBEN; // Se limpia el registro del puerto B
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; // Se activa la señal de reloj del puerto B

    RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOCEN; // Se limpia la señal de registro del puerto C
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; // Se activa la señal de reloj del puerto C

    RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOHEN; // Se limpia la señal de registro del puerto H
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; // Se activa la señal de reloj del puerto H

    // // PUERTO A; configuramos las salidas del puerto A para encender los pines del siete segmento (PA8 y PA4)
    GPIOA->MODER   &= ~(GPIO_MODER_MODE4 | GPIO_MODER_MODE8); //Se limpian los bits de configuración de los pines PA4 y PA8
    GPIOA->MODER   |=  (GPIO_MODER_MODE4_0 | GPIO_MODER_MODE8_0); //Se configura como Salida Digital (estado '01' en el registro MODER)
    GPIOA->OTYPER  &= ~(GPIO_OTYPER_OT4 | GPIO_OTYPER_OT8); // la salida como "Push-Pull" (limpiando el bit a '0')
    GPIOA->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED4 | GPIO_OSPEEDR_OSPEED8); // Se configura con la velocidad màs baja 
    GPIOA->PUPDR   &= ~(GPIO_PUPDR_PUPD4 | GPIO_PUPDR_PUPD8); // Se desactivan las resistencias internas ya que tenemos en el hardware

    // // PUERTO B; configuramos pines para segmentos y dígitos (PB0, PB1, PB2, PB7, PB13, PB14, PB15 salidas) y fotocompuerta (PB12 entrada)
    GPIOB->MODER   &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1 | GPIO_MODER_MODE2 | GPIO_MODER_MODE7 | // Se limpian los bits de configuraciòn de los pines PB0, PB1, PB2, PB7, PB13, PB14, PB15. Y dejamos el GPIO PB12 como entrada "00"
    GPIO_MODER_MODE12 | GPIO_MODER_MODE13 | GPIO_MODER_MODE14 | GPIO_MODER_MODE15); 
    GPIOB->MODER   |=  (GPIO_MODER_MODE0_0 | GPIO_MODER_MODE1_0 | GPIO_MODER_MODE2_0 | GPIO_MODER_MODE7_0 | // Se configuran los pines PB0, PB1, PB2, PB7, PB13, PB14, PB15 (estado "01" en el registro MODER)
    GPIO_MODER_MODE13_0 | GPIO_MODER_MODE14_0 | GPIO_MODER_MODE15_0);
    GPIOB->OTYPER  &= ~(GPIO_OTYPER_OT0 | GPIO_OTYPER_OT1 | GPIO_OTYPER_OT2 | GPIO_OTYPER_OT7 | // la salida como "Push-Pull" (limpiando el bit a '0')
    GPIO_OTYPER_OT13 | GPIO_OTYPER_OT14 | GPIO_OTYPER_OT15);
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED0 | GPIO_OSPEEDR_OSPEED1 | GPIO_OSPEEDR_OSPEED2 | GPIO_OSPEEDR_OSPEED7 |  // Se configura con la velocidad màs baja 
    GPIO_OSPEEDR_OSPEED13 | GPIO_OSPEEDR_OSPEED14 | GPIO_OSPEEDR_OSPEED15);
    GPIOB->PUPDR   &= ~(GPIO_PUPDR_PUPD0 | GPIO_PUPDR_PUPD1 | GPIO_PUPDR_PUPD2 | GPIO_PUPDR_PUPD7 |
    GPIO_PUPDR_PUPD12 | GPIO_PUPDR_PUPD13 | GPIO_PUPDR_PUPD14 | GPIO_PUPDR_PUPD15); //Se desactivan las resistencias internas ya que tenemos en el hardware

    // PUERTO C (PC1, PC3 salidas; PC9 entrada)
    GPIOC->MODER   &= ~(GPIO_MODER_MODE1 | GPIO_MODER_MODE3 | GPIO_MODER_MODE9); // Se limpian los bits de configuraciòn PC1 y PC3, ademas, PC9 como entrada "00"
    GPIOC->MODER   |=  (GPIO_MODER_MODE1_0 | GPIO_MODER_MODE3_0); // Se configura como salidas los GPIO PC1 y PC3, (estado "01" en el registro MODER)
    GPIOC->OTYPER  &= ~(GPIO_OTYPER_OT1 | GPIO_OTYPER_OT3); // la salida como "Push-Pull" (limpiando el bit a '0')
    GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED1 | GPIO_OSPEEDR_OSPEED3); // Se configura con la velocidad màs baja 
    GPIOC->PUPDR   &= ~(GPIO_PUPDR_PUPD1 | GPIO_PUPDR_PUPD3 | GPIO_PUPDR_PUPD9); //Se desactivan las resistencias internas ya que tenemos en el hardware


    // Configuraciòn del PUERTO H para el blinky
    GPIOH->MODER &= ~(GPIO_MODER_MODE1); //Se limpian los bits de configuracion del pin PH1
    GPIOH->MODER |= (GPIO_MODER_MODE1_0); // Se configura como salida (estado "01" en el registro MODER)
    GPIOH->OTYPER &= ~(GPIO_OTYPER_OT1); // la salida como "Push-Pull" (limpiando el bit a '0')
    GPIOH->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED1); // Se configura con la velocidad màs baja
    GPIOH->PUPDR &= ~(GPIO_PUPDR_PUPD1);  // Sin pull up ni pull down 
    GPIOH->ODR |= (GPIO_ODR_OD1); // Escribe un '1' en el registro de salida de datos para empezar encendido

    // Apagar todos los dígitos en el registro GPIO port output data register al iniciar para evitar picos  ya que en este caso PNP se apaga con 1
    GPIOB->ODR |= (GPIO_ODR_OD7 | GPIO_ODR_OD13);
    GPIOA->ODR |= GPIO_ODR_OD8;
    GPIOC->ODR |= GPIO_ODR_OD3;
}
/* 
  REGISTROS CLAVE DEL TIMER:
 1. CR1 (Control Register 1): Contiene bits como CEN (Counter enable) el cual enciende el timer y ARPE(Auto-reload preload enable) que se encarga de la carga automatica.
    Nota: El TIM11 solo cuenta hacia arriba, por lo que no tiene bit DIR.
 2. DIER (DMA/Interrupt Enable Register): Aquí se activa el UIE (Update interrupt enable) para permitir interrupciones al terminar la cuenta.
 3. SR (Status Register): Contiene el bit UIF (Update Interrupt Flag). Se debe poner en 0 para bajar la bandera.
 */

void init_timmer(void){
    RCC->APB2ENR &= ~(RCC_APB2ENR_TIM11EN);  //Limpiamos el bit 
    RCC->APB2ENR |= (RCC_APB2ENR_TIM11EN); //Habilitamos la señal de reloj en el APB2

    //Configuramos los registros del timmer 
    TIM11->PSC = 16000 - 1 ; //El prescaler define el rango de el tiempo del timer, ya que inicialmente va en 16MHz entonces PSC=16MHz/16000= 1KHz osea que el timer cuenta cada 1 ms
    TIM11->ARR = 500 - 1; // Auto-Reload Register cuenta hasta 500 osea que 500 cuentas x 1 ms = 500 ms.   
    TIM11->CNT = 0; // Reinicia el contador a 0 


    TIM11->CR1 |= TIM_CR1_ARPE; // ARPE = 1: Permite precargar el valor del ARR
    TIM11->SR &= ~TIM_SR_UIF;  // UIF = 0: Baja la bandera de actualización 
    TIM11->DIER |= TIM_DIER_UIE; //UIE = 1: Activamos la interrupción por actualización (cuando llega a 500)

    // Habilitamos la interrupción en el procesador (NVIC)
    NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn); 
    TIM11->CR1 |= TIM_CR1_CEN; //El contador inicia
}
// Nota: Usamos el TIM4 porque es un temporizador de propósito general diseñado para tareas más complejas y completas

void init_timmermultiplex4(){
    RCC->APB1ENR &= ~(RCC_APB1ENR_TIM4EN); // Limpiamos el bit de habilitación 
    RCC->APB1ENR |= (RCC_APB1ENR_TIM4EN); //Habilitamos la señal de reloj del TIM4 en el bus APB1

    TIM4->PSC = 160 - 1;   // Prescaler: Divide 16 MHz / 160 = 100 kHz (10 us)
    TIM4->ARR = 200 - 1;   // Auto-Reload Register: 750 pasos x 10 us = 7500 us (7.5 ms por dígito), para que no se perciba el parpadeo
    TIM4->CNT = 0; //Reinica el contador a 0

    TIM4->CR1 |= TIM_CR1_ARPE; // ARPE = 1: Permite precargar el valor del ARR de forma segura.
    TIM4->CR1 &= ~TIM_CR1_DIR; // DIR = 0: Configuramos el timer para que cuente hacia arriba. 
    TIM4->SR &= ~TIM_SR_UIF;  // UIF = 0: Baja la bandera de interrupción 

    TIM4->DIER |= TIM_DIER_UIE;   // UIE = 1: Activa la interrupción por actualización (cuando el timer llega a 750).
    //Habilita la interrupción del TIM4 en el procesador (NVIC).
    NVIC_EnableIRQ(TIM4_IRQn);     
    TIM4->CR1 |= TIM_CR1_CEN;      //El contador inicia
}
// Configuración de Interrupciones Externas (EXTI) para las Fotocompuertas
/* 
 REGISTROS CLAVE DEL EXTI Y SYSCFG:
 1. SYSCFG (System Configuration) Es el que permite el paso de la interrupción es el que decide qué letra de puerto tiene permiso para usar ese número de pin hacia el procesador.
 2. RTSR / FTSR (Rising/Falling Trigger Selection) definen en qué momento exacto salta la interrupción
 3. IMR (Interrupt Mask Register) quita la "máscara" protectora de la línea EXTI para dejar que la señal de interrupción llegue finalmente al procesador.
 */
void init_EXTI(void){
    
    RCC->APB2ENR &= ~RCC_APB2ENR_SYSCFGEN; // Se limpia el bit del registro
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;  // Habilita la señal del SYSCFG

    // Configurar PC9 para EXTI9
    SYSCFG->EXTICR[2] &= ~SYSCFG_EXTICR3_EXTI9;  // Limpia la configuraciòn del registro del syscfg para el EXTI en el pin 9, el cual se encuentra en el registro CR3 por eso [2]
    SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI9_PC; // Se activa  la interrupción del Puerto C (PC9)

    // Configurar PB12 para EXTI12
    SYSCFG->EXTICR[3] &= ~SYSCFG_EXTICR4_EXTI12;  // Limpia la configuración del registro del syscfg para el EXTI en el pin 12, el cual se encuentra en el registro CR4 por eso [3]
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI12_PB; // Le da el paso a la interrupción del Puerto B (PB12)

    // PC9: Flanco de Subida
    EXTI->RTSR |= EXTI_RTSR_TR9;  // Activa el detector de flanco de subida (Rising Trigger) 
    EXTI->FTSR &= ~EXTI_FTSR_TR9; // Apaga el flanco de bajada por seguridad, para que no cuente doble

    // PB12: Flanco de Bajada
    EXTI->RTSR &= ~EXTI_RTSR_TR12; //Apaga el flanco de subida por seguridad
    EXTI->FTSR |= EXTI_FTSR_TR12; //Activa el detector de flanco de bajada (Falling Trigger) 

    EXTI->IMR |= EXTI_IMR_MR9 | EXTI_IMR_MR12; //Interrupt Mask Register quita la máscara
    //Se activa el NVIC para que se tomen las interrupciones externas
    NVIC_EnableIRQ(EXTI9_5_IRQn); 
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void Registro_Digitos(uint8_t numero) {
    //Primero se apaga todos los segmentos (ánodo común, se apagan con 1)
    GPIOA->ODR |= GPIO_ODR_OD4;  // A
    GPIOC->ODR |= GPIO_ODR_OD1;  // B
    GPIOB->ODR |= GPIO_ODR_OD1;  // C
    GPIOB->ODR |= GPIO_ODR_OD14; // D
    GPIOB->ODR |= GPIO_ODR_OD15; // E
    GPIOB->ODR |= GPIO_ODR_OD0;  // F
    GPIOB->ODR |= GPIO_ODR_OD2;  // G

    /*Encender con 0 según el número.
    TENIENDO EN CUENTA QUE (Mapeo de Segmentos):
    como el display es de Ánodo Común (VCC constante), los segmentos se encienden 
    poniendo un 0 lógico (GND) en el registro ODR (Output Data Register).
    A -> PA4  |  B -> PC1  |  C -> PB1  |  D -> PB14  |  E -> PB15  |  F -> PB0  |  G -> PB2*/
    switch(numero) {
        //Para el 0 se encienden todos los bordes (A, B, C, D, E, F) y dejamos apagado el centro (G)
        case 0:
            GPIOA->ODR &= ~GPIO_ODR_OD4; GPIOC->ODR &= ~GPIO_ODR_OD1; GPIOB->ODR &= ~GPIO_ODR_OD1;
            GPIOB->ODR &= ~GPIO_ODR_OD14; GPIOB->ODR &= ~GPIO_ODR_OD15; GPIOB->ODR &= ~GPIO_ODR_OD0;
            break;
        // Para el 1 solo encendemos los segmentos de la derecha (B, C) 
        case 1:
            GPIOC->ODR &= ~GPIO_ODR_OD1; GPIOB->ODR &= ~GPIO_ODR_OD1;
            break;
        // Para el 2 encendemos (A, B, G, E, D)
        case 2:
            GPIOA->ODR &= ~GPIO_ODR_OD4; GPIOC->ODR &= ~GPIO_ODR_OD1; GPIOB->ODR &= ~GPIO_ODR_OD14;
            GPIOB->ODR &= ~GPIO_ODR_OD15; GPIOB->ODR &= ~GPIO_ODR_OD2;
            break;
        // Para el 3 encendemos (A, B, C, D, G)
        case 3:
            GPIOA->ODR &= ~GPIO_ODR_OD4; GPIOC->ODR &= ~GPIO_ODR_OD1; GPIOB->ODR &= ~GPIO_ODR_OD1;
            GPIOB->ODR &= ~GPIO_ODR_OD14; GPIOB->ODR &= ~GPIO_ODR_OD2;
            break;
        // Para el 4 encendemos (B, C, F, G)
        case 4:
            GPIOC->ODR &= ~GPIO_ODR_OD1; GPIOB->ODR &= ~GPIO_ODR_OD1; GPIOB->ODR &= ~GPIO_ODR_OD0;
            GPIOB->ODR &= ~GPIO_ODR_OD2;
            break;
        // Para el 5 encendemos (A, C, D, F, G)
        case 5:
            GPIOA->ODR &= ~GPIO_ODR_OD4; GPIOB->ODR &= ~GPIO_ODR_OD1; GPIOB->ODR &= ~GPIO_ODR_OD14;
            GPIOB->ODR &= ~GPIO_ODR_OD0; GPIOB->ODR &= ~GPIO_ODR_OD2;
            break;
        // Para el 6 encendemos (A, C, D, E, F, G)
        case 6:
            GPIOA->ODR &= ~GPIO_ODR_OD4; GPIOB->ODR &= ~GPIO_ODR_OD1; GPIOB->ODR &= ~GPIO_ODR_OD14;
            GPIOB->ODR &= ~GPIO_ODR_OD15; GPIOB->ODR &= ~GPIO_ODR_OD0; GPIOB->ODR &= ~GPIO_ODR_OD2;
            break;
        // Para el 7 encendemos (A, B, C)
        case 7:
            GPIOA->ODR &= ~GPIO_ODR_OD4; GPIOC->ODR &= ~GPIO_ODR_OD1; GPIOB->ODR &= ~GPIO_ODR_OD1;
            break;
        // Para el 8 encendemos todos (A, B, C, D, E, F, G)
        case 8:
            GPIOA->ODR &= ~GPIO_ODR_OD4; GPIOC->ODR &= ~GPIO_ODR_OD1; GPIOB->ODR &= ~GPIO_ODR_OD1;
            GPIOB->ODR &= ~GPIO_ODR_OD14; GPIOB->ODR &= ~GPIO_ODR_OD15; GPIOB->ODR &= ~GPIO_ODR_OD0;
            GPIOB->ODR &= ~GPIO_ODR_OD2;
            break;
        // Para el 9 encendemos todos (A, B, C, D, F, G)
        case 9:
            GPIOA->ODR &= ~GPIO_ODR_OD4; GPIOC->ODR &= ~GPIO_ODR_OD1; GPIOB->ODR &= ~GPIO_ODR_OD1;
            GPIOB->ODR &= ~GPIO_ODR_OD14; GPIOB->ODR &= ~GPIO_ODR_OD0; GPIOB->ODR &= ~GPIO_ODR_OD2;
            break;
    }
}

void Refresco_Display(void) {
    // En PNP, poner en 1 apaga el dígito en el registro ODR (Output Data Register),apagamos todos antes de cambiar de número 

    GPIOB->ODR |= GPIO_ODR_OD7;  // DIGITO 1
    GPIOA->ODR |= GPIO_ODR_OD8;  // DIGITO 2
    GPIOC->ODR |= GPIO_ODR_OD3;  // DIGITO 3
    GPIOB->ODR |= GPIO_ODR_OD13; // DIGITO 4

    // Máquina de estados
    //
    switch(DigActual) {
        case DIGITO_1:
            // Extrae la cifra de los miles: ej. Si contador = 1234 -> (1234/1000)%10 = 1
            Registro_Digitos((contador / 1000) % 10); 
            GPIOB->ODR &= ~GPIO_ODR_OD7;   //  Poner en 0 ENCIENDE el PNP
            DigActual = DIGITO_2;      // Actualiza la variable para que el siguiente dígito se ilumine     
            break;
        
        case DIGITO_2:
            Registro_Digitos((contador / 100) % 10);  
            GPIOA->ODR &= ~GPIO_ODR_OD8;               // Poner en 0 ENCIENDE el PNP
            DigActual = DIGITO_3; // Actualiza la variable para que el siguiente dígito se ilumine  
            break;
        case DIGITO_3:
            Registro_Digitos((contador / 10) % 10);   
            GPIOC->ODR &= ~GPIO_ODR_OD3;               // Poner en 0 ENCIENDE el PNP
            DigActual = DIGITO_4; // Actualiza la variable para que el siguiente dígito se ilumine  
            break;
        case DIGITO_4:
            Registro_Digitos(contador % 10);          
            GPIOB->ODR &= ~GPIO_ODR_OD13;              //  Poner en 0 ENCIENDE el PNP
            DigActual = DIGITO_1; // Actualiza la variable para que el siguiente dígito se ilumine  
            break;

    }
}
// ISR (Interrupt service request ) Handlers se encargan de que hago durante la interrupción como es una funcion del startup no se tiene que declarar en la cabecera de funciones
//Configuracion timer11 para el bllinky
void TIM1_TRG_COM_TIM11_IRQHandler(void){
    if (TIM11->SR & TIM_SR_UIF) { //Comprueba si está activa la bandera Update Interrupt Flag  del registro de estado `SR` de TIM11. Esa bandera suele activarse cuando el temporizador llega al valor de actualización
        TIM11->SR &= ~TIM_SR_UIF; //Limpia la bandera ` escribiendo un 0 en ese bit
        GPIOH->ODR ^= GPIO_ODR_OD1; //hace que cambie de estado cada vez que se atiende la interrupción.
    }
}
//Configuracion timer 4 multiplexado del display
void TIM4_IRQHandler(void){
    if (TIM4->SR & TIM_SR_UIF) {  // Comprueba si esta activa la bandera 
        TIM4->SR &= ~TIM_SR_UIF;  // Limpia la bandera
        multiplex = 1; //asigna un 1 a la variable multiplex para que esta sea revisada en el bucle 
             
    }
}
// interrupcion de la foto compuerta suma flanco de subida en PC9
void EXTI9_5_IRQHandler(void){
    if (EXTI->PR & EXTI_PR_PR9) { //PR Pending register comprueba si la interrupción provino específicamente del 9.
        EXTI->PR |= EXTI_PR_PR9; // la bandera se limpia escribiendo un 1 lógico en el bit.
        contador++; //Registra el evento icrementando la variable 
    }
}
//interrupcion de la fotocompuerta de flanco de bajada en PB12
void EXTI15_10_IRQHandler(void){
    if (EXTI->PR & EXTI_PR_PR12) { //PR Pending register comprueba si la interrupción provino específicamente del 12
        EXTI->PR |= EXTI_PR_PR12; // la bandera se limpia escribiendo un 1 lógico en el bit.
        contador--; //Registra el evento decrementando la variable
    }
}