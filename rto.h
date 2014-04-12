#define RTT_RXTMIN 1000
#define RTT_RXTMAX 3000
#define MAX_RTO_COUNT 12

#define INITIAL_SRTT 0
#define INITIAL_RTTVAR RTT_RXTMIN

int srtt8 = INITIAL_SRTT * 8;
int rttvar4 = INITIAL_RTTVAR * 4;
int rto = INITIAL_RTTVAR;

int iabs(int a) {
  return a >= 0 ? a : -a;
}

int imin(int a, int b) {
  return a <= b ? a : b;
}

int imax(int a, int b) {
  return a >= b ? a : b;
}

int get_rto() {
  int val = rto;
  val = imax(val, RTT_RXTMIN);
  val = imin(val, RTT_RXTMAX);
  return val;
}

int rto_update(int rtt) {
  srtt8 = srtt8 + rtt - (srtt8 >> 3);
  rttvar4 = rttvar4 + iabs(rtt) - (rttvar4 >> 2);
  rto = (srtt8 >> 3) + rttvar4;
  return rto;
}
