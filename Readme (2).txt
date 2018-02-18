In this programming assignment we implement a TFTP server. It uses the UDP to transfer file. A default TFTP
client is used. Server is able to handle multiple file transfer. TFTP client makes a read request to read 
the files from the server. Server uses the fork function to handle multiple clients, whenever the server 
receives a new packet server has create a new port and start communicating on that port so that new connections
can be accepted on the main port. We implement the stop and wait algorithm to ensure the reliable delivery of 
the messages. Timeouts are used to ensure that packets are retransmitted in case an ack is lost. Our client 
can also write to the server which is the bonus part of this assignment. There are two modes that are supported
by the TFTP: net ASCII and Octet. 16 bit sequence numbers are used to break a large file into smaller chunks 
and transfer independently. As mentioned earlier TFTP uses the UDP to communicate, UDP is a connectionless unreliable protocol.