#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <math.h>
#include <algorithm>


/* PULSES are position */
/* TICKS are samples clocks, constant 10MHz */

#define PPR			10000	// PULSES PER REV
#define MAX_RPS		100		// MAX revs per sec
#define MAX_PPS     (PPR*MAX_RPS)

#define ACC 		50     // Revs Per Sec Per Sec

#define ACCP		(ACC*PPR)	// Pulses per sec per sec
#define ACCP_MS		(ACCP/1000) // Pulses per sec per msec

#define T_RAMP      (MAX_RPS/ACC)
#define S_RAMP      ((ACC*T_RAMP*T_RAMP)/2)

#define TPS			10000000	// Ticks Per Second minimum unit of time 10MHz

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
	unsigned uu;		// start speed RPS
	unsigned vv; 		// end speed RPS
	unsigned tt;        // duration
	static unsigned ttotal;
	Trajectory(unsigned _uu, unsigned _vv, unsigned _tt):
		uu(_uu), vv(_vv), tt(_tt) {
		ttotal += tt;
		//printf("Trajectory tt:%u ttotal:%u\n", tt, ttotal);
	}
	void print() const {
		printf("Trajectory: uu:%3u vv:%3u tt:%3u  => ss:%u\n", uu, vv, tt, (uu+vv)*tt/2);
	}
};

unsigned Trajectory::ttotal;

class Motion {

	Motion(const Move& _move): move(_move) {
		if (move.distance > 2*S_RAMP){
			stages.emplace_back(Trajectory(0, MAX_RPS, T_RAMP));
			stages.emplace_back(Trajectory(MAX_RPS, MAX_RPS, (move.distance-2*S_RAMP)/MAX_RPS));
			stages.emplace_back(Trajectory(MAX_RPS, 0, T_RAMP));
		}else{
			unsigned s_ramp = move.distance/2;
			unsigned t_ramp = sqrt(2*s_ramp/ACC);  // s=ut +1/2 at*t
			unsigned vmax = ACC*t_ramp;
			stages.emplace_back(Trajectory(0, vmax, t_ramp));
			stages.emplace_back(Trajectory(vmax, 0, t_ramp));
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

	unsigned long long tick;
	unsigned char* dio;				// only 4 bits are valid;
	unsigned char* cursor;
	unsigned long line;

	void make_pulses(unsigned ticks_per_pulse, unsigned states, bool forwards){
		const unsigned f50pc = ticks_per_pulse/2;
		const unsigned f25pc = ticks_per_pulse/4;
		const unsigned f75pc = 3*f25pc;

		//printf("make_pulses tpp:%u states:%u %s\n", ticks_per_pulse, states, forwards?"F":"R");
		for (unsigned state = 0; state < states; ++state, ++cursor){
			unsigned char yy = 0;
			unsigned sfrac = state%ticks_per_pulse;

			if (state && sfrac == 0){
				++line;
				if (line > PPR){
					line = 0;
				}
			}
			yy |= line == 0? IBIT: 0;

			if (sfrac <= f50pc){
				yy |= (forwards? ABIT: BBIT);
			}
			if (sfrac >= f25pc && sfrac <= f75pc){
				yy |= (forwards? BBIT: ABIT);
			}
			*cursor = yy;
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
		const unsigned long max_ticks = tj.tt * TPS;
		unsigned ticks_per_pulse;
		for (unsigned long tick = 0; tick < max_ticks;  tick += ticks_per_pulse){
			if (vx_pps(tj, tick) < 10 /*== 0*/ ){
				*cursor++ = 0;
				ticks_per_pulse = 1;
			}else{
				//printf("ramp ticks:%lu\n", tick);
				ticks_per_pulse = TPS / vx_pps(tj, tick);
				int limit = ticks_per_pulse;
				if (limit > max_ticks-tick) limit = max_ticks-tick;
				make_pulses(ticks_per_pulse, limit, forwards);
			}
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
public:
	QuadEncoder(unsigned _line=0): ABIT(1<<0), BBIT(1<<1), IBIT(1<<2), EBIT(1<<3), tick(0), line(_line) {
		dio = new unsigned char[Trajectory::ttotal*TPS];
		cursor = dio;
		printf("QuadEncoder: tt:%u s TPS:%u  states:%u\n",
				Trajectory::ttotal, TPS, Trajectory::ttotal*TPS);
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
};

int main(int argc, const char* argv[]){
	printf("anstostim\n");
	std::ifstream input_file("anstostim.dat");
	unsigned start_line;
	if (!input_file.is_open()) {
		std::cerr << "Could not open the file - '"
			 << "anstostim.dat" << "'" << std::endl;
		return EXIT_FAILURE;
	}

	std::string line;
	while (std::getline(input_file, line)){
		if (sscanf(line.c_str(), "STARTLINE=%u", &start_line) == 1){
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
	qe.compress("dio4.dat");
}
