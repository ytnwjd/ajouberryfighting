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
#define BUTTON_PIN 21
char button_state[1024];
int pressure_state;

pthread_t gid;
int sock;

#define GPS_SERIAL_PORT "/dev/serial0"  // Raspberry Pi의 GPS 사용 시리얼 포트
#define GPS_BAUD_RATE 9600  // GPS 모듈의 통신 속도

/******************************************************
 * error 출력
 *******************************************************/
void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

void readGPSData(int serialPort, char *buffer, int bufferSize) {
    int bytesRead = 0;
    char currentChar;

    // 줄바꿈 문자가 나올 때까지 문자를 읽음
    while (currentChar != '\n' && bytesRead < bufferSize - 1) {
        currentChar = serialGetchar(serialPort);

        // 읽은 문자를 버퍼에 추가
        if (currentChar != '\n') {
            buffer[bytesRead] = currentChar;
            bytesRead++;
        }
    }

    buffer[bytesRead] = '\0';
}


void *GPS_thread(void *arg) {
	if (wiringPiSetup() == -1) {
        error_handling("WiringPi initialization failed.\n");
		exit(1);
    }

	printf("GPS thread 입장\n");

	int serialPort = serialOpen(GPS_SERIAL_PORT, GPS_BAUD_RATE);
    if (serialPort == -1) {
        error_handling("Failed to open serial port.\n");
		exit(1);
    }

	// system("cat /dev/serial0");

	int state = 7;
    while (state--) {
        char buffer[1024];
		readGPSData(serialPort, buffer, sizeof(buffer));

        if (strlen(buffer) > 0) {

			printf("Received line: %s\n", buffer);
			write(sock, buffer, strlen(buffer));
        }

        usleep(1000 * 5000);  
    }



}


/******************************************************
 * socket thread
 *******************************************************/
void *socket_thread(void *data)
{
	char **argv = (char **)data;
	struct sockaddr_in serv_addr;
	char msg[1024];

	// socket 생성
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		error_handling("socket() error");

	// port, ip 할당
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

	// socket과 연결
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
		error_handling("connect() error");

	printf("Connection established\n");

	// 압력센서 눌린 시간 확인
    struct timeval start, end;
    long min, sec, usecond;

	while (1)
	{
        // button이 눌리면 눌렸다고 socekt 통신
        if(strcmp(button_state, "ON") == 0){
            printf("send button state to server\n");
			
			// 눌린 경우 20 서버로 전송
			strcpy(msg, "20");
            write(sock, msg, strlen(msg));
			pthread_create(&gid, NULL, GPS_thread, NULL);
			pthread_join(gid, NULL);
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
			if((min/1000) > 3){
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
	if (GPIOExport(BUTTON) || -1 == GPIOExport(BUTTON_PIN))
		exit(1);

	if (-1 == GPIODirection(BUTTON, IN) || -1 == GPIODirection(BUTTON_PIN, OUT))
		exit(1);
}

// button thread
void *button_thread(void *data)
{
    int state;
    button_init();
	while (1)
	{
        if(GPIOWrite(BUTTON_PIN, 1) == -1){
            exit(1);
        }
        // button 눌리면 state ON으로 변환
        state = GPIORead(BUTTON);
        if(state == LOW){
            strcpy(button_state, "ON");
            printf("BUTTON PRESSED\n");
			pthread_create(&gid, NULL, GPS_thread, NULL);
        }
        else 
            strcpy(button_state, "OFF");
        usleep(1000000);
	}

	/*if(-1 == GPIOUnexport(POUT) || -1 == GPIOUnexport(BUTTON_PIN)){
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
		if(pressure >= 300){
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