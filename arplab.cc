#include "node.h"
#include <arpa/inet.h>
#include <stdbool.h>

typedef unsigned char u_char;

#define PROTO_IP  0x0800
#define PROTO_ARP 0x0806
#define ARP_REQUEST 0x01
#define ARP_REPLY   0x02
#define ETHERNET 0x0001
#define HW_ADDR_LEN 6
#define IP_ADDR_LEN 4
#define MAX_PAYLOAD_SIZE 1500

Define_Module(Node);

MACAddress broadcast = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
struct arphdr {
    u_int16_t hdw_type;                   /* Hardware Type           */
    u_int16_t protocol_type;              /* Protocol Type           */
    u_char hdw_len;                       /* Hardware Address Length */
    u_char protocol_len;                  /* Protocol Address Length */
    u_int16_t opcode;                     /* Operation Code          */
    MACAddress sender_hdw_addr;           /* Sender hardware address */
    IPAddress sender_ip_addr;             /* Sender IP address       */
    MACAddress target_hdw_addr;           /* Target hardware address */
    IPAddress target_ip_addr;             /* Target IP address       */
}__attribute__ ((__packed__));

struct eth_frame {
    MACAddress target;
    MACAddress sender;
    u_int16_t ptype;
    char payload[MAX_PAYLOAD_SIZE];
}__attribute__ ((__packed__));


int Node::send_to_ip(IPAddress ip, void *data) {
    
    struct eth_frame eth_packet;
    if(ARP_table[ip[3]][0] != '\0') {
        // Caso donde se conoce la MAC de destino
        memcpy(eth_packet.target, ARP_table[ip[3]], sizeof(MACAddress));
        memcpy(eth_packet.payload, data, MAX_PAYLOAD_SIZE);
        get_my_mac_address(eth_packet.sender);
        eth_packet.ptype = htons(PROTO_IP);

        send_ethernet_packet(&eth_packet);
        return 0;
    }
    else {
        // Caso donde no se conoce la MAC de destino

        struct arphdr arp_packet;
        IPAddress sender_ip;
        get_my_ip_address(sender_ip);
        MACAddress MAC_source;
        get_my_mac_address(MAC_source);

        /* Comenzamos a armar el paquete ARP */
        arp_packet.hdw_type = htons(ETHERNET);
        arp_packet.protocol_type = htons(PROTO_IP);
        arp_packet.hdw_len = sizeof(MACAddress);
        arp_packet.protocol_len = sizeof(IPAddress);
        arp_packet.opcode = htons(ARP_REQUEST);

        /* Se setea la MAC e IP del origen y la IP del destino */
        get_my_mac_address(arp_packet.sender_hdw_addr);
        get_my_ip_address(arp_packet.sender_ip_addr);
        memcpy(arp_packet.target_ip_addr, ip, sizeof(IPAddress));

        /* En la MAC de destino establecemos que sea de tipo broadcast */
        memcpy(arp_packet.target_hdw_addr, broadcast, sizeof(MACAddress));

        /* Envolvemos el paquete ARP en un paquete ethernet */
        eth_packet.ptype = htons(PROTO_ARP);
        get_my_mac_address(eth_packet.sender);
        memcpy(eth_packet.target, broadcast, sizeof(MACAddress));
        memcpy(eth_packet.payload, &arp_packet, sizeof(struct arphdr));

        /* Enviamos el paquete */
        send_ethernet_packet(&eth_packet);
        return 1;
    }
}


void Node::receive_ethernet_packet(void *packet) {
    // Chequeo primero si es un paquete arp o un paquete eth
    eth_frame eth_packet;
    memcpy(&eth_packet, packet, sizeof(struct eth_frame));
    if(eth_packet.ptype == htons(PROTO_IP)) {
        // Paquete ethernet
        // Me fijo si la IP de destino es mi maquina
        if (is_my_mac(eth_packet)) {
            receive_ip_packet(eth_packet.payload); // Paso el paquete ip a la capa de red.
        }
    }
    else {
        IPAddress reciever_ip;
        get_my_ip_address(reciever_ip);

        // Paquete ARP
        bool Merge_flag;
        struct arphdr arp_packet;
        memcpy(&arp_packet, eth_packet.payload, sizeof(struct arphdr));
        /* ?Do I have the hardware type in ar$hrd? */
        if (arp_packet.hdw_type == htons(ETHERNET)) {

            /* ?Do I speak the protocol in ar$pro? */
            if (arp_packet.protocol_type == htons(PROTO_IP)) {

                /* If the pair <protocol type, sender protocol address> is already in my
                 * translation table, update the sender hardware address field of the entry
                 * with the new information in the packet and set Merge_flag to true.
                 */
                Merge_flag = false;
                IPAddress sender_ip;
                memcpy(sender_ip, arp_packet.sender_ip_addr, sizeof(IP_ADDR_LEN));
                if (ARP_table[sender_ip[3]][0] != '\0') {
                    memcpy(ARP_table[sender_ip[3]], eth_packet.sender, sizeof(MACAddress));
                    Merge_flag = true;
                }

                /* ?Am I the target protocol address? */
                if (reciever_ip[3] == arp_packet.target_ip_addr[3]) {

                    /* If Merge_flag is false,
                     * add the triplet <protocol type,sender protocol address,
                     * sender hardware address> to the translation table.*/
                    if (!Merge_flag) {
                        memcpy(ARP_table[sender_ip[3]], eth_packet.sender, sizeof(MACAddress));
                    }

                    /* ?Is the opcode ares_op$REQUEST? */
                    if (arp_packet.opcode == htons(ARP_REQUEST)) {

                        /* Swap hardware and protocol fields, putting the local
                                hardware and protocol addresses in the sender fields.
                                Set the ar$op field to ares_op$REPLY
                                Send the packet to the (new) target hardware address on
                                the same hardware on which the request was received.*/
                        
                        /* Seteamos el opcode en reply*/
                        arp_packet.opcode = htons(ARP_REPLY);

                        MACAddress sender_mac;
                        memcpy(sender_mac, eth_packet.sender, sizeof(MACAddress));

                        /* Intercambiamos las direcciones de hdw e ip */
                        memcpy(arp_packet.target_ip_addr, sender_ip, sizeof(IPAddress));
                        memcpy(arp_packet.target_hdw_addr, sender_mac, sizeof(MACAddress));
                        get_my_mac_address(arp_packet.sender_hdw_addr);
                        get_my_ip_address(arp_packet.sender_ip_addr);

                        
                        /* Terminamos de envolver el paquete */
                        memcpy(eth_packet.payload, &arp_packet, sizeof(struct arphdr));
                        memcpy(eth_packet.target, sender_mac, sizeof(MACAddress));
                        get_my_mac_address(eth_packet.sender);
                        send_ethernet_packet(&eth_packet);
                    }
                }
            }
        }
    }
}

bool Node::is_my_mac(eth_frame packet) {
    MACAddress reciever_mac;
    get_my_mac_address(reciever_mac);
    bool result = true;
    for (unsigned int i = 0; i < HW_ADDR_LEN; ++i) {
        result = result && reciever_mac[i] == packet.target[i];
    }
    return result;
}

Node::Node()
{
    timer = NULL;
    for (unsigned int i = 0; i != AMOUNT_OF_CLIENTS; ++i) {
        seen[i] = 0;
    }
    
    for (unsigned int i = 0; i < 256; ++i) {
        for (unsigned int j = 0; j < 6; ++j) {
            ARP_table[i][j] = '\0';
        }
    }
}
