#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "hardware/timer.h"

#define TRIGGER_PIN 16
#define ECHO_PIN 17

volatile bool reading_active = false;
volatile bool timer_fired = false;
volatile alarm_id_t alarm_id;
volatile uint32_t start_echo = 0; 

const char *DAYS_OF_WEEK[] = {"Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado"};
const char *MONTHS[] = {"Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro"};

void echo_irq_handler(uint gpio, uint32_t events) {
    static uint32_t end_echo;
    static uint32_t duration;
    static float distance;

    if (events & GPIO_IRQ_EDGE_RISE) {
        start_echo = time_us_32(); 
        timer_fired = false;
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        end_echo = time_us_32();
        duration = end_echo - start_echo;
        distance = (duration * 0.034) / 2;  

        datetime_t dt;
        rtc_get_datetime(&dt);

        printf("%s, %02d de %s %02d:%02d:%02d - %.2f cm\n",
               DAYS_OF_WEEK[dt.dotw], dt.day, MONTHS[dt.month - 1],
               dt.hour, dt.min, dt.sec, distance);

        cancel_alarm(alarm_id);
    }
}

int64_t alarm_callback(alarm_id_t id, void *user_data) {
    timer_fired = true;
    datetime_t dt;
    rtc_get_datetime(&dt);

    printf("%s, %02d de %s %02d:%02d:%02d - Falha\n",
           DAYS_OF_WEEK[dt.dotw], dt.day, MONTHS[dt.month - 1],
           dt.hour, dt.min, dt.sec);
    return 0;  
}

void trigger_sensor() {
    gpio_put(TRIGGER_PIN, 1);
    sleep_us(10);
    gpio_put(TRIGGER_PIN, 0);

    alarm_id = add_alarm_in_ms(50, alarm_callback, NULL, false);
}

void init_rtc() {
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
}

void init_gpio() {
    gpio_init(TRIGGER_PIN);
    gpio_init(ECHO_PIN);
    
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    
    gpio_put(TRIGGER_PIN, 0);

    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_irq_handler);
}

void read_serial_input() {
    int c = getchar_timeout_us(1000);
    if (c == 'S') {
        reading_active = true;
        printf("Iniciando...\n");
    } else if (c == 'P') {
        reading_active = false;
        printf("Parando...\n");
    }
}

int main() {
    stdio_init_all();
    init_rtc();
    init_gpio();

    printf("Digite 'S' para iniciar as leituras e 'P' para parar.\n");

    while (true) {
        read_serial_input();
        if (reading_active) {
            trigger_sensor();
            sleep_ms(1000);
        }
    }
}
