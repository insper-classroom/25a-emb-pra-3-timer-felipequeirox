#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "hardware/timer.h"

#define TRIGGER_PIN 16
#define ECHO_PIN 17

volatile bool fail = false;
volatile bool data = false;
volatile uint32_t start_echo = 0;
volatile float distance = 0.0;

const char *DAYS_OF_WEEK[] = {"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado"};
const char *MONTHS[] = {"Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro"};

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    *(bool *)user_data = true;  
    return 0;
}

void echo_irq_handler(uint gpio, uint32_t events) {
    
    static alarm_id_t alarm_id = -1;  
    uint32_t end_echo;
    uint32_t duration;

    if (events & GPIO_IRQ_EDGE_RISE) {
        start_echo = time_us_32();
        fail = false;
    } 
    else if (events & GPIO_IRQ_EDGE_FALL) {
        end_echo = time_us_32();

        if (start_echo > 0) {  
            duration = end_echo - start_echo;
            distance = (duration * 0.034) / 2; 
            data = true;
        }

        if (alarm_id != -1) {
            cancel_alarm(alarm_id); 
            alarm_id = -1;
        }
    }
}

void trigger_sensor() {
    static alarm_id_t alarm_id = -1; 

    gpio_put(TRIGGER_PIN, 1);
    sleep_us(15);
    gpio_put(TRIGGER_PIN, 0);

    if (alarm_id == -1) {
        alarm_id = add_alarm_in_ms(100, alarm_callback, (void *)&fail, false);
    }
}

int main() {
    
    stdio_init_all();
    rtc_init();

    datetime_t datetime = {
        .year = 2025,
        .month = 3,
        .day = 16,
        .dotw = 0,
        .hour = 19,
        .min = 10,
        .sec = 0
    };

    rtc_set_datetime(&datetime);

    gpio_init(TRIGGER_PIN);
    gpio_init(ECHO_PIN);
    
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    
    gpio_put(TRIGGER_PIN, 0);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_irq_handler);

    printf("Digite 'S' para iniciar, 'P' para parar.\n");

    bool reading_active = false;

    while (true) {
        int c = getchar_timeout_us(1000);

        if (c == 'S') {
            reading_active = true;
            printf("Iniciando...\n");
        } else if (c == 'P') {
            reading_active = false;
            printf("Parando...\n");
        }

        if (reading_active) {
            trigger_sensor();
        }

        if (data) {

            datetime_t dt;
            rtc_get_datetime(&dt);
            
            printf("%s, %02d de %s %02d:%02d:%02d - %.2f cm\n", 
                   DAYS_OF_WEEK[dt.dotw], dt.day, MONTHS[dt.month - 1], 
                   dt.hour, dt.min, dt.sec, distance);
            
            data = false; 
        } 
        else if (fail) {
            
            datetime_t dt;
            rtc_get_datetime(&dt);
            
            printf("%s, %02d de %s %02d:%02d:%02d - Falha\n", 
                   DAYS_OF_WEEK[dt.dotw], dt.day, MONTHS[dt.month - 1], 
                   dt.hour, dt.min, dt.sec);

            fail = false;  
        }

        sleep_ms(1000);
    }
}
