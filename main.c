#include "esp_event.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "soc/ledc_reg.h"
#include "stdio.h"
// Definir pines de los segmentos del display
#define Seg_a GPIO_NUM_5
#define Seg_b GPIO_NUM_18
#define Seg_c GPIO_NUM_2
#define Seg_d GPIO_NUM_22
#define Seg_e GPIO_NUM_23
#define Seg_f GPIO_NUM_4
#define Seg_g GPIO_NUM_15
// Definir pines de los cátodos (para multiplexado)
#define Catodo_2 GPIO_NUM_19
#define Catodo_3 GPIO_NUM_21
// Definir pines de los pulsadores
#define BtnA GPIO_NUM_32
#define BtnB GPIO_NUM_33
#define BtnEncendido GPIO_NUM_26
#define BtnApagado GPIO_NUM_27
//Salidas
#define Motor GPIO_NUM_14
// Matriz de segmentos para los dígitos 0-9
uint8_t Digitos[][7] =
 {
    {1,1,1,1,1,1,0}, // 0
    {0,1,1,0,0,0,0}, // 1
    {1,1,0,1,1,0,1}, // 2
    {1,1,1,1,0,0,1}, // 3
    {0,1,1,0,0,1,1}, // 4
    {1,0,1,1,0,1,1}, // 5
    {1,0,1,1,1,1,1}, // 6
    {1,1,1,0,0,0,0}, // 7
    {1,1,1,1,1,1,1}, // 8
    {1,1,1,0,0,1,1}  // 9
};
//Apagar
void ApagarDisplays(void)
{
    gpio_set_level(Catodo_2, 1);
    gpio_set_level(Catodo_3, 1);
}
void AsignarSegmentos(uint8_t BCD_Value)
{
	gpio_set_level(Seg_a, Digitos[BCD_Value][0]);
	gpio_set_level(Seg_b, Digitos[BCD_Value][1]);
	gpio_set_level(Seg_c, Digitos[BCD_Value][2]);
	gpio_set_level(Seg_d, Digitos[BCD_Value][3]);
	gpio_set_level(Seg_e, Digitos[BCD_Value][4]);
	gpio_set_level(Seg_f, Digitos[BCD_Value][5]);
	gpio_set_level(Seg_g, Digitos[BCD_Value][6]);
}
///================================================== Configuraciones =================================================
// Configuracion del ledc timer
	ledc_timer_config_t PWM_timer = 
	    {
	        .duty_resolution = LEDC_TIMER_10_BIT, 		// resolution of PWM duty (1024)
	        .freq_hz = 10000,                      		// frequency of PWM signal
	        .speed_mode = LEDC_HIGH_SPEED_MODE,      	// timer mode
	        .timer_num = LEDC_TIMER_0,            		// timer index
	        .clk_cfg = LEDC_APB_CLK,             		// Reloj de 80Mhz
	    };
	 // Configuracion del canal del timer (cada ledc timer tiene dos canales)
	 ledc_channel_config_t PWM_channel = 
	  {
		  .channel    = LEDC_CHANNEL_0,
		  .duty       = 325, //Ciclo minimo de 325
	      .gpio_num   = 14,
		  .speed_mode = LEDC_HIGH_SPEED_MODE,
		  .intr_type = LEDC_INTR_DISABLE,
		  .hpoint     = 0,
		  .timer_sel  = LEDC_TIMER_0
	  };
