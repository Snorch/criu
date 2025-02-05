#include "zdtmtst.h"
#include "sysctl.h"

const char *test_doc = "static test for ICMP socket\n";
const char *test_author = "समीर सिंह Sameer Singh <lumarzeli30@gmail.com>\n";

/* Description:
 * Send a ping to localhost using ICMP socket
 */

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>

#define PACKET_SIZE  64
#define RECV_TIMEOUT 1

static int echo_id = 1234;

int main(int argc, char **argv)
{
	int ret, sock, seq = 0;
	char packet[PACKET_SIZE], recv_packet[PACKET_SIZE];

	struct timeval tv;
	struct icmphdr icmp_header, *icmp_reply;
	struct sockaddr_in addr, recv_addr;
	socklen_t addr_len;

	test_init(argc, argv);

	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_ICMP);
	if (sock < 0) {
		pr_perror("Can't create socket");
		return 1;
	}

	tv.tv_sec = RECV_TIMEOUT;
	tv.tv_usec = 0;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
		pr_perror("Can't set socket option");
		return 1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	memset(&icmp_header, 0, sizeof(icmp_header));
	icmp_header.type = ICMP_ECHO;
	icmp_header.code = 0;
	icmp_header.un.echo.id = echo_id;
	icmp_header.un.echo.sequence = seq;

	memcpy(packet, &icmp_header, sizeof(icmp_header));
	memset(packet + sizeof(icmp_header), 0xa5,
	       PACKET_SIZE - sizeof(icmp_header));

	test_daemon();
	test_waitsig();

	ret = sendto(sock, packet, PACKET_SIZE, 0,
		     (struct sockaddr *)&addr, sizeof(addr));

	if (ret < 0) {
		fail("Can't send");
		return 1;
	}

	addr_len = sizeof(recv_addr);

	ret = recvfrom(sock, recv_packet, sizeof(recv_packet), 0,
		       (struct sockaddr *)&recv_addr, &addr_len);

	if (ret < 0) {
		fail("Can't recv");
		return 1;
	}

	icmp_reply = (struct icmphdr *)recv_packet;

	if (icmp_reply->type != ICMP_ECHOREPLY) {
		fail("Got no ICMP_ECHO_REPLY");
		return 1;
	}

	close(sock);

	pass();
	return 0;
}
