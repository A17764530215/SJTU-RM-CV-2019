#include <serial/serial.h>
#include <options/options.h>
#include <log.h>
using namespace std;
#include <iostream>

Serial::Serial(UINT portNo, UINT  baud, char  parity, UINT  databits, UINT stopsbits, DWORD dwCommEvents) :
hComm(INVALID_HANDLE_VALUE),
portNo(portNo),
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
