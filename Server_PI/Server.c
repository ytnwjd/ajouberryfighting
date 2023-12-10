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

#define BUFFER_MAX 3
#define DIRECTION_MAX 45
#define VALUE_MAX 256

#define IN  0
#define OUT 1

#define LOW 0
#define HIGH 1

#define GREEN 21
#define PIR_PIN 18 

float cds; 

char danger[5] = {0};
char msg[2];
char detect[1024];

int fire = 1;
int imageIndex = 1;

int serv_sock = -1; 
int serv_sock2 = -1; 
int serv_sock3 = -1;
int smoking_sock = -1; // 흡연 감지 파이랑 통신하기 위한 소켓 (재엽)
int trash_sock = -1; // 압력 감지 파이랑 통신하기 위한 소켓 (대희)
int notice_sock = -1; // 알림 파이랑 통신하기 위한 소켓(수정)

pthread_t sid;
pthread_t cid;
pthread_t pir;
int bright=0;

struct sockaddr_in serv_addr, smoking_addr, trash_addr, notice_addr;
struct sockaddr_in serv_addr2;
struct sockaddr_in serv_addr3;
socklen_t smoking_addr_size, trash_addr_size, notice_addr_size;

void error_handling(char *message) {
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}

void capture() {      ///사진찍기 : 흡연, 가스, 안전 버튼
    char filename[50];
    snprintf(filename, sizeof(filename), "/home/pi/Pictures/%d.JPG.jpg", imageIndex);

    char command[100];
    snprintf(command, sizeof(command), "libcamera-jpeg -o %s", filename);

    system(command);

    usleep(1000 * 500); 
    FILE *fp = fopen(filename, "r");
    if (fp != NULL) {
        fclose(fp);
    }
    imageIndex++;

}

//////////////// 쓰레기 감지 파이 ///////////////////

void* Pi4(void* arg) {

    char buffer[1024];
    int buttonState = 0;

    // Pi4와 소켓 통신 연결
    if(bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr))== -1) {
        fprintf(stderr, "Error in bind() for serv1\n");
        exit(0);
    }

    if(listen(serv_sock, 5) == -1){
        fprintf(stderr, "listen error!\n");
        exit(0);    
    }

    if (trash_sock < 0) {
        trash_addr_size = sizeof(trash_addr);
        trash_sock = accept(serv_sock, (struct sockaddr *)&trash_addr, &trash_addr_size);
        if (trash_sock == -1) error_handling("accept() error");
    }

    printf("Connection established - pi4\n");

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        // 압력 파이로부터 받은 내용 read
        int read_len = read(trash_sock, buffer, sizeof(buffer));

        printf("압력 파이 받은 내용: %s\n", buffer);
        if(read_len > 0) {

            // BUTTON 입력 시 알림 파이와 통신
            if(strncmp(buffer,"20",2) == 0){
                char temp_filename[50];
                buttonState = 1;
                printf("1번 가로등에서 비상상황 발생\n");
                // 20 알림파이로 전송
                write(notice_sock, buffer, strlen(buffer));
                capture();
                sleep(3);
            } 
            // 압력 센서 입력 시 알림 파이와 통신
            else if(strncmp(buffer, "21", 2) == 0){
                int time = atoi(buffer +5);
                printf("pressure time val: %d\n", time);
                write(notice_sock, buffer, strlen(buffer));
                capture();
            }
        }
        else if(read_len == 0) {
            printf("Client disconnected - trash\n");
            break;
        }
        else {
            // 오류 발생 시
            perror("recv failed");
            break;
        }
    }

    close(trash_sock);
    pthread_detach(pthread_self());

    return NULL;
}

/////////////////흡연센서 가스센서 ////////////

void *danger_thd() //계속 센서 4가지 상황을 read 함.
{    

    if(bind(serv_sock2, (struct sockaddr*) &serv_addr2, sizeof(serv_addr2))== -1) {
        fprintf(stderr, "Error in bind() for serv2\n");
        exit(0);
    }

    if(listen(serv_sock2, 5) == -1){
        fprintf(stderr, "listen error!\n");
        exit(0);
    }    

    if (smoking_sock < 0) {
        smoking_addr_size = sizeof(smoking_addr);
        smoking_sock =
            accept(serv_sock2, (struct sockaddr *)&smoking_addr, &smoking_addr_size);
    if (smoking_sock == -1) error_handling("accept() error");
    }

    printf("Connection established - danger\n");
    

    int state = 0;

    while (1)
    {

        memset(msg, 0, sizeof(msg));
        int str_len = read(smoking_sock, msg, sizeof(msg));
        if (str_len == -1 ) {
            fprintf(stderr, "read error!\n");
            exit(0);
        } else if (str_len == 0) {
            printf("Client disconnected - smoking\n");
            break;
        }

        printf("dangerThread : %s\n",msg);
        //--------------------LED 조작---------------
        if (strcmp("10", msg) == 0 || strcmp("01", msg) == 0 || strcmp("11", msg) == 0)
        {
            state++;
            printf("state 개수: %d\n",state);
            memset(detect, 0, sizeof(detect));
            if (strcmp("10", msg) == 0 && state ==10)
            { //불꽃 감지
                
                printf("fire!!\n");
                strcpy(detect, "fire");
                capture();
                state = 0;
            }
            else if (strcmp("01", msg) == 0 && state ==10)
            { //가스 감지
                printf("smoking!!\n");
                strcpy(detect, "smoking");
                capture();
                state = 0;
            }
            else if (strcmp("11", msg) == 0 && state ==10)
            { //둘다 감지
                printf("fire&gas!!\n");
                strcpy(detect, "fire_gas");
                capture();
                state = 0;
            }
            
        }
    }
}