//================================================== Variables Locales ==============================================
uint8_t Display, Unidades, Decenas;
int  Residuo, DutyCycle; 
int  Encendido=0;
int  Desvaneciendo=0;
int  CuentaActual;
volatile int Cuenta = 0;  // 1 = horario, -1 = antihorario, 0 = sin movimientoo
//================================================== Funciones  ====================================================
//================================================== Logica del display
void actualizar_display()
{
	//Separa el numero en cifras para asignarlas a cada dispaly
	Decenas = Cuenta/10;
	Residuo = Cuenta%10;
	Unidades = Residuo%10;
	ApagarDisplays();
	//Asigna la cifra al display
	switch(Display)
	{
		case 0:
			AsignarSegmentos(Decenas);
			gpio_set_level(Catodo_2, 0);
		break;
		case 1:
			AsignarSegmentos(Unidades);
			gpio_set_level(Catodo_3, 0);
		break;
	}
	//Siguiente display
	Display++;
	//Solo existen 2 display, cuando llegue al segundo, se reinicia
	Display &= 1;
}
 void actualizar_duty_cycle() {
	DutyCycle=(5*Cuenta)+514;

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, DutyCycle);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
}
//================================================== interrupciones =================================================
//================================================== Display
// ISR para detectar dirección de giro
static void IRAM_ATTR encoder_isr_handler(void* arg) 
{
	actualizar_duty_cycle();
    int estado_A = gpio_get_level(BtnA);
    int estado_B = gpio_get_level(BtnB);
    if (estado_A == estado_B) 
    {		
        if (Cuenta>0)
        {Cuenta=Cuenta-1;}
		else
		{Cuenta=Cuenta;}
     } 
     else 
     {
       	if (Cuenta<99)
       	{Cuenta=Cuenta+1;}
		else
		{
			Cuenta=Cuenta;
		}
     }
}
// Interrupcion de Actualizacion de alarma
static bool IRAM_ATTR ActualizacionDelDisplay(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
	BaseType_t high_task_awoken = pdFALSE;
	actualizar_display();
	return (high_task_awoken == pdTRUE); 		
}
//================================================== Boton y estado regresivo
static void IRAM_ATTR InterrupcionDeEncendido(void* arg) 
{
	
	
	if(Encendido==0)
	{
			gptimer_handle_t TimerFade = (gptimer_handle_t) arg;
	 	if(Cuenta>0)
	 	{
			 
			CuentaActual=Cuenta;
			//Indicamos el estado, o sea, encendido
			Encendido=1;
			
		 	ledc_timer_rst(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0);
			ledc_timer_resume(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0);
		 	
		 	gpio_isr_handler_remove(BtnA);
			gpio_isr_handler_remove(BtnEncendido);
			gpio_isr_handler_remove(BtnApagado);
				 if(Desvaneciendo==0)
				 {
					 Cuenta=0;
					 gptimer_set_raw_count(TimerFade, 0);			
					 gptimer_start(TimerFade);
					 Desvaneciendo=1;
				 }
		 }
	}
 	
}
static void IRAM_ATTR InterrupcionDeApagado(void* arg) 
{
	// Indicamos el estado (ecendido=0 es que esta apagado)
	
	// Velocidad del canal	/Numerodel canal	 Frecuenciadeseada	 /Tiempo en que tarda en llegar a la frecuencia deseada(ms)
	ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0, 		 2000);
	ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_WAIT_DONE);		// nada puede interrumpir esta linea de codigo (wait done)
    // ledc_timer_pause(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0);
 	gptimer_handle_t TimerFade = (gptimer_handle_t) arg;
 	if(Cuenta>0)
 	{
		Encendido=0;
		gpio_isr_handler_remove(BtnA);
		gpio_isr_handler_remove(BtnEncendido);
		gpio_isr_handler_remove(BtnApagado);
		 if(Desvaneciendo==0)
		 {
			 gptimer_set_raw_count(TimerFade, 0);			
			 gptimer_start(TimerFade);
			 Desvaneciendo=1;
		 }
	 }
 	
}
static bool IRAM_ATTR AplicarFade(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
	BaseType_t high_task_awoken = pdFALSE;
	actualizar_duty_cycle();
	
	
	if (Encendido==0)
	{
		if (Cuenta==0)
		{
			  // Detener el canal de PWM completamente
   			ledc_timer_pause(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0);
   			ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0); 
			Cuenta=0;
			Desvaneciendo=0;
			gpio_isr_handler_add(BtnA, encoder_isr_handler, NULL);
			gpio_isr_handler_add(BtnEncendido, InterrupcionDeEncendido, timer);
			gpio_isr_handler_add(BtnApagado, InterrupcionDeApagado, timer);
			gptimer_stop(timer);
		}
		else if(Cuenta>0)
		{
			Cuenta=Cuenta -1;
		}
	}
	
	else if(Encendido==1)
	{
		if (Cuenta==CuentaActual)
		{
			//Encendido=0;
			Cuenta=Cuenta;
			Desvaneciendo=0;
			gpio_isr_handler_add(BtnA, encoder_isr_handler, NULL);
			gpio_isr_handler_add(BtnEncendido, InterrupcionDeEncendido, timer);
			gpio_isr_handler_add(BtnApagado, InterrupcionDeApagado, timer);
			gptimer_stop(timer);
		}
		else if(Cuenta>=0)
		{
			Cuenta=Cuenta +1;
		}
	}
	
	
	
	return (high_task_awoken == pdTRUE); 		
}
//================================================== Main ==========================================================
void app_main(void)
{
// =========================================== 	Configuracion y habilitacion del timer ============================
 // ===========================================    TimerDisplay
  // Primero configuramos al timer de proposito general (64bits) para contar hacia arriba
	gptimer_config_t TimerConfiguracionGeneral = 
	{
		.clk_src = GPTIMER_CLK_SRC_APB,			 
		.direction = GPTIMER_COUNT_UP, 			
		.resolution_hz = 1000000,				 
	};
	// Declaramos el manejador del gptimer llamado Timerdisplay
	gptimer_handle_t TimerDisplay = NULL;		  	
	//Aplicamos Configuracion General
	gptimer_new_timer(&TimerConfiguracionGeneral, &TimerDisplay);  
	// Configuracion de la alarma
	gptimer_alarm_config_t AlarmaDisplay = 
	{
		.alarm_count = 1000,
		.reload_count = 0,
		.flags.auto_reload_on_alarm = true
	};
	// Aplicamos la configuracion de la alarma para el manejador
	gptimer_set_alarm_action(TimerDisplay, &AlarmaDisplay);
	// Establecemos que la alarma del manejador, active la siguiente interrupcion
	gptimer_event_callbacks_t CambioDeDisplay = 
	{
		.on_alarm = ActualizacionDelDisplay,
	};
	// Aplicamos el evento al manejador
	gptimer_register_event_callbacks(TimerDisplay, &CambioDeDisplay, NULL);
	// Habilitaos y empezamos el timer del display
	gptimer_enable(TimerDisplay);
	gptimer_start(TimerDisplay);
	//============================================= Timer de Fade
	gptimer_handle_t TimerFade = NULL;
 	// Aplicamos la configuracion general al nuevo timer
	gptimer_new_timer(&TimerConfiguracionGeneral, &TimerFade); 
	gptimer_alarm_config_t AlarmaCambioPwm =
	{
		.alarm_count = 10000,
		.reload_count = 0,
		.flags.auto_reload_on_alarm = true
	};
	
	// Aplicamos
	gptimer_set_alarm_action(TimerFade, &AlarmaCambioPwm);
	// Establecemos Evento
	gptimer_event_callbacks_t DisminuyePwm =
	{
		.on_alarm = AplicarFade,
	};
	// Aplicamos evento
	gptimer_register_event_callbacks(TimerFade, &DisminuyePwm, NULL);
	// Habilitaos y empezamos el timer del display
	gptimer_enable(TimerFade);
// =========================================== 	Configuracion de entradas y salidas ============================	
  // Configuración de salidas
    gpio_config_t ConfiguracionDeLasSalidas = {
        .pin_bit_mask = (1ULL << Seg_a | 1ULL << Seg_b | 1ULL << Seg_c | 
                         1ULL << Seg_d | 1ULL << Seg_e | 1ULL << Seg_f | 
                         1ULL << Seg_g | 1ULL << Catodo_2 | 1ULL << Catodo_3|
                         1ULL << Motor ),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&ConfiguracionDeLasSalidas);
    // Configuración de entradas
    gpio_config_t ConfiguracionDeLasEntradas = 
    {
        .pin_bit_mask = (1ULL << BtnA | 1ULL << BtnB| 1ULL << BtnEncendido | 1ULL << BtnApagado),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE //dETECTAR CAMBIO DE ESTADO
    };
    gpio_config(&ConfiguracionDeLasEntradas);
    // ====================================== Timers para PWM (configutacion) ============================
	// Desvanecido de ciclos de trabajo
    ledc_fade_func_install(ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_SHARED);
	// Se aplican las configuraciones
	ledc_timer_config(&PWM_timer);
	ledc_channel_config(&PWM_channel);
    // =============================================== VARIABLES DE INICIO =======================================
    //Pagardisplay 
    ApagarDisplays();
    //Variable que acumula la cuenta
    Cuenta = 0;
    //Variable que dice en que display 
    Display = 0;		
    // Detener el canal de PWM completamente
   	ledc_timer_pause(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0);
    //==================================================== Interrupciones =========================================
	// 	Activamos el servicio de interruppciones
    gpio_install_isr_service(0);
    // 
    gpio_isr_handler_add(BtnA, encoder_isr_handler, NULL);
    // 
    gpio_isr_handler_add(BtnEncendido, InterrupcionDeEncendido, (void*)TimerFade);
    // 
    gpio_isr_handler_add(BtnApagado, InterrupcionDeApagado, (void*)TimerFade);
    // =============================================== LOGICA DEL DISPLAY =========================================
    while (true) 
    {
		if(Encendido==1){
			printf("Encendido");
		}
		else if(Encendido==0){
			printf("Apagado");
		}
    	// Delay
        vTaskDelay(10/ portTICK_PERIOD_MS);
        
        
    }
  }