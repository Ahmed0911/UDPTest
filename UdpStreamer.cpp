#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if WIN32
#include <winsock2.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#endif

//#define CLIENT_IP "10.12.3.16"
//#define SERVER_IP "10.12.7.253"
#define CLIENT_IP "192.168.0.19"
#define SERVER_IP "192.168.0.20"
#define SERVER_PORT 12345
#define FRAME_SEND_SIZE 1450 /* Without Header */
#define FRAME_SIZE (FRAME_SEND_SIZE*100000) /* Total Frame Size in bytes */

struct sFrameHeader
{
	int FrameNumber; // frame ID
	int PacketNumber; // packet within frame (0...MAXBLOCKS)
};

int InitClient();
int InitServer();
int SendFrame(int frameNr, char* frameBuffer, int frameSize);
int ReceiveFrame(int frameNr, char* frameBuffer, int frameSize);

// Timer stuff
void StartTimer();
double GetElapsedTimeSec();
void MuSleep(unsigned int waitTimeUS);

// global vars
int sock;
struct sockaddr_in clientaddr; // client/remote address
struct sockaddr_in serveraddr; // server address

// Frame Buffer
char frameBuffer[FRAME_SIZE];

int main()
{
	bool server = false;

#if WIN32
	// Init winsock WINDOWS ONLY
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}
#endif

	if (server)
	{
		InitServer();

		// Receive One Frame
		printf("Waiting for Frame...\n");
		int receivedPackets = ReceiveFrame(1, frameBuffer, int FRAME_SIZE);
		printf("Received: %d frames (%0.2f %%)\n", receivedPackets, receivedPackets/100000.0 * 100);
	}
	else
	{
		InitClient();

		// Send Data
		printf("Sending Frame...\n");
		StartTimer();
		int sent = SendFrame(1, frameBuffer, FRAME_SIZE);
		double seconds = GetElapsedTimeSec();
		double Mbps = (double)sent*FRAME_SEND_SIZE * 8 * 1e-6 / seconds; // doesn't include overhead!!!
		printf("Sent: %d frames in %0.2f seconds [%0.2f Mbps, %0.2f MByte/s]\n", sent, seconds, Mbps, Mbps/8);
		SendFrame(2, frameBuffer, FRAME_SEND_SIZE*10); // change frame number to stop receive and report!
	}
	

	// Windows stuff
#if WIN32
	WSACleanup();
#endif

    return 0;
}

