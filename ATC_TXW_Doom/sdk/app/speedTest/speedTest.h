#ifndef __SPEEDTEST_H
#define __SPEEDTEST_H
void speedTest_udp_server(int port);
void speedTest_udp_client(const char *ip,int port);
void speedTest_tcp_server(int port);
void speedTest_tcp_rx_server(int port);
void Udp_rx_server(int port,int timer);
void Udp_to_ip(char *ip,int port,int timer);
#endif
