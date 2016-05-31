# INFORME LAB 3

En este lab, implementamos **ARP** (Address Resolution Protocol). Para ello creamos dos estructuras nuevas, una única funcion nueva y un nuevo arreglo *ARP_Table*

## Estructuras

### arphdr

Definimos la estructura **arphdr** (ARP hardware), que define un paquete ARP:

```c++
struct arphdr {
    u_int16_t hdw_type;                  /* Hardware Type           */
    u_int16_t protocol_type;             /* Protocol Type           */
    u_char hdw_len;                      /* Hardware Address Length */
    u_char protocol_len;                 /* Protocol Address Length */
    u_int16_t opcode;                    /* Operation Code          */
    u_char sender_hdw_addr[HW_ADDR_LEN]; /* Sender hardware address */
    u_char sender_ip_addr[IP_ADDR_LEN];  /* Sender IP address       */
    u_char target_hdw_addr[HW_ADDR_LEN]; /* Target hardware address */
    u_char target_ip_addr[IP_ADDR_LEN];  /* Target IP address       */
}__attribute__ ((__packed__));
```
Esta estructura esta especificada en el **RFC 826**.

### eth_frame

Definimos la estructura **eth_frame** (Ethernet Frame), que define un paquete Ethernet:

```c++
struct eth_frame {
    u_char MACAddress_target[HW_ADDR_LEN]; /* Target hardware address */
    u_char MACAddress_sender[HW_ADDR_LEN]; /* Sender harware address */
    u_int16_t ptype;                       /* Protocol Type */
    char payload[MAX_PAYLOAD_SIZE];        /* Payload */
}__attribute__ ((__packed__));
```

Esta estructura está definida por nosotros.

En dicha estructura, *Payload* es donde se guarda la carga útil del paquete Ethernet, que puede variar ampliamente, desde un paquete IP a un paquete FTP, entre otros.

### ARP_Table

La estructura donde guardamos todas las **MAC Address** conocidas. Es de *256x6*, ya que, podemos tener 255 vecinos, y cada uno tiene una dirección física de *6 bytes*:

```c++
unsigned char ARP_table[256][6];
```

## Funciones

### Principales

#### send_to_ip

De manera resumida, la tarea de esta aplicación es enviar un paquete **IP** a traves de la red.

Para esto la función tiene un gran *if* con dos casos:

* conocemos la **MAC** de destino:

  esto significa que tenemos la capacidad de crear el **paquete Ethernet**, ya que este nos pide la **MAC**, y enviarlo.

* no conocemos la **MAC** de destino:

  la solución es implementar **ARP** (Address Resolution Protocol). Esto significa que tenemos que generar un nuevo paquete, de tipo **arphdr**, el cual contiene una serie de datos, siendo los más importantes:

  * sender_hdw_addr
  * sender_ip_addr
  * target_hdw_addr
  * target_ip_addr

  Estos contienen las **IPs** y **MACs**, de la fuente y destino.

  Ahora, para rellenar el campo `target_hwd_addr`, obviamente no podemos poner la **MAC** de la computadora que nos interesa, si no el problema no existiría. Entonces lo seteamos en `broadcast`, lo cual lo enviará a todos los usuarios de la red.

#### receive_ethernet_packet

Recibe y maneja un paquete, **diferenciando entre ARP y paquetes regulares**.

Cuando recibimos un paquete regular, en el caso del lab de tipo IP, basta con llamar la función `receive_ip_packet` con un parametro `eth_packet.payload`.

El caso **ARP** es más complicado. En este tenemos que actualizar nuestra tabla y luego chequear si el paquete va dirigido a nosotros, ya que debido a que hacemos inundación, podría ser para cualquiera en la red.

En caso de que sea para nosotros nuestro deber es responderle con nuestra dirección **MAC**, de esa forma la otra computadora podrá comenzar a comunicarse con nosotros, mandando datos verdaderamente útiles.

### Auxiliares

#### is_my_mac

Dado un *eth_frame*, chequea si su **MAC** de destino es la nuestra, devolviendo un *true* o *false*:

```c++
bool Node::is_my_mac(eth_frame packet) {
    u_char reciever_mac[HW_ADDR_LEN];
    get_my_mac_address(reciever_mac);
    bool result = true;
    for (unsigned int i = 0; i < HW_ADDR_LEN; ++i) {
        result = result && reciever_mac[i] == packet.MACAddress_target[i];
    }
    return result;
}
```

