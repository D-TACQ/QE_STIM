#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <math.h>
#include <algorithm>
#include <cstring>

#include <assert.h>     /* assert */


/* PULSES are position */
/* TICKS are samples clocks, constant 10MHz */

#define PPR		10000	// PULSES PER REV
#define MAX_RPS		100		// MAX revs per sec
#define MAX_PPS     (PPR*MAX_RPS)

#define ACC 		50     // Revs Per Sec Per Sec

#define ACCP		(ACC*PPR)	// Pulses per sec per sec
#define ACCP_MS		(ACCP/1000) // Pulses per sec per msec

#define T_RAMP      (MAX_RPS/ACC)
#define S_RAMP      ((ACC*T_RAMP*T_RAMP)/2)

#define TPS			10000000	// Ticks Per Second minimum unit of time 10MHz


int getenv_default(const char* key, int def = 0){
	const char* vs = getenv(key);
	if (vs){
		return atoi(vs);
	}else{
		return def;
	}
}


class Move {
public:
	static long seq_start;
	static std::vector<Move> moves;
	const bool forwards;
	const unsigned long distance;
	const long start;

	static unsigned long total_distance;

	Move(bool _fwds, unsigned long _dist, unsigned long _start):
		forwards(_fwds), distance(_dist), start(_start) {}

	static bool input(std::string& line){
		char dir;
		unsigned long dist;
		const char* def = line.c_str();
		if (sscanf(def, "%c%lu", &dir, &dist) == 2){
			switch(dir){
			case '+':
				moves.emplace_back(Move(true, dist, seq_start));
				seq_start += dist;
				total_distance += dist;
				return true;
			case '-':
				moves.emplace_back(Move(false, dist, seq_start));
				seq_start -= dist;
				total_distance += dist;
				return true;
			default:
				;
			}
		}
		return false;
	}

	void print() const {
		printf("Move: %10ld: %5lu %s\n", start, distance, forwards? "Forward": "Reverse");
	}
};
std::vector<Move> Move::moves;
long Move::seq_start;
unsigned long Move::total_distance;

struct Trajectory {
	double uu;		// start speed RPS
	double vv; 		// end speed RPS
	double tt;        // duration
	static double ttotal;
	Trajectory(double _uu, double _vv, double _tt):
		uu(_uu), vv(_vv), tt(_tt) {
		ttotal += tt;
		printf("Trajectory tt:%.3f ttotal:%.3f\n", tt, ttotal);
	}
	void print() const {
		printf("Trajectory: uu:%.3f vv:%.3f tt:%.3f  => ss:%.3f\n", uu, vv, tt, (uu+vv)*tt/2);
	}
};

double Trajectory::ttotal;

class Motion {
	Motion(const Move& _move): move(_move) {
		printf("%s  distance > *S_RAMP ? %lu > %lu %s\n", __FUNCTION__, move.distance, 2*S_RAMP, move.distance>2*S_RAMP? "YES": "NO");

		if (move.distance > 2*S_RAMP){
			stages.emplace_back(Trajectory(0, MAX_RPS, T_RAMP));
			stages.emplace_back(Trajectory(MAX_RPS, MAX_RPS, (move.distance-2*S_RAMP)/MAX_RPS));
			stages.emplace_back(Trajectory(MAX_RPS, 0, T_RAMP));
		}else{
			double s_ramp = move.distance/2;
			double t_ramp = sqrt(2*s_ramp/ACC);  // s=ut +1/2 at*t
			double vmax = ACC*t_ramp;
			printf("%s distance:%lu s_ramp:%u t_ramp:%u vmax:%u\n", __FUNCTION__, move.distance, s_ramp, t_ramp, vmax);
			if (vmax > 0){
				stages.emplace_back(Trajectory(0, vmax, t_ramp));
				stages.emplace_back(Trajectory(vmax, 0, t_ramp));
			}else{
				printf("WARNING: Motion too small to register\n");
			}
		}
	}
public:
	std::vector<Trajectory> stages;
	const Move& move;

	void print() const {
		printf("Motion of Move: "); move.print();
		for (auto s: stages){
			printf("\t"); s.print();
		}
	}

