#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <wiringPiI2C.h>
#include <wiringPi.h>

#include "LCD.h"
#include "PWM.h"
#include "GPIO.h"

#define BUZZER_PIN 18   
#define LED_PIN 17  // gpio 17번 핀에 연결된 led

// 소켓으로 넘어온 정보    -- "00"이 기본
// "01" 이면 흡연 감지    ->  lcd에  "no smoking!!"
// "10" 이면 화재         ->  lcd에  "fire ~!!!"   
// "11" 이면 흡연&화재    ->  lcd에  "fire caused by smoking"

struct Scale
{
    int Do;
    int Re;
    int Mi;
    int Pa;
    int Sol;
    int Ra;
    int Si;
};

struct Scale octav = {523, 587, 659, 698, 784, 880, 988};
pthread_t lcdThreadId, buzzerThreadId, ledThreadId;

char flag[2];

void *ledThread(void *arg);
void *buzzerThread(void *arg);

void cleanup_threads(pthread_t lcdThreadId, pthread_t ledThreadId, pthread_t buzzerThreadId);
void error_handling(char *message);


int main (int argc, char *argv[]) {
    //int fd;

    int serv_sock;
    struct sockaddr_in serv_addr;

    // char *flag = argv[1];
    // memset(flag, 0, sizeof(flag));
    // strcpy(flag, "01");
    
    if(argc != 3){ // 2인지 3인지?
        printf("Usage : %s <port>\n", argv[0]);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0); // SOCK_DGRAM - TCP
    if(serv_sock == -1){
        fprintf(stderr,"Error in socket() for serv_sock\n");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    printf("Connection established\n");


    while (1)
    {
        read(serv_sock, flag, sizeof(flag));
        printf("서버파이에서 읽어온 정보: %s\n", flag);

        if (strcmp(flag, "01") == 0)    // 흡연 감지
        { 
            printf("흡연 감자!\n");
            if (pthread_create(&lcdThreadId, NULL, lcdThread, "No smoking!!") != 0)
            {
                fprintf(stderr, "LCD 쓰레드 생성 실패\n");
                break;
            }
            pthread_join(lcdThreadId, NULL);

            if (pthread_create(&ledThreadId, NULL, ledThread, NULL) != 0)
            {
                fprintf(stderr, "led 쓰레드 생성 실패\n");
                break;
            }

            pthread_join(ledThreadId, NULL);

            pthread_cancel(lcdThreadId);
            pthread_cancel(ledThreadId);
        }

        else if (strcmp(flag, "10") == 0)   // 화재 감지
        { 
            printf("화재 감지!\n");  
            if (pthread_create(&lcdThreadId, NULL, lcdThread, "fire ! ! !") != 0)
            {
                fprintf(stderr, "LCD 쓰레드 생성 실패\n");
                break;
            }
            pthread_join(lcdThreadId, NULL);

            if (pthread_create(&ledThreadId, NULL, ledThread, NULL) != 0)
            {
                fprintf(stderr, "led 쓰레드 생성 실패\n");
                break;
            }

            if (pthread_create(&buzzerThreadId, NULL, buzzerThread, NULL) != 0)
            {
                fprintf(stderr, "부저 쓰레드 생성 실패\n");
                break;
            }

            pthread_join(ledThreadId, NULL);
            pthread_join(buzzerThreadId, NULL);

            pthread_cancel(lcdThreadId);
            pthread_cancel(ledThreadId);
            pthread_cancel(buzzerThreadId);
        }

        else if (strcmp(flag, "11") == 0) // 화재 & 흡연
        { 
            printf("화재 &흡연 감자!\n");
            if (pthread_create(&lcdThreadId, NULL, lcdThread, "fire and smoking") != 0)
            {
                fprintf(stderr, "LCD 쓰레드 생성 실패\n");
                break;
            }
            pthread_join(lcdThreadId, NULL);
          
            if (pthread_create(&ledThreadId, NULL, ledThread, NULL) != 0)
            {
                fprintf(stderr, "led 쓰레드 생성 실패\n");
                break;
            }
            if (pthread_create(&buzzerThreadId, NULL, buzzerThread, NULL) != 0)
            {
                fprintf(stderr, "부저 쓰레드 생성 실패\n");
                break;
            }

            pthread_join(ledThreadId, NULL);
            pthread_join(buzzerThreadId, NULL);

            pthread_cancel(lcdThreadId);
            pthread_cancel(ledThreadId);
            pthread_cancel(buzzerThreadId);
        }

        else if (strcmp(flag, "21") == 0)   // 무단 투기
        { 
            printf("무단투기 감지!\n");
            if (pthread_create(&lcdThreadId, NULL, lcdThread, "No illegal dumping") != 0)
            {
                fprintf(stderr, "LCD 쓰레드 생성 실패\n");
                break;
            }
            pthread_join(lcdThreadId, NULL);

            if (pthread_create(&ledThreadId, NULL, ledThread, NULL) != 0)
            {
                fprintf(stderr, "led 쓰레드 생성 실패\n");
                break;
            }

            pthread_join(ledThreadId, NULL);

            pthread_cancel(ledThreadId);
        }

        else if (strcmp(flag, "20") == 0)   // 위급 상황 버튼
        { 
            printf("위급 상황 감지..\n");
            if (pthread_create(&ledThreadId, NULL, ledThread, NULL) != 0)
            {
                fprintf(stderr, "led 쓰레드 생성 실패\n");
                break;
            }

            if (pthread_create(&buzzerThreadId, NULL, buzzerThread, NULL) != 0)
            {
                fprintf(stderr, "buzzer 종료 쓰레드 생성 실패\n");
                break;
            }

            pthread_join(ledThreadId, NULL);
            pthread_join(buzzerThreadId, NULL);

            pthread_cancel(ledThreadId);            
            pthread_cancel(buzzerThreadId);
        }

        else
        {
            GPIOWrite(LED_PIN, LOW);
            printf("LED Thread finish!\n");

            if (wiringPiSetupGpio() == -1) { fprintf(stderr, "WiringPi 초기화 실패\n"); }

            printf("피에조 부저 off\n");
            pwmWrite(BUZZER_PIN, 0);

            printf("lcd clear\n");
            lcd_init(wiringPiI2CSetup(I2C_ADDR));
            ClearLcd(wiringPiI2CSetup(I2C_ADDR));
        }
        usleep(1000*1000);
    }

    if (GPIOUnexport(LED_PIN) == -1) {  fprintf(stderr, "GPIODirection err\n"); }
    cleanup_threads(lcdThreadId, ledThreadId, buzzerThreadId);

    return 0;
}

void *ledThread(void *arg) {
    int state = 0;
    int repeat = 6;
    
    if (GPIOExport(LED_PIN) == -1) { 
        fprintf(stderr, "GPIOExprot err\n");
        return  NULL; 
    }

    if (GPIODirection(LED_PIN, OUT) == -1) { 
        fprintf(stderr, "GPIODirection err\n");
        return NULL; 
    }

    printf("led 쓰레드 입장!\n");
    
    while (repeat--)
    {       
        if (state == 1) {
            GPIOWrite(LED_PIN, HIGH);
            printf("LED On\n");
        }
        else if (state == 0){
            
            GPIOWrite(LED_PIN, LOW);
            printf("LED Off\n");
        }
        else {
            fprintf(stderr, "GPIODirection err\n");
            return NULL;
        }
        state = (state == 0) ? 1 : 0;
        usleep(1000 * 500);
    }

    pthread_cancel(ledThreadId);
    return 0;
}

void *buzzerThread(void *arg) {
    int repeat = 3;

    if (wiringPiSetupGpio() == -1)
    {
        fprintf(stderr, "WiringPi 초기화 실패\n");
        return NULL;
    }

    pinMode(BUZZER_PIN, PWM_OUTPUT);

    pwmSetClock(19);
    pwmSetMode(PWM_MODE_MS);

    printf("부저 스레드 입장!!!\n");

    while (repeat --)
    {   
        printf("피에조 부저 on\n");
        pwmSetRange(1000000/octav.Si);
        pwmWrite(BUZZER_PIN, 1000000/octav.Si/2);
        usleep(1000*500);          // 0.5s 대기

        pwmSetRange(1000000/octav.Sol);
        pwmWrite(BUZZER_PIN, 1000000/octav.Sol/2);
        usleep(1000*500);          // 0.5s 대기
    }

    pthread_cancel(buzzerThreadId);
}

void cleanup_threads(pthread_t lcdThreadId, pthread_t ledThreadId, pthread_t buzzerThreadId) {
    pthread_cancel(lcdThreadId);
    pthread_cancel(ledThreadId);
    pthread_cancel(buzzerThreadId);

    pthread_join(lcdThreadId, NULL);
    pthread_join(ledThreadId, NULL);
    pthread_join(buzzerThreadId, NULL);
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}