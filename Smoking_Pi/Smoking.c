#include<stdint.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<getopt.h>
#include<fcntl.h>
#include<sys/ioctl.h>
#include<linux/types.h>
#include<linux/spi/spidev.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<string.h>
#include<pthread.h>
#include<arpa/inet.h>
#include<sys/socket.h>

#include"GPIO.h"
#include"SPI.h"

#define IN  0
#define OUT 1

#define HIGH 1

#define fire_pin 21

char danger[5] = {0}; // 위험 상태를 나타내는 변수 fire, smoking, fire & gas 

int fire = 1; // 화재 감지 상태
float gas; // 가스 농도
int fi = 0; // 화재 지속 카운터
int gi = 0; // 가스 지속 카운터

void error_handling(char *message) {
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}

void *fire_detect(){ // 화재 감지 스레드 
    if(-1 == GPIOExport(fire_pin)){
        printf("error in fire export\n");
        exit(0);
    }

    usleep(1000*1000);

    if(-1 == GPIODirection(fire_pin, IN)){
        printf("error in fire direction\n");
        exit(0);
    }

    fi = 0;

    while(1){
        fire = GPIORead(fire_pin);
        printf("now fire: %d\n", fire);

        // 화재가 감지되면 카운터를 증가시키고 일정 횟수 이상 감지되면 danger 상태로 표시
        if(!fire && fi < 3){
           fi++;
        }
        else if(!fire && fi >= 3){
            danger[0] = '1';
        }
        else if(fire && fi > 0){
            fi--;
        }
        else{
            danger[0] = '0';
        }
        usleep(1000*1000);
    }

}

void *gas_detect(){ // 가스 감지 스레드 

    int gd = open(DEVICE, O_RDWR);
    if(gd <= 0 ){
        printf("SPI file open error!\n");
        exit(0);
    }

    if(prepare(gd) == -1){
        printf("SPI prepare error!\n");
        exit(0);
    }

    gi = 0;

    while(1){
        gas = (float)readadc(gd, 0);
        gas = (gas / 1024) * 9800 + 200;
        printf("now GAS PPM : %.2f\n", gas);

        // 가스 농도가 일정 수치 이상이면 카운터를 증가시키고 일정 횟수 이상 감지되면 danger 상태로 표시
        if(gas >= 5000 && gi < 5){
            gi++;
        }
        else if(gas >= 5000 && gi >= 5){
            danger[1] = '1';
        }
        else if(gas < 5000 && gi > 0){
            gi--;
        }
        else{
            danger[1] = '0';
        }
        usleep(1000 * 1000);
    }

}

int main(int argc, char *argv[]){
    int serv_sock;  
    pthread_t fid, gid; 

    pthread_create(&fid, NULL, fire_detect, NULL);
    pthread_create(&gid, NULL, gas_detect, NULL);
     
    char recv[2]; 

    struct sockaddr_in serv_addr;
    char msg[2] = {0};

    if(argc != 3){
        printf("Usage : %s <port>\n", argv[0]);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1){
        fprintf(stderr,"Error in socket() for serv_sock\n");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    printf("Connection established\n");
    
    danger[3] = '\0';

    while(1){
        // danger 상태에 따라 메시지를 출력하고 서버에 전송
        if(danger[0] == '1' && danger[1] == '1'){
            printf("fire and gas!\n");
            if (write(serv_sock, danger, 2) == -1) {
                perror("write() error");
                exit(1);
            }
            
        }
        else if(danger[0] == '0' && danger[1] == '1'){
            printf("smoking!\n");
            if (write(serv_sock, danger, 2) == -1) {
                perror("write() error");
                exit(1);
            }
            
        }
        else if(danger[0] == '1' && danger[1] == '0'){
            printf("fire!\n");
            if (write(serv_sock, danger, 2) == -1) {
                perror("write() error");
                exit(1);
            }
        }
        else{
            printf("safe now!\n");
        }

        write(serv_sock, danger, 2);
        usleep(500000);
    }
    
    int status;
    pthread_join(fid, (void**)&status);
    pthread_join(gid, (void**)&status);
}