	static std::vector<Motion> motions;
	static void add(const Move& move){
		motions.emplace_back(Motion(move));
	}
};

std::vector<Motion> Motion::motions;

class QuadEncoder {
	const int ABIT;
	const int BBIT;
	const int IBIT;
	const int EBIT;

	const int FWD[4] = { ABIT, ABIT|BBIT, BBIT, 0 };   // a 4 state LUT might be cleaner..
	const int BAK[4] = { BBIT, BBIT|ABIT, ABIT, 0 };

	unsigned long long tick;
	const unsigned long max_dio;
	unsigned char* dio;				// only 4 bits are valid;
	unsigned char* cursor;
	unsigned long line;
	unsigned istate;

	void make_pulses(unsigned ticks_per_pulse, unsigned pulses, bool forwards){
		if (verbose){
			printf("make_pulses cursor:%d tpp:%u pulses:%u direction:%s\n", cursor-dio, ticks_per_pulse, pulses, forwards?"F":"R");
		}
		assert (cursor-dio < max_dio*2-pulses*ticks_per_pulse);


		for (unsigned pulse = 0, index_stretch_countdown = 0; pulse < pulses; ++pulse, istate = ++istate&0x3){
			unsigned char yy = forwards? FWD[istate]: BAK[istate];

			if (++line > PPR){
				line = 0;
			}
			if (line == 0 && index_stretch >= 0){
				yy |= line == 0? IBIT: 0;

				if (index_stretch){
					index_stretch_countdown = index_stretch;
				}
			}
			if (index_stretch_countdown){
				yy |= IBIT;
				--index_stretch_countdown;
			}
			/* ERROR */
			if (!forwards && ebit_shows_reverse){
				yy |= EBIT;
			}

			for (int tick = ticks_per_pulse; tick--; ){
				*cursor++ = yy;
			}
		}
	}
	unsigned vx_pps(const Trajectory& tj, unsigned long tick){
		unsigned long maxticks = tj.tt * TPS;
		if (tj.vv > tj.uu){
			return PPR*tj.uu + PPR*(tj.vv-tj.uu)*tick/maxticks;
		}else{
			return PPR*tj.uu - PPR*(tj.uu-tj.vv)*tick/maxticks;
		}
	}



	void ramp(const Trajectory& tj, bool forwards){
		unsigned long max_ticks = tj.tt * TPS;
		const unsigned long THRESHOLD = max_ticks/10;
		unsigned long limit = 1;
		unsigned long tick = 0;

		if (verbose){
			printf("ramp: THRESHOLD:%lu\n", THRESHOLD);
		}
		for (unsigned long ticks_per_pulse = 1; tick < max_ticks;  tick += ticks_per_pulse*limit){
			if (vx_pps(tj, tick) < 1){
				;//*cursor++ = 0;
			}else{
				//printf("ramp ticks:%lu\n", tick);
				ticks_per_pulse = TPS / vx_pps(tj, tick);

				if (verbose){
					printf("ramp: tick:%d ticks_per_pulse:%d\n", tick, ticks_per_pulse);
				}
				if (ticks_per_pulse > THRESHOLD){
					limit = 1;
					ticks_per_pulse = THRESHOLD;
				}else{
					//limit = std::min(ticks_per_pulse, max_ticks-tick);
					limit = std::min(max_ticks-tick, 10000UL);
				}

				make_pulses(ticks_per_pulse, limit, forwards);
			}
		}
		if (verbose){
			printf("ramp finished at %d ticks\n", tick);
		}
	}
	void speed_up(const Trajectory& tj, bool forwards) {
		printf("speed_up\n");
		ramp(tj, forwards);
	}
	void full_ahead(const Trajectory& tj, bool forwards){
		const unsigned ticks_per_pulse = TPS / (tj.uu * PPR);
		const unsigned states = tj.tt * TPS;
		printf("full_ahead ticks_per_pulse:%u %s\n", ticks_per_pulse, forwards? "F": "R");
		make_pulses(ticks_per_pulse, states, forwards);
	}
	void slow_down(const Trajectory& tj, bool forwards){
		printf("slow down\n");
		ramp(tj, forwards);
	}
	static int verbose;
public:
	QuadEncoder(unsigned _line=0): ABIT(1<<0), BBIT(1<<1), IBIT(1<<2), EBIT(1<<3), tick(0), max_dio(Trajectory::ttotal*TPS), line(_line), istate(0) {
		dio = new unsigned char[max_dio*2];
		cursor = dio;
		printf("QuadEncoder: tt:%u s TPS:%u  ticks:%u alloc:%d\n",
				Trajectory::ttotal, TPS, max_dio, max_dio*2);
	}
	~QuadEncoder() {
		FILE* fp = fopen("dio.dat", "w");
		fwrite(dio, 1, cursor-dio, fp);
		fclose(fp);
		delete [] dio;
	}

