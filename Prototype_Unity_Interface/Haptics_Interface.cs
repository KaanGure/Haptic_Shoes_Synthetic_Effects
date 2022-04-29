using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.Net;
using System.Net.Sockets;
using System.Text;
public class Haptics_Interface : MonoBehaviour
{
    Socket sock = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
    string ip_addr = "127.0.0.1";
    int zone = -2;
 
    public void sendUDP(string message){
        IPAddress serverAddr = IPAddress.Parse(ip_addr);
        IPEndPoint endPoint = new IPEndPoint(serverAddr, 11000);
        byte[] send_buffer = Encoding.ASCII.GetBytes(message );
        sock.SendTo(send_buffer , endPoint);  
    }

    // Start is called before the first frame update
    void Start()
    {

    }

    // Update is called once per frame
    void Update()
    {
        Vector3 playerPosition = transform.position;
        int newZone = -1;
        if(playerPosition.x < 0){
            newZone = 0;
        } else{
            newZone = 1;
        }
        if(zone != newZone){
            zone = newZone;
            sendUDP(zone.ToString());
        }
    }
}
