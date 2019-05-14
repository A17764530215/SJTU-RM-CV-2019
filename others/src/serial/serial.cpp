#include <serial/serial.h>
#include <options/options.h>
#include <log.h>
using namespace std;
#include <iostream>

#ifdef Windows

Serial::Serial(UINT  baud, char  parity, UINT  databits, UINT stopsbits, DWORD dwCommEvents) :
hComm(INVALID_HANDLE_VALUE),
portNo(3),
parity(parity),
databits(databits),
stopsbits(stopsbits),
dwCommEvents(dwCommEvents){
	if (wait_uart) {
		LOGM("Waiting for serial COM%d", portNo);
		while (InitPort(portNo, baud, parity, databits, stopsbits, dwCommEvents) == false);
		LOGM("Port COM%d open success!", portNo);
	} else {
		if (InitPort(portNo, baud, parity, databits, stopsbits, dwCommEvents)) {
			LOGM("Port COM%d open success!", portNo);
		} else {
			LOGE("Port COM%d open fail!", portNo);
		}
	}
}

Serial::~Serial() {
	ClosePort();
}

void Serial::ErrorHandler() {
	if (wait_uart) {
		LOGE("Serial COM%d offline, waiting for serial COM%d", portNo, portNo);
		while (InitPort(portNo, baud, parity, databits, stopsbits, dwCommEvents) == false);
		LOGM("Port COM%d reopen success!", portNo);
	}
}

bool Serial::openPort(UINT  portNo) {
	char szPort[50];
	sprintf_s(szPort, "COM%d", portNo);

	/** ��ָ���Ĵ��� */
	hComm = CreateFileA(szPort,	      /** �豸��,COM1,COM2�� */
		GENERIC_READ | GENERIC_WRITE, /** ����ģʽ,��ͬʱ��д */
		0,                            /** ����ģʽ,0��ʾ������ */
		NULL,                         /** ��ȫ������,һ��ʹ��NULL */
		OPEN_EXISTING,                /** �ò�����ʾ�豸�������,���򴴽�ʧ�� */
		0,
		0);

	return hComm != INVALID_HANDLE_VALUE;
}

void Serial::ClosePort() {
	/** ����д��ڱ��򿪣��ر��� */
	if (hComm != INVALID_HANDLE_VALUE) {
		CloseHandle(hComm);
		hComm = INVALID_HANDLE_VALUE;
	}
}

bool Serial::InitPort(UINT  portNo, UINT baud, char parity, UINT databits, UINT stopsbits, DWORD dwCommEvents) {
	/** ��ʱ����,���ƶ�����ת��Ϊ�ַ�����ʽ,�Թ���DCB�ṹ */
	char szDCBparam[50];
	sprintf_s(szDCBparam, "baud=%d parity=%c data=%d stop=%d", baud, parity, databits, stopsbits);
	
	if (!openPort(portNo)){
		cout << "open error!" << endl;
		return false;
	}

	BOOL bIsSuccess = TRUE;
	COMMTIMEOUTS  CommTimeouts;
	CommTimeouts.ReadIntervalTimeout = 0;
	CommTimeouts.ReadTotalTimeoutMultiplier = 0;
	CommTimeouts.ReadTotalTimeoutConstant = 0;
	CommTimeouts.WriteTotalTimeoutMultiplier = 0;
	CommTimeouts.WriteTotalTimeoutConstant = 0;
	if (bIsSuccess) {
		bIsSuccess = SetCommTimeouts(hComm, &CommTimeouts);
	} else {
		cout << "SetCommTimeouts error!" << endl;
	}

	DCB  dcb;
	if (bIsSuccess)	{
		/** ��ȡ��ǰ�������ò���,���ҹ��촮��DCB���� */
		bIsSuccess = GetCommState(hComm, &dcb);
		bIsSuccess = BuildCommDCB(szDCBparam, &dcb);
		if (!bIsSuccess) {
			
			cout << "Create dcb fail with "<< GetLastError() << endl;
		}
		/** ����RTS flow���� */
		dcb.fRtsControl = RTS_CONTROL_ENABLE;
	}

	if (bIsSuccess)	{
		/** ʹ��DCB�������ô���״̬ */
		bIsSuccess = SetCommState(hComm, &dcb);
		if (!bIsSuccess) {
			cout << "SetCommState error!" << endl;
		}
	}

	/**  ��մ��ڻ����� */
	PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

	return bIsSuccess;
}

UINT Serial::GetBytesInCOM() const {
	DWORD dwError = 0;  /** ������ */
	COMSTAT  comstat;   /** COMSTAT�ṹ��,��¼ͨ���豸��״̬��Ϣ */
	memset(&comstat, 0, sizeof(COMSTAT));

	UINT BytesInQue = 0;
	/** �ڵ���ReadFile��WriteFile֮ǰ,ͨ�������������ǰ�����Ĵ����־ */
	if (ClearCommError(hComm, &dwError, &comstat)) {
		BytesInQue = comstat.cbInQue; /** ��ȡ�����뻺�����е��ֽ��� */
	}

	return BytesInQue;
}

