
Goal: stimulator simulates a shaft encoder

PULSES_PER_REV = 10k
MAX_REVS_SEC   = 100   # 6000 rpm, seems feasible
MAX PULSE_PER_SEC = 1M

We define a series of moves:


+10.000 revs
-5.5555 revs 

A move starts at zero revs and ends at zero revs
Shaft is assumed to accelerate to max speed
Acceleration: reaches 100 RPS in 1s (ie 100R/s2)

The system starts at START.
The INDEX pulse outputs every time the shaft passes START.
Fault pulses may be injected at any time

Move(startpos, revs)

s = ut + 1/2.aT12
s = vT2
s = v2 - 1/2.aT3a

s_accel - at

s_accel_max = 100

if S <= 100
   t = S/100   speedup: t/2, slowdown t/2
else
   tup = 1s (Sup=50)
   tdn = 1s (Sdn50
   tflat = (S-100)/100

call move1(tup)
call move2(tflat)
call move3(tdn)

Inside move: 
generate a set of data sampled at 1MSPS
in each 1msec slice, generate N pulses, keep tabs on the total so as to gen the Index

Fault is preprogrammed as a list of transition times, insert at the appropriate slice 

Results:

2026  ./anstostim 
 2027  ls -l
 2028  hexdump -ve '1/1 "%02x\n"' raw.dat | grep -n . | more
 2029  hexdump -ve '1/1 "%02x\n"' dio4.dat | grep -n . | more
 2030  history
 2031  hexdump -ve '1/1 "%08x\n"' dio4.dat | grep -n . | more
 2032  hexdump -ve '1/4 "%08x\n"' dio4.dat | grep -n . | more

Scott: is the packing correct?

pgm@peter-XPS-13-7390:~/PROJECTS/ANSTOSTIM$ hexdump -ve '1/1 "%02x\n"' dio4.dat | grep -n . | more
1:11
2:33
3:33
4:20
5:00
6:11
7:33
8:33
9:20
10:00
11:11
12:33
13:33
14:20


But what we really load is:
pgm@peter-XPS-13-7390:~/PROJECTS/ANSTOSTIM$ hexdump -ve '1/4 "%08x\n"' dio4.dat | grep -n . | head
1:20333311
2:33331100
3:33110020
4:11002033
5:00203333
6:20333311
7:33331100
8:33110020
9:11002033




