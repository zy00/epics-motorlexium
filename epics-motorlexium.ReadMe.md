# IMS Lexium/Liberty Mdrive Guide

## Quick Start
Follow the following steps to quickly get the lexium mdrive to run under EPICS ioc control. For details, see the related sections below.
* Wire up 24V power to the mdrive unit
* Configure IPaddress: default ip is 192.168.33.1, configure a laptop with network in this subnet, and connect with LMD software suite, change IP address to the desired one. 
* Configure software settings for EPICS driver: issue the following commands in the LMD software suite command window
  ```
  EM=2 # disable echo
  AS=2 # enable variable hMT tech
  AT=3 # enable s-curve; 1=linear(default); 3=s-curve
  IS=2,2,1 # input 2 for + limit, 
  IS=3,3,1 # input 3 for - limit
  (if disable limit: IS=2,2,0; IS=3,3,0)
  S # save the settings
  ```
* Limit Hareware wire up (dry-contact switches)
  Input Ref (Pin 1a)=> V+ (24v)  (reference)
  Input 2 (Pin 3a) +> Limit +
  Input 3 (Pin 4a) => Limit -
  Limit Common => V- (0v) 

* Repower mdrive. Mdrive motor is ready for EPICS IOC control 




## Details
### Configure IP address
The default mdrive ip address when shipped is 192.168.33.1. Note there is no way to reset IP, once changed, label the motor with ip addr.
* Install mdrive interface software LMD software suite (windows) downloaded from URL https://novantaims.com/dloads/user-interface-software/
* Launch windows software. chose adapter interface 192.168.33.x as configured, connect to 192.168.33.1 port 503, connect 
* Newones come with CSE mark, asking to set passwd. - set up your password, and save it somewhere.  
* Disable Security  (Edit->Cyber Security Disable). It disconnects to motor, "connect" again
* Click on Mac Address "Read"/ReadAll, you should see mac and ip address existed in the controller.
* now change to new ip address/subnet mask/gateway, click on "write" on each item
* Reset driver. Connect to new IP, and double-check new ip/mask/gateway settings

### Configure Communication settings:
* set echo mode to 2  # see Mcode manual 5-41
otherwise all commands would not respond
```
EM=2                  
PR EM       # to verify the change
```

### Configure Limits connection
* Hardware set up: at NSLS-II, we use: input2 = Limit+,  input3 = limit-

connections: P2a
pin 1a = input ref (connect to v+)
pin 3a = in2       (LIMIT PLUS)
pin 4a = in3       (LIMIT MINUS)
Limit common to v-
[Reason of not using IN1 is that NEMA17 (LMDxE42.IP20.pdf, pg49, 6.3.3) has Pin2a N/C]

* In windows program, set up limit inputs using ascii commands:
```
IS = 2, 2, 1
IS = 3, 3, 1
```

(this is activate Limit when open).

IS command syntax: 
```
IS input IOfunction InputActiveResponse

input = 1,2,3,4 etc
IOfunction: 0/generalpurpose; 1/Homing; 2/Limit+; 3/Limit-; 
```

driver will read "PR IS" response to determine which inputs are the limits

### For encoder feedback operations (LMDAE models)
to close loop inside the controller, using hMT technology, set

AS=1;
MRES is the openloop resolution; 51200cts/rev  (256*200, 256 is the microstepping factor)
Note that Encoder is 1000 lines /rev => 4000 cts/rev
But MRES is still using openloop resolution 51200 cts/rev

Or can use AS=2 (variable current)
DO NOT use AS=3, otherwise motor keeps running


Save to flash with command "S"

once ioc starts,  If disconnect motor/controller power, it takes about 20s to re-establish the connection - controller reboot. Asyn autoconnect works.  For "absolute encoder models - with capacitor to save position, it will recover to the old position; For non-absolute models, readback position is zero - in this case, it would be good to set autoconnect to no, and restart IOC manually.


### Running and  Holding Current

Running Current:  RC=25 (default, 25%), to change to 50%, using asyn ascii interface:
``` 
RC=50  
PR RC   
S
```

Holding Current:  HC=5 (default 5%), to change to 10%, using asyn ascii interface:
```
HC=10 
PR HC 
S
``````


============

### IS command: 
set input function:

 IS = <input#>, <type>, <active>

 Type:

0 GP        Typical usage: to trigger events within a program
1 Home      When active triggers the homing routine as defined by the homing variable (HM)
2 Limit plus (+)    Functions as specified by the limit variable (LM).
3 Limit minus (—)   Functions as specified by the limit variable (LM).
4 G0        Executes a program at address 1 upon activation.
5 Soft stop Stops motion with deceleration and halts program
6 Pause     Pause/resume program with motion.

When given "PR IS" command, typical reply
IS = 1, 0, 1
IS = 2, 2, 0
IS = 3, 3, 0
IS = 4, 0, 1
IS = 6, 0, 1

## IO Pinouts

P2a input plug has 7 pins:
1a: input reference
2a: IN1 (42 size not connected)
3a: IN2
4a: IN3
5a: IN4
6a: Analog In  - not avail on Absolute model
7a: Logic Gnd  - not avail on absolute model