bool Serial::WriteData(const unsigned char* pData, unsigned int length) {
	if (hComm == INVALID_HANDLE_VALUE) {
		ErrorHandler();
		return false;
	}

	/** �򻺳���д��ָ���������� */
	BOOL   bResult = TRUE;
	DWORD  BytesToSend = 0;
	bResult = WriteFile(hComm, pData, length, &BytesToSend, NULL);
	if (!bResult) {
		DWORD dwError = GetLastError();
		/** ��մ��ڻ����� */
		PurgeComm(hComm, PURGE_RXCLEAR | PURGE_RXABORT);
		ErrorHandler();
		return false;
	}
	return true;
}

bool Serial::ReadData(unsigned char *buffer,  unsigned int length) {
	if (hComm == INVALID_HANDLE_VALUE) {
		ErrorHandler();
		return false;
	}

	/** �ӻ�������ȡlength�ֽڵ����� */
	BOOL  bResult = TRUE;
	DWORD totalRead = 0, onceRead = 0;
	while (totalRead < length) {
		bResult = ReadFile(hComm, buffer, length-totalRead, &onceRead, NULL);
		totalRead += onceRead;
		if ((!bResult)) {
			/** ��ȡ������,���Ը��ݸô�����������ԭ�� */
			DWORD dwError = GetLastError();

			/** ��մ��ڻ����� */
			PurgeComm(hComm, PURGE_RXCLEAR | PURGE_RXABORT);
			ErrorHandler();
			return false;
		}
	}
	return bResult;
}

#else defined(Linux)

#include <string.h>

using namespace std;

string get_uart_dev_name(){
    FILE* ls = popen("ls /dev/ttyUSB* --color=never", "r");
    char name[20] = {0};
    fscanf(ls, "%s", name);
    return name;
}

Serial::Serial(int nSpeed, char nEvent, int nBits, int nStop) :
nSpeed(nSpeed), nEvent(nEvent), nBits(nBits), nStop(nStop){
    if(wait_uart){
        LOGM("Wait for serial be ready!");
        while(InitPort(nSpeed, nEvent, nBits, nStop) == false);
        LOGM("Port set successfully!");
    }else{
        if(InitPort(nSpeed, nEvent, nBits, nStop)){
            LOGM("Port set successfully!");
        }else{
            LOGE("Port set fail!");
        }
    }
}

Serial::~Serial() {
    close(fd);
    fd = -1;
}

bool Serial::InitPort(int nSpeed, char nEvent, int nBits, int nStop){
    string name = get_uart_dev_name();
    if(name == ""){
        return false;
    }
    if((fd=open(name.data(), O_RDWR)) < 0){
        return false;
    }
    if(set_opt(fd, nSpeed, nEvent, nBits, nStop) < 0){
        return false;
    }
    return true;
}

//int GetBytesInCOM() const {
//
//}

bool Serial::WriteData(const unsigned char* pData, unsigned int length){
    int cnt=0, curr=0;
    while((curr=write(fd, pData+cnt, length-cnt))>0 && (cnt+=curr)<length);
    if(cnt < 0){
        LOGE("Serial offline!");
        close(fd);
        if(wait_uart){
            InitPort(nSpeed, nEvent, nBits, nStop);
        }
    }
}

bool Serial::ReadData(unsigned char* buffer, unsigned int length){
    int cnt=0, curr=0;
    while((curr=read(fd, buffer+cnt, length-cnt))>0 && (cnt+=curr)<length);
    if(cnt < 0){
        LOGE("Serial offline!");
        close(fd);
        if(wait_uart){
            InitPort(nSpeed, nEvent, nBits, nStop);
        }
    }
}

int Serial::set_opt(int fd, int nSpeed, char nEvent, int nBits, int nStop) {
    termios newtio{}, oldtio{};
    if (tcgetattr(fd, &oldtio) != 0) {
        perror("SetupSerial 1");
        return -1;
    }
    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag |= CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;

    switch (nBits) {
        case 7:
            newtio.c_cflag |= CS7;break;
        case 8:
            newtio.c_cflag |= CS8;break;
        default:break;
    }

    switch (nEvent) {
        case 'O':  //��У��
            newtio.c_cflag |= PARENB;
            newtio.c_cflag |= PARODD;
            newtio.c_iflag |= (INPCK | ISTRIP);
            break;
        case 'E':  //żУ��
            newtio.c_iflag |= (INPCK | ISTRIP);
            newtio.c_cflag |= PARENB;
            newtio.c_cflag &= ~PARODD;
            break;
        case 'N':  //��У��
            newtio.c_cflag &= ~PARENB;
            break;
        default:break;
    }

    switch (nSpeed) {
        case 2400:
            cfsetispeed(&newtio, B2400);
            cfsetospeed(&newtio, B2400);
            break;
        case 4800:
            cfsetispeed(&newtio, B4800);
            cfsetospeed(&newtio, B4800);
            break;
        case 9600:
            cfsetispeed(&newtio, B9600);
            cfsetospeed(&newtio, B9600);
            break;
        case 115200:
            cfsetispeed(&newtio, B115200);
            cfsetospeed(&newtio, B115200);
            break;
        default:
            cfsetispeed(&newtio, B9600);
            cfsetospeed(&newtio, B9600);
            break;
    }

    if (nStop == 1) {
        newtio.c_cflag &= ~CSTOPB;
    } else if (nStop == 2) {
        newtio.c_cflag |= CSTOPB;
    }

    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;
    tcflush(fd, TCIFLUSH);
    if ((tcsetattr(fd, TCSANOW, &newtio)) != 0) {
        perror("com set error");
        return -1;
    }
    printf("set done!\n");

    return 0;
}

#endif /* switch os */
