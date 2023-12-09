#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include"GPIO.h"
#include"SPI.h"

#define IN 0
#define OUT 1
#define LOW 0
#define HIGH 1
#define BUTTON 20
#define PIN 21
char button_state[1024];

int pressure_state;
int gid;

#define GPS_SERIAL_PORT "/dev/serial0"  // Raspberry Pi의 시리얼 포트
#define GPS_BAUD_RATE 9600  // GPS 모듈의 통신 속도

// void initializeGPS() {

//     serialPrintf(serialOpen(GPS_SERIAL_PORT, GPS_BAUD_RATE), "$PMTK251,9600*17\r\n");

//     delay(1000);

//     serialPrintf(serialOpen(GPS_SERIAL_PORT, GPS_BAUD_RATE), "$PMTK220,100*2F\r\n");

//     delay(1000);

// }

// void readGPSData(char *buffer, int bufferSize) {

//     int serialPort = serialOpen(GPS_SERIAL_PORT, GPS_BAUD_RATE);

//     int bytesRead = serialDataAvail(serialPort);

//     if (bytesRead > 0) {

//         bytesRead = (bytesRead > bufferSize) ? bufferSize : bytesRead;

//         serialRead(serialPort, buffer, bytesRead);

//         buffer[bytesRead] = '\0';

//     }

// }

// void *GPS_thread(void *arg) {

//  if (wiringPiSetup() == -1) {

//         fprintf(stderr, "WiringPi initialization failed.\n");

//     }

//     initializeGPS();

//  int state = 3;

//     while (state--) {

//         char buffer[256];

//         readGPSData(buffer, sizeof(buffer));

//         if (strlen(buffer) > 0) {

//             // 예제에서는 GPGGA 메시지에서 좌표 정보를 추출합니다.

//             char *token = strtok(buffer, ",");

//             while (token != NULL) {

//                 if (strcmp(token, "$GPGGA") == 0) {

//                     // 좌표 정보가 있는 인덱스로 이동

//                     int i;

//                     for (i = 0; i < 6; ++i) {

//                         token = strtok(NULL, ",");

//                     }

//                     // 좌표 정보 출력

//                     printf("Latitude: %s, Longitude: %s\n", token, strtok(NULL, ","));

//                     break;

//                 }

//                 token = strtok(NULL, ",");

//             }

//         }

//         delay(1000);  // 1초마다 GPS 데이터를 확인

//     }

// }

/******************************************************

* error 출력

*******************************************************/

void error_print(char *message)

{

fputs(message, stderr);

fputc('\n', stderr);

exit(1);

}

/******************************************************

* socket thread

*******************************************************/

void *socket_thread(void *data)

{

char **argv = (char **)data;

int sock;

struct sockaddr_in serv_addr;

char msg[1024];

// socket 생성

sock = socket(PF_INET, SOCK_STREAM, 0);

if (sock == -1)

error_print("socket() error");

// port, ip 할당

memset(&serv_addr, 0, sizeof(serv_addr));

serv_addr.sin_family = AF_INET;

serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

serv_addr.sin_port = htons(atoi(argv[2]));

// socket과 연결

if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)

error_print("connect() error");

printf("Connection established\n");

// 압력센서 눌린 시간 확인

struct timeval start, end;

long min, sec, usecond;

while (1)

{

// button이 눌리면 눌렸다고 socekt 통신

if(strcmp(button_state, "ON") == 0){

// pthread_create(&gid, NULL, GPS_thread, NULL);

// pthread_join(gid, NULL);

printf("send button state to server\n");

// 눌린 경우 20 서버로 전송

strcpy(msg, "20");

write(sock, msg, strlen(msg));

usleep(1000000);

}

// pressure state 변경 시 시간 측정

if(pressure_state == 1){

gettimeofday(&start, NULL);

printf("send pressure state to server\n");

// sprintf(msg, "%d", pressure_state);

// write(sock, msg, strlen(msg));

while(pressure_state == 1)

usleep(1000000);

// pressure가 눌린 시간 보내기

gettimeofday(&end, NULL);

sec = end.tv_sec - start.tv_sec;

usecond = end.tv_usec - start.tv_usec;

min = ((sec) * 1000 + usecond/1000.0) + 1;

printf("sec == %d\n", min/1000);

// 사용자 지정한 시간이상의 압력이 눌릴 시 서버로 21 전송

if((min/1000) > 1){

char prefix[1024];

strcpy(prefix, "21");

write(sock, prefix, strlen(prefix));

usleep(1000000);

}

}

}

close(sock);

}

/******************************************************

* button initialization

* button thread

*******************************************************/

// button 사용 전 초기화

void button_init()

{

if (GPIOExport(BUTTON) || -1 == GPIOExport(PIN))

exit(1);

if (-1 == GPIODirection(BUTTON, IN) || -1 == GPIODirection(PIN, OUT))

exit(1);

}

// button thread

void *button_thread(void *data)

{

int state;

button_init();

while (1)

{

if(GPIOWrite(PIN, 1) == -1){

exit(1);

}

// button 눌리면 state ON으로 변환

state = GPIORead(BUTTON);

if(state == LOW){

strcpy(button_state, "ON");

printf("BUTTON PRESSED\n");

}

else

strcpy(button_state, "OFF");

usleep(1000000);

}

/*if(-1 == GPIOUnexport(POUT) || -1 == GPIOUnexport(PIN)){

return;

}*/

exit(0);

}

/******************************************************

* pressure thread

*******************************************************/

void *pressure_thread(){

int pressure=0;

// 압력 센서 device

int fd = open(DEVICE, O_RDWR);

// 연결 확인

if(fd <= 0){

printf("Device %s not found\n", DEVICE);

exit(0);

}

if(prepare(fd) == -1){

printf("Device is not prepared\n");

exit(0);

}

while(1){

// 일정수치 이상 pressure -> state 변환

pressure = readadc(fd, 0);

if(pressure >= 200){

pressure_state = 1;

printf("pressure state: %d\n", pressure_state);

printf("pressure value: %d\n", pressure);

}

else

pressure_state = 0;

usleep(500000);

}

close(fd);

}

/******************************************************

* main function

*******************************************************/

int main(int argc, char *argv[])

{

pthread_t p_thread[3];

int thr_id, status;

char p1[] = "button";

char p2[] = "pressure";

if (argc != 3)

{

printf("Usage : %s <IP> <port>\n", argv[0]);

exit(1);

}

// 서버와 정보 수신할 socket thread create

// 버튼 기능, 압력 센서 기능 thread create

thr_id = pthread_create(&p_thread[0], NULL, socket_thread, argv);

thr_id = pthread_create(&p_thread[1], NULL, button_thread, (void *)p1);

thr_id = pthread_create(&p_thread[2], NULL, pressure_thread, (void *)p2);

// thread join

pthread_join(p_thread[0], (void **)&status);

pthread_join(p_thread[1], (void **)&status);

pthread_join(p_thread[2], (void **)&status);

return 0;

}