///////////////모션 센서//////////////////////////////

void *pirThread(void *arg) {
    printf("PIR module test\n");

    if(-1 == GPIOExport(PIR_PIN) ){
        printf("error in pir export\n");
        exit(0);
    }

    usleep(1000*1000);

    if(-1 == GPIODirection(PIR_PIN, IN) ){
        printf("error in pir direction\n");
        exit(0);
    }

    
    int pirfd = open(DEVICE, O_RDWR);
    if(pirfd <= 0 ){
        printf("SPI file open error!\n");
        exit(0);
    }
    usleep(1000*1000);

    if(prepare(pirfd) == -1){
        printf("SPI prepare error!\n");
        exit(0);
    }
    
    while (1) {
        cds=readadc(pirfd, 0); // 조도 센서 값 읽어오기
        printf("cds = %lf\n", cds);
        if(cds >300){
			bright = 1;
            printf("NO need to turn on\n");
		}		
		else {
			bright = 0;
            printf("dark! need to turn on\n");
		}

        if (GPIORead(PIR_PIN) == HIGH && bright==0) {
            GPIOWrite(GREEN, HIGH);  // LED를 켬
            printf("PIR Detected, LED On\n");

            usleep(1000 * 7000);
        } else {
            GPIOWrite(GREEN, LOW);   // LED를 끔
            printf("PIR Undetected, LED Off\n");
        }
         usleep(1000 * 1000);
    }

    return NULL;
}

//////////////알림 파이///////////////////////////


void *notice_thread() {

    if(bind(serv_sock3, (struct sockaddr*) &serv_addr3, sizeof(serv_addr3))== -1) {
        fprintf(stderr, "Error in bind() for serv2\n");
        exit(0);
    }

    if(listen(serv_sock3, 5) == -1){
        fprintf(stderr, "listen error!\n");
        exit(0);
    }    

    if (notice_sock < 0) {
        notice_addr_size = sizeof(notice_addr);
        notice_sock =
            accept(serv_sock3, (struct sockaddr *)&notice_addr, &notice_addr_size);
    if (notice_sock == -1) error_handling("accept() error");
    }

    printf("Connection established - danger\n");

    while (1) {
        memset(msg, 0, sizeof(msg));
        int str_len = read(smoking_sock, msg, sizeof(msg));
        if (str_len == -1 ) {
            fprintf(stderr, "read error!\n");
            exit(0);
        } else if (str_len == 0) {
            printf("Client disconnected - notice\n");
            break;
        }

        printf("알림파이로 전송되는 정보: %s\n", msg);
        if (write(notice_sock, msg, strlen(msg)) == -1) {
        perror("notice.c로 쓰기 실패");
        }
        sleep(3); // 3초 간격으로 전송 (조절 가능)

    }

    printf("Connection established - notice\n");
}

//////////////////////////main////////////////////////////

int main(int argc, char *argv[]){
    if(GPIOExport(GREEN) == -1){
        printf("green export error\n");
        exit(0);
    }
    
    if(GPIODirection(GREEN, OUT) == -1){
        printf("green direction error\n");
        exit(0);
    }

 
    char recv[2]; 

    pthread_t did;
    pthread_t pi4;
    pthread_t nid;
    pthread_t pir;
    
    char msg[2] = {0}; 

    char message[1024]; // 버퍼로 메시지를 주고 받음

    if(argc != 4){ 
        printf("Usage : %s <port>\n", argv[0]);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0); 
    if(serv_sock == -1){
        fprintf(stderr,"Error in socket() for serv_sock\n");
    }
    
    serv_sock2 = socket(PF_INET, SOCK_STREAM, 0); 
    if(serv_sock2 == -1){
        fprintf(stderr,"Error in socket() for serv_sock\n");
    }

    serv_sock3 = socket(PF_INET, SOCK_STREAM, 0); 
    if(serv_sock3 == -1){
        fprintf(stderr,"Error in socket() for serv_sock\n");
    }


    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1])); // Pi4

    memset(&serv_addr2, 0, sizeof(serv_addr2));
    serv_addr2.sin_family = AF_INET;
    serv_addr2.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr2.sin_port = htons(atoi(argv[2])); // Smoking

    memset(&serv_addr3, 0, sizeof(serv_addr3));
    serv_addr3.sin_family = AF_INET;
    serv_addr3.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr3.sin_port = htons(atoi(argv[3])); // notice
    
    pthread_create(&did, NULL, danger_thd, NULL);
    pthread_create(&pi4, NULL, Pi4, NULL);
    pthread_create(&nid, NULL, notice_thread, NULL);
    pthread_create(&pir, NULL, pirThread, NULL);


    usleep(1000 * 500);


    pthread_join(pi4, NULL);
    pthread_join(did, NULL);
    pthread_join(nid, NULL);
    pthread_join(pir, NULL);
}