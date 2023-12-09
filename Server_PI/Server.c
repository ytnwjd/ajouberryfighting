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

#define FD 21
#define MD 5

#define RED 17
#define GREEN 21

#define PIR_PIN 18 
#define LED_PIN 4

float cds; 
int cdi = 0; 

char danger[5] = {0};
char msg[2];
char detect[1024];

int fire = 1;
int imageIndex = 1;
#define ARRAY_SIZE(array) sizeof(array) / sizeof(array[0])

const char *DEVICE = "/dev/spidev0.0";
uint8_t MODE = SPI_MODE_0;
uint8_t BITS = 8;
uint32_t CLOCK = 1000000;
uint8_t DELAY = 5;

int prepare(int fd);
uint8_t control_bits_differential(uint8_t channel);
uint8_t control_bits(uint8_t channel);
int readadc(int fd, uint8_t channel);
int serv_sock = -1; 
int serv_sock2 = -1; 
int serv_sock3 = -1;
int smoking_sock = -1; // 흡연 감지 파이랑 통신하기 위한 소켓 (재엽)
int trash_sock = -1; // 압력 감지 파이랑 통신하기 위한 소켓 (대희)
int notice_sock = -1; // 알림 파이랑 통신하기 위한 소켓(수정)
int sensor_worked = 0;

pthread_t sid;
pthread_t cid;
pthread_t pir;
int bright=0;
// flaot *cds_detect();

struct sockaddr_in serv_addr, smoking_addr, trash_addr, notice_addr;
struct sockaddr_in serv_addr2;
struct sockaddr_in serv_addr3;
socklen_t smoking_addr_size, trash_addr_size, notice_addr_size;

void error_handling(char *message) {
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}

int prepare(int fd){
    if(ioctl(fd, SPI_IOC_WR_MODE, &MODE) == -1){
        error_handling("Can't set MODE");
        return -1;
    }

    if(ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &BITS) == -1){
        error_handling("Can't set number of BITS");
        return -1;
    }

    if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &CLOCK) == -1){
       error_handling("Can't set write CLOCK");
        return -1;
    }

    if(ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &CLOCK) == -1){
        error_handling("Can't set read CLOCK");
        return -1;
    }

    return 0;
}

uint8_t control_bits_differential(uint8_t channel){
    return (channel & 7) << 4;
}

uint8_t control_bits(uint8_t channel){
    return 0x8 | control_bits_differential(channel);
}

int readadc(int fd, uint8_t channel){
    uint8_t tx[] = {1, control_bits(channel), 0};
    uint8_t rx[3];

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = DELAY,
        .speed_hz = CLOCK,
        .bits_per_word = BITS,
    };

    if(ioctl(fd, SPI_IOC_MESSAGE(1), &tr) == 1){
        perror("IO Error");
        abort();
    }
    return ((rx[1] << 8) & 0x300) | (rx[2] & 0xFF);
}



/*
///////////   GPIO    //////////////////////
*/

int GPIOExport(int pin){
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;
	
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if(-1 == fd){
		fprintf(stderr, "Failed to open export for writing!\n");
		return -1;
	}
	
	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return 0;
}

