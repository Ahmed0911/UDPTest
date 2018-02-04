// UdpStreamer.cpp : Defines the entry point for the console application.
//

#include <stdio.h>

#if WIN32
#include <winsock2.h>
void nanosleep(DWORD waitTime);
#else
#include <sys/socket.h>
#endif

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

		int ReceiveFrame(int frameNr, char* frameBuffer, int frameSize);		
	}
	else
	{
		InitClient();

		// Send Data
		int sent = SendFrame(1, frameBuffer, FRAME_SIZE);
		printf("Sent: %d frames\n", sent);
		SendFrame(2, frameBuffer, FRAME_SIZE); // change frame number to stop receive and report!
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
		if (sendto(sock, sendBuffer, FRAME_SEND_SIZE + sizeof(sFrameHeader), 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
		{
			return i;
		}
		nanosleep(30000);
	}

	return numOfPackets;
}

int ReceiveFrame(int frameNr, char* frameBuffer, int frameSize)
{
	// Receive Data
	char buffer_to_receive[2048];
	int addrlen = sizeof(clientaddr);            /* length of addresses */
	int recvlen = recvfrom(sock, buffer_to_receive, 2048, 0, (struct sockaddr *)&clientaddr, &addrlen);

	return 0;
}

#if WIN32
// Windows Helpers
void nanosleep(DWORD waitTime) 
{
	LARGE_INTEGER perfCnt, start, now;
    QueryPerformanceFrequency(&perfCnt);
	QueryPerformanceCounter(&start);

	do {
		QueryPerformanceCounter((LARGE_INTEGER*)&now);
	} while ((now.QuadPart - start.QuadPart) / float(perfCnt.QuadPart) * 1000.0 * 1000.0 * 1000.0 < waitTime);
}
#endif
