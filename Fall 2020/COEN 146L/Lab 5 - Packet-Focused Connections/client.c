#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "oversight.h"

// NOTE: beware of sign extension if the checksum is stored in a char, then converted to an int!
int getChecksum(Packet packet) {
    packet.header.cksum = 0;

    int checksum = 0;
    char *ptr = (char *)&packet;
    char *end = ptr + sizeof(Header) + packet.header.len;

//Please find an error from the remaining part getChecksum() function 
    while (ptr < end) {
        checksum ^= *ptr++;
    }

    return checksum;
}

void logPacket(Packet packet) {
    printf("Packet{ header: { seq_ack: %d, len: %d, cksum: %d }, data: \"",
            packet.header.seq_ack,
            packet.header.len,
            packet.header.cksum);
    fwrite(packet.data, (size_t)packet.header.len, 1, stdout);
    printf("\" }\n");
}   

void ClientSend(int sockfd, const struct sockaddr *address, socklen_t addrlen, Packet packet) {
    while (1) {

        // mess up the checksum
        int randomAgain;
        randomAgain = rand() % 100;
        if (randomAgain < 20)
        {
            packet.header.cksum = 101; 
        } else {
            packet.header.cksum = getChecksum(packet);
        }

        // send the packet
        printf("Sending packet\n");
		sendto(sockfd, &packet, sizeof(packet), 0, address, addrlen);

        // receive an ACK from the server
        Packet recvpacket;
		recvfrom(sockfd, &recvpacket, sizeof(recvpacket), 0, address, &addrlen);

        // log what was received
        printf("Received ACK %d, checksum %d - ", recvpacket.header.seq_ack, recvpacket.header.cksum);

        // validate the ACK
        if (recvpacket.header.cksum != getChecksum(recvpacket)) 
        {
            // bad checksum, resend packet
            printf("Bad checksum, expected checksum was: %d\n", getChecksum(recvpacket));
        
        } else if (recvpacket.header.seq_ack != packet.header.seq_ack)
        { 
            // incorrect sequence number, resend packet
            printf("Bad seqnum, expected sequence number was: %d\n", packet.header.seq_ack);
        
        } else {
            // good ACK, we're done
            printf("Good ACK\n");
            break;
        }
    }

    printf("\n");
}

int main(int argc, char *argv[]) {
    // check arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip> <port> <infile>\n", argv[0]);
        return 1;
    }

    // seed the RNG
    srand((unsigned)time(NULL));

    // create and configure the socket [name the socket sockfd] 
	int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Could not setup socket.");
        exit(1);
    }	    
    
    // initialize the server address structure using argv[2] and argv[1]
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
	
    socklen_t addr_len = sizeof(address);
    struct hostent *host;
    host = (struct hostent *) gethostbyname(argv[1]);

    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(argv[2]));
    address.sin_addr = *((struct in_addr *)host->h_addr);

    // open file using argv[3]
	int myfile = open(argv[3], O_RDWR);

    // send file contents to server
	//-create a packet
	//-set the seq_ack field 
	//-set the len field
	//-calculate the checksum
	//-send the packet
	Packet packet;
    int seqnum = 0;
    while (read(myfile, packet.data, sizeof(packet.data)) > 0)
    {
        // Actual packet code
        packet.header.seq_ack = seqnum;
        packet.header.len = strlen(packet.data);
        packet.header.cksum = getChecksum(packet);

        ClientSend(sockfd, (struct sockaddr *)&address, addr_len, packet);
        
        // some equation needed for the sequence number
        seqnum = (seqnum + 1) % 2;

    }
    // send zero-length packet to server to end connection
	Packet zero;
    zero.header.seq_ack = seqnum;
    zero.header.len = 0;
    zero.header.cksum = getChecksum(zero);
    ClientSend(sockfd, (struct sockaddr *)&address, addr_len, zero);

    // clean up
	//-close the sockets and file
    close(myfile);
    close(sockfd);

    return 0;
}