int InitClient()
{
	// create socket
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	// Resolve Server Address
	struct hostent *hp_server;
	hp_server = gethostbyname(SERVER_IP);
	
	// set address
	struct sockaddr_in myaddr;
	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY); // choose first interface (hopefully eth0)
	myaddr.sin_port = htons(0); // let system choose random port

	if (bind(sock, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return -1;
	}

	/* fill in the server's address and data */
	memset((char*)&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(SERVER_PORT);
	memcpy((void *)&serveraddr.sin_addr, hp_server->h_addr_list[0], hp_server->h_length);

	// "connect" to server (use send/write instead of sendto)
	if (connect(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
	{
		perror("connect  failed");
		return -1;
	}

	return 0;
}

int InitServer()
{
	// create socket
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	// Resolve Server + Client address
	struct hostent *hp_client;
	hp_client = gethostbyname(CLIENT_IP);

	// set address
	struct sockaddr_in myaddr;
	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY); // choose first interface (hopefully eth0)
	myaddr.sin_port = htons(SERVER_PORT); // Server port is fixed

	if (bind(sock, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return -1;
	}

	/* fill in the client's (remote) address and data */
	memset((char*)&clientaddr, 0, sizeof(clientaddr));
	clientaddr.sin_family = AF_INET;
	clientaddr.sin_port = htons(SERVER_PORT);
	memcpy((void *)&clientaddr.sin_addr, hp_client->h_addr_list[0], hp_client->h_length);

	return 0;
}


int SendFrame(int frameNr, char* frameBuffer, int frameSize)
{
	int numOfPackets = frameSize / FRAME_SEND_SIZE;

	char sendBuffer[FRAME_SEND_SIZE + sizeof(sFrameHeader)];
	for (int i = 0; i != numOfPackets; i++)
	{
		// add header
		sFrameHeader header;
		header.FrameNumber = frameNr;
		header.PacketNumber = i;
		memcpy(&sendBuffer[0], &header, sizeof(sFrameHeader));
		memcpy(&sendBuffer[sizeof(sFrameHeader)], frameBuffer+i*FRAME_SEND_SIZE, FRAME_SEND_SIZE); // TODO: fix last packet size!
		if (send(sock, sendBuffer, FRAME_SEND_SIZE + sizeof(sFrameHeader), 0) < 0)
		{
			return i;
		}
		MuSleep(20); // TODO!!!
	}

	return numOfPackets;
}

int ReceiveFrame(int frameNr, char* frameBuffer, int frameSize)
{
	int receivedPackets = 0;

	// Receive Data
	sFrameHeader header;
	header.FrameNumber = frameNr;
	char buffer_to_receive[2048];	
	do
	{
		socklen_t addrlen = sizeof(clientaddr);            /* length of addresses */
		int recvlen = recvfrom(sock, buffer_to_receive, 2048, 0, (struct sockaddr *)&clientaddr, &addrlen);
		if (recvlen == (FRAME_SEND_SIZE + sizeof(sFrameHeader)) )
		{
			// valid frame
			memcpy(&header, &buffer_to_receive[0], sizeof(sFrameHeader));
			if(header.FrameNumber == frameNr) receivedPackets++; // increase only if corrent frameNr

			// TODO: COPY DATA
			//memcpy(frameBuffer + header.PacketNumber *FRAME_SEND_SIZE, &buffer_to_receive[sizeof(sFrameHeader)], FRAME_SEND_SIZE); // TODO: fix last packet size!
		}
	} while (header.FrameNumber == frameNr);

	return receivedPackets;
}

#if WIN32
// Windows Helpers
void MuSleep(unsigned int waitTimeUS)
{
	LARGE_INTEGER perfCnt, start, now;
    QueryPerformanceFrequency(&perfCnt);
	QueryPerformanceCounter(&start);

	do {
		QueryPerformanceCounter((LARGE_INTEGER*)&now);
	} while ((now.QuadPart - start.QuadPart) / float(perfCnt.QuadPart) * 1000.0 * 1000.0  < waitTimeUS);
}

LARGE_INTEGER liStartTime;
void StartTimer()
{
	QueryPerformanceCounter(&liStartTime);
}

double GetElapsedTimeSec()
{
	LARGE_INTEGER perfCnt, now;
	QueryPerformanceFrequency(&perfCnt);
	QueryPerformanceCounter((LARGE_INTEGER*)&now);

	double timeSeconds = ((now.QuadPart - liStartTime.QuadPart) / float(perfCnt.QuadPart));

	return timeSeconds;
}
#else
void MuSleep(unsigned int waitTimeUS)
{
	struct timespec start, now;
	clock_gettime(CLOCK_MONOTONIC, &start);
	double timeStart = start.tv_sec + (double)start.tv_nsec * 1e-9;
	double timeNow;

	do
	{
		clock_gettime(CLOCK_MONOTONIC, &now);
		timeNow = now.tv_sec + (double)now.tv_nsec * 1e-9;
	} while ( (timeNow - timeStart) < ((double)waitTimeUS * 1e-6) );
}

double dStartTime;
void StartTimer()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	double time = ts.tv_sec + (double)ts.tv_nsec * 1e-9;
	dStartTime = time;
}

double GetElapsedTimeSec()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	double time = ts.tv_sec + (double)ts.tv_nsec * 1e-9;

	return time - dStartTime;
}
#endif

