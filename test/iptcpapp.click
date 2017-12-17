

tcp1 :: tcpEntity(MY_PORT 111, TYPE 1, TIME_OUT 2, MY_IP 1.2.3.4); //server
tcp2 :: tcpEntity(MY_PORT 222, TYPE 0, TIME_OUT 2, MY_IP 1.2.3.5); //client

ip1 :: IPElement(MY_ADDRESS 1.2.3.4, PERIOD_HELLO 1, PERIOD_EXCHANGE 5);
ip2 :: IPElement(MY_ADDRESS 1.2.3.5, PERIOD_HELLO 1, PERIOD_EXCHANGE 5);

sv1 :: Server(TYPE 1,TEXT "log.txt",DEST_IP 1.2.3.5, DEST_PORT 222);
cl1 :: Client(TYPE 1);

sv1->[1]tcp1[0]->Print("after tcp")->[0]ip1[1]->Print("after ip")->[1]ip2[0]->[0]tcp2[1]->Print(1)->cl1;
Idle()->[1]tcp2[0]->[0]ip2[1]->[1]ip1[0]->Print("before server")->[0]tcp1[1]->Print(2)->Discard();
