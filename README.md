# SystemProgramming project
2023-2 시스템 프로그래밍 ajouberryfighting팀 repository<br>

---
### 팀원
- 202020722 조대희    joedaehui@ajou.ac.kr <br>
- 202020787 심재엽    jysim0326@ajou.ac.kr <br>
- 202220700 오태림    tae1231@ajou.ac.kr <br>
- 202220718 유수정    ysjsw@ajou.ac.kr <br>
<br>

### 프로젝트 설명
| 프로젝트 이름 : 스마트 도시 골목 대장 <br>
  
| 프로젝트 목표 : 골목길에서 자주 발생하는 상황들에 대하여 종합적인 대처 수행 <br>

![description](./최종%20발표_4.png) <br>
<br>

### 컴파일 방법
```
gcc -o server Server.c GPIO.c SPI.c -lpthread 
gcc -o smoking Smoking.c GPIO.c SPI.c -lptread 
gcc -o pressure Pressure.c GPIO.c SPI.c -lptread 
gcc -o notice Notice.c GPIO.c PWM.c LCD.c -lptread -lwiringPi
```
<br>

### 실행 방법
```
./server <port1> <port2> <port3>
./pressure <server_IP> <port1>
./smoking <server_IP> <port2>
./notice <server_IP> <port3>
```