int GPIODirection(int pin, int dir){
	static const char s_directions_str[] = "in\0out";
	
	char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
	int fd;
	
	snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	
	fd = open(path, O_WRONLY);
	if(-1 == fd){
		fprintf(stderr, "Failed to open gpio direction for writing!\n");
		return -1;
	}
	
	if(-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)){
		fprintf(stderr, "Failed to set direction!\n");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int GPIOWrite(int pin , int value){
	static const char s_values_str[] = "01";
	
	char path[VALUE_MAX];
	int fd;
	
	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_WRONLY);
	if(-1 == fd){
		fprintf(stderr, "Failed to open gpio value for writing!\n");
		return -1;
	}
	
	if(1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)){
		fprintf(stderr, "Failed to write value!\n");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int GPIORead(int pin){
	char path[VALUE_MAX];
	char value_str[3];
	int fd;

	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if(-1 == fd){
		fprintf(stderr, "Failed to open gpio value for reading!\n");
		return -1;
	}

	if(-1 == read(fd, value_str, 3)){
		fprintf(stderr, "Failed to read value!\n");
		close(fd);
		return -1;
	}

	close(fd);

	return(atoi(value_str));
}

int GPIOUnexport(int pin){
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;
	
	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if(-1 == fd){
		fprintf(stderr, "Failed to open unexport for writing!\n");
		return -1;
	}
	
	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return 0;
}

///////////////////////////////////////////////////////


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


//////////////// 조도 센서 ///////////////////

float cds_detect(){
    
    int fd = open(DEVICE, O_RDWR);
    if(fd <= 0 ){
        printf("SPI file open error!\n");
        exit(0);
    }

    if(prepare(fd) == -1){
        printf("SPI prepare error!\n");
        exit(0);
    }
    cdi = 0;
    
    cds = (float)readadc(fd, 0);
    printf("now cds PPM : %.2f\n", cds);
        
    usleep(1000 * 1000);
    return cds;
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
        // Pi4로부터 받은 내용 read
        int read_len = read(trash_sock, buffer, sizeof(buffer));

        printf("보낸내용: %s\n", buffer);
        printf("pi4 : %s\n",buffer);
        if(read_len > 0) {
            // printf("[pid:%d] received from client: %s", getpid(), buffer);

            // BUTTON 입력 시 알림 파이와 통신
            if(strncmp(buffer,"20",2) == 0){
                char temp_filename[50];
                buttonState = 1;
                printf("1번 가로등에서 비상상황 발생\n");
                // 20 알림파이로 전송
                write(notice_sock, buffer, strlen(buffer));
                capture();
                sleep(3);
                // char **open_command = (char **)malloc(sizeof(char *) * 10);
                // snprintf(temp_filename, sizeof(temp_filename), "/home/pi/Pictures/%d.JPG.jpg", imageIndex);
                // open_command[0] = "xdg-open";  // strdup("xdg-open");
                // open_command[1] = "/home/pi/Pictures/1.JPG.jpg"; // strdup(temp_filename); // "home/pi/Pictures/1.JPG.jpg";
                // open_command[2] = NULL;

                // open_command[0] = strdup("xdg-open");  // strdup("xdg-open");
                // open_command[1] = strdup(temp_filename); // strdup(temp_filename); // "home/pi/Pictures/1.JPG.jpg";
                // open_command[2] = NULL;
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
            // 클라이언트가 연결을 끊었을 때
            // printf("[pid:%d] client disconnected\n", getpid());
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
            printf("Client disconnected\n");
            break;
        }

        
        
        printf("dangerThread : %s\n",msg);
        //--------------------LED 조작---------------
        if (strcmp("10", msg) == 0 || strcmp("01", msg) == 0 || strcmp("11", msg) == 0)
        {
            state++;
            printf("state 개수: %d\n",state);
            sensor_worked = 1;
            memset(detect, 0, sizeof(detect));
            if (strcmp("10", msg) == 0 && state ==10)
            { //불꽃 감지
                
                printf("fire!!\n");
                strcpy(detect, "fire");
                capture();
                // printf("현재 상황: %s\n", detect);
                // pthread_create(&sid, NULL, camera_shot, NULL);
                // if (-1 == GPIOWrite(LED_FIRE_POUT, 1))
                // {
                //     error_handling("write() error");
                // }
                state = 0;
            }
            else if (strcmp("01", msg) == 0 && state ==10)
            { //가스 감지
                printf("smoking!!\n");
                strcpy(detect, "smoking");
                capture();
                // pthread_create(&sid, NULL, camera_shot, NULL);
                // if (-1 == GPIOWrite(LED_GAS_POUT, 1))
                // {
                //     error_handling("write() error");
                // }
                state = 0;
            }
            else if (strcmp("11", msg) == 0 && state ==10)
            { //둘다 감지
                printf("fire&gas!!\n");
                strcpy(detect, "fire_gas");
                capture();
                // pthread_create(&sid, NsULL, camera_shot, NULL);
                // if (-1 == GPIOWrite(LED_BOTH_POUT, 1))
                // {
                //     error_handling("write() error");
                // }
                state = 0;
            }
            
            // strcpy("00", msg);
        }
    }
}

///////////////모션 센서//////////////////////////////

void *pirThread(void *arg) {
printf("PIR module test\n");
    // pinMode(PIR_PIN, INPUT);
    // pinMode(LED_PIN, OUTPUT);
//    int spi_fd = open(DEVICE, O_RDWR);
//     if (spi_fd <= 0) {
//         printf("SPI file open error!\n");
//         exit(0);
//     }

//     if (prepare(spi_fd) == -1) {
//         printf("SPI prepare error!\n");
//         exit(0);
//     }

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
    int fd = open(DEVICE, O_RDWR);
    if(fd <= 0 ){
        printf("SPI file open error!\n");
        exit(0);
    }

    if(prepare(fd) == -1){
        printf("SPI prepare error!\n");
        exit(0);
    }

    
    while (1) {
        // cds_detect();
        // cds = readadc(fd, 0);

        //cds = cds_detect();
        cds=readadc(fd, 0);
        printf("cds = %lf\n", cds);
        if(cds >300){
			bright = 1;
            printf("NO need to turn on\n");
		}		
		else {
			bright = 0;
            printf("dark! need to turn on\n");
		}
        // cds_detect();

        if (GPIORead(PIR_PIN) == HIGH && bright==0) {
            GPIOWrite(GREEN, HIGH);  // LED를 켬
            printf("PIR Detected, LED On\n");

            // pthread_create(&cid, NULL, cds_detect, NULL);
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
            printf("Client disconnected\n");
            break;
        }

        printf("알림파이로 전송되는 정보: %s\n", msg);
        if (write(notice_sock, msg, strlen(msg)) == -1) {
        perror("notice.c로 쓰기 실패");
        }
        sleep(3); // 3초 간격으로 전송 (조절 가능)

        // 추가적인 로직 및 전송 요건에 따라 수정하세요
    }

    printf("Connection established - notice\n");
}






//////////////////////////main////////////////////////////

int main(int argc, char *argv[]){
    // if (wiringPiSetup() == -1) {
    //     fprintf(stderr, "Unable to initialize wiringPi\n");
    //     return 1;
    // }

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

    if(argc != 4){ // 
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
     
    GPIOWrite(GREEN, HIGH);  // LED를 켬
    
    pthread_create(&did, NULL, danger_thd, NULL);
    pthread_create(&pi4, NULL, Pi4, NULL);
    pthread_create(&nid, NULL, notice_thread, NULL);
    pthread_create(&pir, NULL, pirThread, NULL);


    usleep(500000);


    pthread_join(pi4, NULL);
    pthread_join(did, NULL);
    pthread_join(nid, NULL);
    pthread_join(pir, NULL);
}