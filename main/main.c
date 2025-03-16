#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "hardware/timer.h"

#define TRIGGER_PIN 16
#define ECHO_PIN 17

volatile bool data = false;
volatile float last_distance = 0;
volatile bool fail = false;

const char *DAYS_OF_WEEK[] = {"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado"};
const char *MONTHS[] = {"Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro"};

volatile uint32_t start_echo = 0;

void echo_irq_handler(uint gpio, uint32_t events) {
    static uint32_t end_echo;
    static uint32_t duration;

    if (events & GPIO_IRQ_EDGE_RISE) {
        start_echo = time_us_32();
        fail = false;
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        end_echo = time_us_32();
        duration = end_echo - start_echo;
        last_distance = (duration * 0.034) / 2;  
        data = true;  
    }
}

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    fail = true;
    return 0;  
}

void trigger_sensor() {
    gpio_put(TRIGGER_PIN, 1);
    sleep_us(10);
    gpio_put(TRIGGER_PIN, 0);
    add_alarm_in_ms(50, alarm_callback, NULL, false); 
}


void print_sensor_data() {
    
    datetime_t dt;
    rtc_get_datetime(&dt);

    if (data) {

        printf("%s, %02d de %s %02d:%02d:%02d - %.2f cm\n",
               DAYS_OF_WEEK[dt.dotw], dt.day, MONTHS[dt.month - 1],
               dt.hour, dt.min, dt.sec, last_distance);
        
        data = false;

    } else if (fail) {

        printf("%s, %02d de %s %02d:%02d:%02d - Falha\n",
               DAYS_OF_WEEK[dt.dotw], dt.day, MONTHS[dt.month - 1],
               dt.hour, dt.min, dt.sec);
        
        fail = false;
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

    printf("Digite 'S' para iniciar as leituras e 'P' para parar.\n");

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

        print_sensor_data();
        sleep_ms(1000);
    }
}
