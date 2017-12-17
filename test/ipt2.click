require(library /home/comnetsii/elements/routerport.click);
rp1 :: RouterPort(DEV veth1, IN_MAC 76:49:97:6c:10:82 , OUT_MAC ce:7a:7b:4e:b5:b9 );
rp2 :: RouterPort(DEV veth2, IN_MAC ce:7a:7b:4e:b5:b9 , OUT_MAC 76:49:97:6c:10:82 );


ip1 :: IPElement(MY_ADDRESS 1.2.3.4,PERIOD_HELLO 1, PERIOD_EXCHANGE 5);
ip2 :: IPElement(MY_ADDRESS 3.4.5.6,PERIOD_HELLO 1,PERIOD_EXCHANGE 5);
ip3 :: IPElement(MY_ADDRESS 5.6.7.8,PERIOD_HELLO 1,PERIOD_EXCHANGE 5);
ip4 :: IPElement(MY_ADDRESS 7.8.9.10,PERIOD_HELLO 1,PERIOD_EXCHANGE 5);
ip5 :: IPElement(MY_ADDRESS 9.10.11.12,PERIOD_HELLO 1,PERIOD_EXCHANGE 5);


Idle()->[0]ip1[1]->[1]ip2[2]->rp1;
rp2->[2]ip3[1]->[1]ip4[0]->Discard();
Idle()->[0]ip4[1]->[1]ip3[2]->rp2;
rp1->[2]ip2[1]->[1]ip1[0]->Discard();

Idle()->[3]ip1[2]->[1]ip5[2]->[2]ip4[3]->Discard();
Idle()->[3]ip4[2]->[2]ip5[1]->[2]ip1[3]->Discard();



Idle()->[0]ip2[0]->Discard();
Idle()->[0]ip3[0]->Discard();
Idle()->[0]ip5[0]->Discard();