## Decisiones de diseño tomadas

Para este laboratorio el número de decisiones de diseño que podríamos haber tomado resultó acotado, ya que restringimos la implementación a la seguir exactamente la especificación de RFC 826 sobre definición, envío y recepción de paquetes ARP. Con esto presente, las decisiones de diseño que podríamos mencionar son detalles mínimos, como cambiar los nombres de los miembros para las estructuras del paquete ARP *arphdr* y el *eth_frame*, los cuales en su especificación formal solamente estan representados por las siglas de lo que representan, mientras que en nuestra especificación los indicamos en su mayoria con su nombre entero, por cuestiones de legibilidad.

Otra decisión de diseño que podemos mencionar, es la definición de la función *is_my_mac()*, que chequea si la dirección MAC indicada en el paquete como destinatario es la misma del nodo que lo recibe, como indicado en la sección anterior.

Finalmente otra desición de diseño que finalmente no incluímos en nuestra implementación, aunque lo consideramos como una posible modularización, fue crear una función separada de las dos principales *send_to_ip()* y *receive_ethernet_packet()* que se encargue de crear un paquete ARP, completarlo con los parámetros dados y enviarlo a la dirección especificada, evitando de esta manera tener que hacerlo en ambas funciones. Algo de la forma:

```c++
int send_ARP(u_int16_t operation, u_char target_hw_addr, 
                     u_int16_t target_ip_addr);
```

De esta manera el programa podría escribirse de la siguiente forma:

De esta manera tendriamos algo del estilo:

```c++
int Node::send_to_ip(IPAddress ip, void *data) {
    // Caso donde se conoce la MAC de destino
    if(La dirección MAC asociada al parámetro ip está en la tabla ARP)
      crear un eth_frame, llenarlo y enviarlo;
      return 0;

    // Caso donde no se conoce la MAC de destino
    else {
       send_ARP(u_int16_t ARP_REQUEST, u_char target_hw_addr, 
                       u_int16_t target_ip_addr);
       return 1;
    }
}

void Node::receive_ethernet_packet(void *packet) {
    // Chequeo primero si es un paquete ARP o un paquete Ethernet
    if (!isItArp(packet)) {
       Procesamos el paquete como un paquete de tipo Ethernet;
    }

    else {
    // Paquete ARP
    Fijarse si el paquete ARP corresponde a la IP propia;
        Si no corresponde, descartarlo;
        Si corresponde {
             Fijarse si la MAC del emisor esta en la tabla
                '----> hacer lo que corresponda si no esta
             send_ARP(u_int16_t ARP_REPLY, u_char target_hw_addr, 
                             u_int16_t target_ip_addr);
        }

}
```

De todas formas, esta posibilidad la descartamos por practicidad y mantuvimos la especificación dada por la documentación oficial.

## Dificultades con las que nos encontramos

La primera dificultad con la que nos encontramos fue comprender de que manera un host, al recibir un *eth_frame*, se puede enterar de que tipo es lo que contiene en su *payload* para saber en que formato tomarlo.

Otras dificultades surgieron antes de que el programa sea funcional, aunque en esta instancia ya teniamos las estructuras y las funciones definidas, y estas fueron mayoritariamente encontrar pequeños *bugs* como verificar si el *broadcast* del paquete ARP se estaba realizando correctamente, aplicar las funciones que importamos de *arpa/inet.h* para solucionar las diferencias del orden de bits de la capa de acceso al medio con la que se utiliza en la de Internet.


## Cómo las resolvimos

En general todas las dudas fueron integramente conceptuales, y fueron fácilmente resueltas con dar una releida a la documentación oficial y eventualmente a distintas fuentes para resolver aspectos de implementación relacionados con C++ en particular.

La primera dificultad la resolvimos al comprender que un paquete ARP se recibe en el *payload* dentro de un *eth_frame*, y éstos tienen definida en su estructura el tipo de protocolo que rige para el contenido del *payload*:

```c++
struct eth_frame {
    [...]
    u_int16_t ptype;
    [...]
}__attribute__ ((__packed__));
```
De manera que basta con chequear ese campo para decidir seguidamente qué se debe hacer con el contenido en *payload* antes de intentar castearlo en algun formato dado.