	void operator() (const Motion& motion) {
		printf("QuadEncoder:"); motion.print();
		for (const Trajectory tj: motion.stages){
			if (tj.uu < tj.vv){
				speed_up(tj, motion.move.forwards);
			}else if (tj.uu > tj.vv){
				slow_down(tj, motion.move.forwards);
			}else{
				full_ahead(tj, motion.move.forwards);
			}
		}
	}

	void dump_byte_per_bit(const char* fname) {
		const unsigned raw_len = (cursor-dio)/2;
		auto bytes = new char [raw_len][4];

		memset(bytes, 0, raw_len*4);

		for (int ii = 0; ii < raw_len; ++ii){
			char yy = dio[ii];
			if (yy & ABIT) bytes[ii][0] = 1;
			if (yy & BBIT) bytes[ii][1] = 1;
			if (yy & IBIT) bytes[ii][2] = 1;
			if (yy & EBIT) bytes[ii][3] = 1;
		}
		FILE* fp = fopen(fname, "w");
		fwrite(bytes, 4, raw_len, fp);
		fclose(fp);
	}
	void compress(const char* fname){
		unsigned raw_len = (cursor-dio)/2;
		unsigned char* raw = new unsigned char[raw_len];
		for (int iraw = 0; iraw < raw_len; ++iraw){
			raw[iraw] = dio[iraw*2] << 4 | dio[iraw*2+1];
		}
		FILE* fp = fopen(fname, "w");
		fwrite(raw, 1, raw_len, fp);
		fclose(fp);
	}
	static int index_stretch;
	static int ebit_shows_reverse;
};

int QuadEncoder::verbose = getenv_default("QUAD_ENCODER_VERBOSE");
int QuadEncoder::index_stretch;
int QuadEncoder::ebit_shows_reverse;

int main(int argc, const char* argv[]){
	std::string config_file = "anstostim.cfg";
	if (argc > 1) config_file = argv[1];
	std::ifstream input_file(config_file);
	int start_line;
	if (!input_file.is_open()) {
		std::cerr << "Could not open the file - '"
			 << config_file << "'" << std::endl;
		return EXIT_FAILURE;
	}

	std::string fname_ext;

	std::string line;
	while (std::getline(input_file, line)){
		if (sscanf(line.c_str(), "STARTLINE=%d", &start_line) == 1 ||
		    sscanf(line.c_str(), "INDEX_STRETCH=%d", &QuadEncoder::index_stretch) == 1 ||
		    sscanf(line.c_str(), "EBIT_SHOWS_REVERSE=%u", &QuadEncoder::ebit_shows_reverse) == 1){
			fname_ext += "_" + line;
			continue;
		}
		Move::input(line);
	}
	input_file.close();

	for (const Move& m : Move::moves){
		m.print();
		Motion::add(m);
	}
	printf("Total distance: %lu\n", Move::total_distance);
	QuadEncoder qe(start_line);
	for (const Motion& m : Motion::motions){
		qe(m);
	}
	std::string fnamed = config_file + "-c4" + fname_ext;
	printf("Dump file: %s\n", fnamed.c_str());
	qe.dump_byte_per_bit(fnamed.c_str());

	std::string fname = config_file + "-dio4" + fname_ext;
	printf("Compress file: %s\n", fname.c_str());
	qe.compress(fname.c_str());
}
