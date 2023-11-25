#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#define BUZZER_PIN 1 // GPIO 18 핀에 연결된 피에조 부저
int repeat = 60 ;

void *buzzerThread(void *arg)
{
   
    if (wiringPiSetup() == -1)
    {
        printf("WiringPi 초기화 실패\n");
        return NULL;
    }

    softPwmCreate(BUZZER_PIN, 0, 100);

    while (repeat > 0)
    {
        printf("피에조 부저 on\n");
        softPwmWrite(BUZZER_PIN, 90); // Duty cycle 50%
        usleep(1000 * 1000);          // 1s 대기
        repeat--;
    }

    printf("피에조 부저 off\n");
    softPwmWrite(BUZZER_PIN, 0); // 부저 끄기
    usleep(1000 * 1000);         // 1s 대기

    return NULL;
}

int main()
{
    pthread_t buzzerThreadId;

    // 부저 쓰레드 생성
    if (pthread_create(&buzzerThreadId, NULL, buzzerThread, NULL) != 0)
    {
        fprintf(stderr, "부저 쓰레드 생성 실패\n");
        return 1;
    }


    // pthread_cancel(buzzerThreadId);
    pthread_join(buzzerThreadId, NULL);
    return 0;
}

// const int scale[] = {261, 294, 329, 349, 392, 440, 493, 523};

// int main() {
//     if (wiringPiSetup() == -1) {
//         printf("WiringPi 초기화 실패\n");
//         return 1;
//     }

//     softPwmCreate(BUZZER_PIN, 0, 100);  // 소프트 PWM 초기화 (GPIO 18 핀, 초기값 0, 범위 0-100)

//     while (1) {
//         printf("PWM을 이용한 피에조 부저 울리기\n");

//         // PWM을 이용하여 피에조 부저 울리기
//         for (int i = 0; i <= 100; ++i) {
//             softPwmWrite(BUZZER_PIN, i);
//             printf("%d\n", i);
//             usleep(1000*1000);  // 1s 대기
//         }

//         usleep(500000);  // 0.5초 대기

//         for (int i = 100; i >= 0; --i) {
//             softPwmWrite(BUZZER_PIN, i);
//             printf("%d!\n", i);
//             usleep(10000);  // 1s 대기
//         }

//         usleep(1000*1000);  // 0.5초 대기
//     }

//     return 0;
// }
